#include "m4io.h"

void m4_out(unsigned int b) __naked {
    (void)b;
    __asm
        push    bc
        ld      bc, #0xFE00
        out     (c), l          ; L = low byte of b (sdcccall(1): first int in HL)
        pop     bc
        ret
    __endasm;
}

void m4_strobe(void) __naked {
    __asm
        push    bc
        ld      bc, #0xFC00
        xor     a
        out     (c), a
        pop     bc
        ret
    __endasm;
}

/* Response buffer pointer is stored as a 16-bit LE value at 0xFF02. */
unsigned char *m4_resp(void) {
    return (unsigned char *)(*(unsigned int *)0xFF02);
}

/* Spin-wait for M4 to process a command.
 * TODO: calibrate on real hardware — 5000 iterations is a conservative start. */
void m4_wait(void) {
    unsigned int i = 5000U;
    while (i--)
        ;
}
