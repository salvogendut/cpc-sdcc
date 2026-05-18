#include "bank.h"

/*
 * sdcccall(1): first char arg → A, second int arg → DE (use E).
 * bank in A, cfg in E.
 *
 * OUT value = 0xC0 | (bank<<3) | cfg
 * RLCA×3 shifts bank into bits 5:3.
 */
void bank_select(unsigned char bank, unsigned int cfg) __naked {
    (void)bank; (void)cfg;
    __asm
        rlca
        rlca
        rlca
        or   e
        or   #0xC0
        out  (#0x7F), a
        ret
    __endasm;
}

/* Write 0 to port &7F — clears the enable bits, restoring normal CPC RAM. */
void bank_restore(void) __naked {
    __asm
        xor  a
        out  (#0x7F), a
        ret
    __endasm;
}
