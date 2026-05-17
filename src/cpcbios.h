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

#endif /* CPCBIOS_H */
