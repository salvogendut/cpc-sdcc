#include "screen.h"
#include "ansi.h"

/*
 * ANSI / VT100 escape sequence parser.
 * Ported from n4cewenterm ansiterm.s (RASM) to C for SDCC.
 *
 * Supported sequences:
 *   ESC [ n A    CUU  cursor up
 *   ESC [ n B    CUD  cursor down
 *   ESC [ n C    CUF  cursor right
 *   ESC [ n D    CUB  cursor left
 *   ESC [ r ; c H/f  CUP/HVP  cursor position (1-based)
 *   ESC [ n J    ED   erase display (0=to end, 2=all)
 *   ESC [ n K    EL   erase line (0=to end, 2=whole)
 *   ESC [ ... m  SGR  select graphic rendition
 *   ESC [ s      SCP  save cursor
 *   ESC [ u      RCP  restore cursor
 *   ESC c        RIS  reset (clear screen)
 */

/* State machine */
#define S_IDLE    0
#define S_ESC     1  /* received ESC */
#define S_PARAMS  2  /* received ESC[ or 0x9B, accumulating params */
#define S_ESC2    3  /* ESC + intermediate byte (0x20-0x2F): skip one final byte */
#define S_OSC     4  /* ESC ]: skip OSC payload until BEL or ESC */
#define S_OSC_ESC 5  /* saw ESC inside OSC: next char decides end vs continue */

#define MAX_PARAMS 8

static unsigned char ansi_state;
static unsigned char params[MAX_PARAMS];
static unsigned char nparam;
static unsigned char have_digit;  /* nonzero when current param slot has a digit */

static unsigned char saved_col;
static unsigned char saved_row;

/* ANSI → CPC hardware ink mapping (0–7 standard, 8–15 bright) */
static const unsigned char ink_table[16] = {
    0,   /* 0 black */
    6,   /* 1 red */
    18,  /* 2 green */
    14,  /* 3 yellow */
    1,   /* 4 blue */
    8,   /* 5 magenta */
    20,  /* 6 cyan */
    24,  /* 7 white */
    7,   /* 8 dark gray */
    6,   /* 9 bright red */
    18,  /* 10 bright green */
    14,  /* 11 bright yellow */
    2,   /* 12 bright blue */
    8,   /* 13 bright magenta */
    20,  /* 14 bright cyan */
    24,  /* 15 bright white */
};

static void reset(void) {
    ansi_state = S_IDLE;
    nparam = 0;
    have_digit = 0;
}

/* Return param[idx], default `def` if not present or zero */
static unsigned char get_p(unsigned char idx, unsigned char def) {
    if (idx >= nparam) return def;
    if (params[idx] == 0) return def;
    return params[idx];
}

/* Clamp n to [lo,hi] */
static unsigned char clamp(unsigned char n, unsigned char lo, unsigned char hi) {
    if (n < lo) return lo;
    if (n > hi) return hi;
    return n;
}

/* -------------------------------------------------------------------------
 * ANSI command handlers
 * -------------------------------------------------------------------------*/

static void do_cuu(void) {     /* cursor up */
    unsigned char n = get_p(0, 1);
    cursor_row = (cursor_row >= n) ? cursor_row - n : 0;
}

static void do_cud(void) {     /* cursor down */
    unsigned char n = get_p(0, 1);
    cursor_row += n;
    if (cursor_row >= SCREEN_ROWS) cursor_row = SCREEN_ROWS - 1;
}

static void do_cuf(void) {     /* cursor right */
    unsigned char n = get_p(0, 1);
    cursor_col += n;
    if (cursor_col >= SCREEN_COLS) cursor_col = SCREEN_COLS - 1;
}

static void do_cub(void) {     /* cursor left */
    unsigned char n = get_p(0, 1);
    cursor_col = (cursor_col >= n) ? cursor_col - n : 0;
}

static void do_cup(void) {     /* cursor position (1-based) */
    unsigned char r = clamp(get_p(0, 1), 1, SCREEN_ROWS);
    unsigned char c = clamp(get_p(1, 1), 1, SCREEN_COLS);
    cursor_row = r - 1;
    cursor_col = c - 1;
}

static void do_ed(void) {      /* erase display */
    unsigned char n = get_p(0, 0);
    unsigned int addr, count;
    if (n == 2) {
        screen_cls();
        return;
    }
    if (n == 0) {
        addr  = screen_find_cursor();
        count = (unsigned int)(SCREEN_COLS - cursor_col)
              + (unsigned int)(SCREEN_ROWS - 1 - cursor_row) * SCREEN_COLS;
        screen_blank_at(addr, count);
    }
}

static void do_el(void) {      /* erase line */
    unsigned char n = get_p(0, 0);
    unsigned int addr;
    unsigned int count;
    if (n == 2) {
        /* whole line */
        addr  = screen_find_cursor_at(0, cursor_row);
        count = SCREEN_COLS;
    } else if (n == 0) {
        /* cursor to end of line */
        addr  = screen_find_cursor();
        count = SCREEN_COLS - cursor_col;
    } else {              /* n==1: start of line to cursor */
        addr  = screen_find_cursor_at(0, cursor_row);
        count = cursor_col + 1;
    }
    screen_blank_at(addr, count);
}

static void do_sgr(void) {     /* select graphic rendition */
    unsigned char i, v;
    for (i = 0; i < nparam || i == 0; i++) {
        v = (i < nparam) ? params[i] : 0;
        if (v == 0) {
            screen_set_fg(18);  /* bright green */
            screen_set_bg(0);   /* black */
        } else if (v == 1) {
            /* bold: not implemented visually, keep current color */
        } else if (v >= 30 && v <= 37) {
            screen_set_fg(ink_table[v - 30]);
        } else if (v >= 40 && v <= 47) {
            screen_set_bg(ink_table[v - 40]);
        } else if (v >= 90 && v <= 97) {
            screen_set_fg(ink_table[(v - 90) + 8]);
        } else if (v >= 100 && v <= 107) {
            screen_set_bg(ink_table[(v - 100) + 8]);
        }
        if (nparam == 0) break;
    }
}

/* -------------------------------------------------------------------------
 * Dispatch final character of an ESC[ sequence
 * -------------------------------------------------------------------------*/
static void dispatch(unsigned char cmd) {
    /* If have_digit, last param slot was filled; if not, sentinel 0 already there */
    if (!have_digit && nparam < MAX_PARAMS) {
        /* last separator had no following digit — param stays 0 (= default) */
    }
    switch (cmd) {
    case 'A': do_cuu(); break;
    case 'B': do_cud(); break;
    case 'C': do_cuf(); break;
    case 'D': do_cub(); break;
    case 'H': do_cup(); break;
    case 'f': do_cup(); break;
    case 'J': do_ed();  break;
    case 'K': do_el();  break;
    case 'm': do_sgr(); break;
    case 's': saved_col = cursor_col; saved_row = cursor_row; break;
    case 'u': cursor_col = saved_col; cursor_row = saved_row; break;
    default:  break;
    }
}

/* -------------------------------------------------------------------------
 * Public entry point — called by screen_write for ESC/0x9B and all
 * subsequent bytes until the sequence ends.
 * -------------------------------------------------------------------------*/
void ansi_feed(unsigned char c) {
    unsigned char n;

    switch (ansi_state) {
    case S_IDLE:
        if (c == 27) { ansi_state = S_ESC; }
        else if (c == 0x9B) { ansi_state = S_PARAMS; nparam = 0; have_digit = 0; }
        break;

    case S_ESC:
        if (c == '[') {
            ansi_state = S_PARAMS;
            nparam = 0;
            have_digit = 0;
        } else if (c == 'c') {
            /* RIS: full reset */
            screen_cls();
            reset();
        } else if (c == ']') {
            /* OSC: skip until BEL or ESC \ */
            ansi_state = S_OSC;
        } else if (c >= 0x20 && c <= 0x2F) {
            /* Intermediate byte (e.g. '(' for charset designator, ')' for G1).
             * One more final byte follows — consume it without printing. */
            ansi_state = S_ESC2;
        } else {
            /* Self-contained 2-byte ESC sequence (ESC '=', ESC '>', ESC 'M'…):
             * the byte is consumed here; nothing more to skip. */
            reset();
        }
        break;

    case S_ESC2:
        /* Consume the final byte of an ESC + intermediate sequence (e.g. 'B'
         * in ESC ( B).  Never reached for printable content. */
        reset();
        break;

    case S_OSC:
        if (c == 7) { reset(); }           /* BEL terminates OSC */
        else if (c == 27) { ansi_state = S_OSC_ESC; }
        break;

    case S_OSC_ESC:
        /* ESC \ (ST) terminates OSC; anything else — back to skipping */
        if (c == '\\') reset();
        else ansi_state = S_OSC;
        break;

    case S_PARAMS:
        if (c >= '0' && c <= '9') {
            n = c - '0';
            if (have_digit) {
                /* multiply existing value by 10 and add digit */
                params[nparam] = params[nparam] * 10 + n;
            } else {
                params[nparam] = n;
                have_digit = 1;
            }
        } else if (c == ';') {
            if (!have_digit) params[nparam] = 0;
            if (nparam < MAX_PARAMS - 1) nparam++;
            have_digit = 0;
            params[nparam] = 0;
        } else if (c == '?' || c == '>' || c == '<') {
            /* private parameter prefix, ignore */
        } else if (c >= '@' && c <= '~') {
            /* final byte: this is the command */
            if (!have_digit) params[nparam] = 0;
            if (have_digit || nparam > 0) {
                if (nparam < MAX_PARAMS) nparam++;
            }
            dispatch(c);
            reset();
        } else {
            /* unexpected byte: abandon sequence */
            reset();
        }
        break;
    }
}
