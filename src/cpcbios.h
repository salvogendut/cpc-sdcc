#ifndef CPCBIOS_H
#define CPCBIOS_H

/*
 * CPC firmware wrappers for SDCC -mz80.
 *
 * SDCC 4.x uses sdcccall(1) by default on z80: first char/byte argument
 * is passed in A, first int/pointer in HL.  The firmware routines below
 * all take their argument in A, so __naked wrappers that just CALL and
 * RET are correct — no frame setup needed.
 */

/* Print one character (in A) via TXT_OUTPUT. */
static void cpc_print_char(char c) __naked {
    (void)c;
    __asm
        call 0xBB5A
        ret
    __endasm;
}

/* Clear the text window (CLS). */
static void cpc_cls(void) __naked {
    __asm
        call 0xBB6C
        ret
    __endasm;
}

/* Set screen mode: 0 = 160x200 16-colour, 1 = 320x200 4-colour,
                    2 = 640x200 2-colour.  Mode passed in A.  */
static void cpc_set_mode(char mode) __naked {
    (void)mode;
    __asm
        call 0xBC0E
        ret
    __endasm;
}

/* Print a NUL-terminated string. Pointer passed in HL by sdcccall(1). */
static void cpc_print(const char *s) {
    while (*s)
        cpc_print_char(*s++);
}

/* Non-blocking key check (KM_READ_CHAR).
 * Returns the ASCII code if a key is available, -1 if not. */
static int cpc_read_key(void) __naked {
    __asm
        call    0xBB09
        jr      c, 00001$
        ld      de, #-1
        ret
    00001$:
        ld      e, a
        ld      d, #0
        ret
    __endasm;
}

/* Physical key state check (KM_TEST_KEY).
 * key_num = key matrix number. Returns 1 if held, 0 if not.
 * Bypasses translation table — unaffected by KM_SET_TRANSLATE remapping. */
static unsigned char cpc_test_key(unsigned char key_num) __naked {
    (void)key_num;
    __asm
        call 0xBB39
        ld   de, #0
        ret  nc
        ld   e, #1
        ret
    __endasm;
}

/* Wait for a keypress and return the ASCII code (KM_WAIT_CHAR). */
static char cpc_wait_key(void) __naked {
    __asm
        call 0xBB06
        ret
    __endasm;
}

/* Returns elapsed milliseconds from the CPC 50 Hz frame counter at 0xB5CB.
 * Wraps at 65535 ms (~65 s). Interrupts must be enabled (they are under BASIC). */
static unsigned int cpc_time_ms(void) __naked {
    __asm
        ld      hl, (#0xB5CB)
        add     hl, hl
        add     hl, hl
        ld      b, h
        ld      c, l
        add     hl, hl
        add     hl, hl
        add     hl, bc
        ld      d, h
        ld      e, l
        ret
    __endasm;
}

#endif /* CPCBIOS_H */
