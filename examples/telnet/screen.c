#include "screen.h"
#include "ansi.h"

/* Charset: 2048 bytes loaded by BASIC at 0x6800.
 * Layout: pixel row R of char N is at address 0x6800 + R*256 + N. */
#define CHARSET_BASE ((unsigned int)0x6800)

/* Firmware entry points (verified against termN4C.s from n4cewenterm) */
#define FW_KL_U_ROM_DISABLE  0xB903
#define FW_KL_ROM_RESTORE    0xB90C
#define FW_SCR_SET_MODE      0xBC0E
#define FW_SCR_SET_OFFSET    0xBC05
#define FW_SCR_SET_INK       0xBC32
#define FW_SCR_HW_ROLL       0xBC4D
#define FW_MC_WAIT_FLYBACK   0xBD19

unsigned char cursor_col;
unsigned char cursor_row;
unsigned int  screen_offset;

static unsigned char rom_state;

/* Disable upper ROM so we can write to 0xC000–0xFFFF */
static void romdis(void) __naked {
    __asm
        call #0xB903
        ld (_rom_state),a
        ret
    __endasm;
}

/* Re-enable upper ROM */
static void romen(void) __naked {
    __asm
        ld a,(_rom_state)
        call #0xB90C
        ret
    __endasm;
}

/* SCR_SET_OFFSET: HL = screen offset (first int param → HL via sdcccall(1)) */
static void scr_set_offset(unsigned int offset) __naked {
    (void)offset;
    __asm
        call #0xBC05
        ret
    __endasm;
}

/* SCR_HW_ROLL: scroll up (B=255), fill with ink 0 (A=0) */
static void scr_hw_roll_up(void) __naked {
    __asm
        ld b,#0xFF
        xor a
        call #0xBC4D
        ret
    __endasm;
}

/* MC_WAIT_FLYBACK: wait for screen flyback before clearing */
static void mc_wait_flyback(void) __naked {
    __asm
        call #0xBD19
        ret
    __endasm;
}

/* SCR_SET_INK: A=pen, B=ink1, C=ink2 (non-flashing: B=C).
 * ink declared unsigned int so sdcccall(1) passes it in DE (E = value).
 * Declaring it unsigned char would push it on the stack instead. */
static void scr_set_ink(unsigned char pen, unsigned int ink) __naked {
    (void)pen; (void)ink;
    __asm
        ld b,e
        ld c,e
        call #0xBC32
        ret
    __endasm;
}

/* SCR_SET_BORDER (0xBC38): B=ink1, C=ink2 (non-flashing: B=C).
 * Single char arg → A; move to B and C before calling firmware. */
static void scr_set_border(unsigned char ink) __naked {
    (void)ink;
    __asm
        ld b,a
        ld c,a
        call #0xBC38
        ret
    __endasm;
}

/* LDIR to zero 0xC000–0xFFFF (16 KB, 16384 bytes = 1 + 16383 via LDIR) */
static void fill_screen_zero(void) __naked {
    __asm
        ld hl,#0xC000
        ld de,#0xC001
        ld bc,#0x3FFF
        ld (hl),#0
        ldir
        ret
    __endasm;
}

/* Compute screen byte address for (col, row) with current screen_offset.
 * CPC Mode 2 layout: scan lines are 0x0800 apart; this returns the top
 * scan-line address.  H is masked to (H & 7) | 0xC0. */
unsigned int screen_find_cursor(void) {
    return screen_find_cursor_at(cursor_col, cursor_row);
}

unsigned int screen_find_cursor_at(unsigned char col, unsigned char row) {
    unsigned int addr = (unsigned int)row * 80 + 0xC000 + col + screen_offset;
    unsigned char h = (unsigned char)(addr >> 8);
    return ((unsigned int)((h & 7) | 0xC0) << 8) | (addr & 0xFF);
}

/* Write one character cell from the charset into screen RAM.
 * Successive scan lines are 0x0800 apart (H += 8). */
static void render_char(unsigned char ch) {
    unsigned int addr = screen_find_cursor();
    unsigned char *cptr = (unsigned char *)(CHARSET_BASE + ch);
    unsigned char row;
    romdis();
    for (row = 0; row < 8; row++) {
        *((unsigned char *)addr) = *cptr;
        cptr += 256;
        addr = (addr & 0x00FF) | ((unsigned int)((unsigned char)(addr >> 8) + 8) << 8);
    }
    romen();
}

/* Blank `count` consecutive character cells starting at screen address `addr`.
 * Each cell: write 0 to 8 scan lines (H += 8 per row), then advance one column
 * (INC L, re-mask H to (H & 7) | 0xC0). */
void screen_blank_at(unsigned int addr, unsigned int count) {
    unsigned char row;
    unsigned char h;
    unsigned int col_addr;
    romdis();
    while (count--) {
        h = (unsigned char)(addr >> 8);
        col_addr = addr & 0x00FF;
        for (row = 0; row < 8; row++) {
            *((unsigned char *)((unsigned int)h << 8 | col_addr)) = 0;
            h += 8;
        }
        addr++;
        h = (unsigned char)(addr >> 8);
        addr = (addr & 0x00FF) | ((unsigned int)((h & 7) | 0xC0) << 8);
    }
    romen();
}

void screen_cls(void) {
    mc_wait_flyback();
    romdis();
    fill_screen_zero();
    romen();
    screen_offset = 0;
    scr_set_offset(0);
    cursor_col = 0;
    cursor_row = 0;
}

static void scroll_up(void) {
    unsigned int new_off;
    unsigned char h;
    scr_hw_roll_up();   /* firmware advances hardware offset by 80 and fills new row */
    new_off = screen_offset + 80;
    h = (unsigned char)(new_off >> 8);
    screen_offset = (new_off & 0x00FF) | ((unsigned int)(h & 7) << 8);
}

void screen_set_fg(unsigned char ink) {
    scr_set_ink(1, ink);
}

void screen_set_bg(unsigned char ink) {
    scr_set_ink(0, ink);
}

void screen_init(void) {
    __asm
        ld a,#2
        call #0xBC0E
    __endasm;
    cursor_col = 0;
    cursor_row = 0;
    screen_offset = 0;
    screen_cls();
    screen_set_fg(18);   /* ink 18 = Bright Green */
    screen_set_bg(0);    /* ink 0  = Black */
    scr_set_border(0);   /* border = Black */
}

/* XOR the bottom scan line of the cursor cell to draw/erase an underline cursor. */
void screen_cursor_draw(void) {
    unsigned int addr = screen_find_cursor();
    unsigned char h;
    /* Advance to scan line 7 (bottom of cell): add 7*8 = 56 to high byte */
    h = (unsigned char)(addr >> 8) + 56;
    addr = (addr & 0x00FF) | ((unsigned int)h << 8);
    romdis();
    *((unsigned char *)addr) ^= 0xFF;
    romen();
}

void screen_cursor_erase(void) {
    screen_cursor_draw();   /* XOR is its own inverse */
}

void screen_write(unsigned char c) {
    if (ansi_active() || c == 27 || c == 0x9B) {
        ansi_feed(c);
        return;
    }
    if (c == '\r') { cursor_col = 0; return; }
    if (c == '\n') {
        if (cursor_row < SCREEN_ROWS - 1) cursor_row++;
        else scroll_up();
        return;
    }
    if (c == 8) {           /* BS */
        if (cursor_col > 0) cursor_col--;
        return;
    }
    if (c == '\t') {
        cursor_col = (cursor_col + 8) & 0xF8;
        if (cursor_col >= SCREEN_COLS) cursor_col = SCREEN_COLS - 1;
        return;
    }
    if (c < 0x20) return;
    render_char(c);
    cursor_col++;
    if (cursor_col >= SCREEN_COLS) {
        cursor_col = 0;
        if (cursor_row < SCREEN_ROWS - 1) cursor_row++;
        else scroll_up();
    }
}
