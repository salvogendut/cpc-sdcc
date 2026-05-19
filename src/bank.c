#include "bank.h"

/*
 * sdcccall(1): first char arg → A, second int arg → DE (use E).
 * bank in A (1-14), cfg in E.
 *
 * DK'Tronics/Yarek extended banking via OUT (C), A with B=0x7F:
 *   Port A15-A8 = 0x7F (A15=0 required — iRAM1024 PAL and gate array both need it)
 *   banks 1-7  → block 0: C=0x7F → full port 0x7F7F
 *   banks 8-14 → block 1: C=0x7E → full port 0x7F7E
 *   OUT data = 0xC0 | (bank_within_block<<3) | cfg
 *
 * Both bank_select and bank_restore use OUT (C), A so the ASIC gate array
 * (present on costdown CPC 464) and the iRAM1024 always receive matched
 * select/restore pairs and stay in consistent state.
 */
void bank_select(unsigned char bank, unsigned int cfg) __naked {
    (void)bank; (void)cfg;
    __asm
        ld   b, #0x7F
        cp   #8
        jr   c, 00001$        ; bank < 8: block 0 (port 0x7F7F)
        sub  #8               ; bank >= 8: adjust to within-block offset
        rlca
        rlca
        rlca
        or   e
        or   #0xC0
        ld   c, #0x7E         ; block 1: port 0x7F7E
        out  (c), a
        ret
    00001$:
        rlca
        rlca
        rlca
        or   e
        or   #0xC0
        ld   c, #0x7F         ; block 0: port 0x7F7F
        out  (c), a
        ret
    __endasm;
}

/* OUT (C), A with B=0x7F, C=0x7F, A=0xC0: bank=0 cfg=0 restores normal mapping.
 * Must use the same OUT (C), A form as bank_select so the ASIC gate array
 * receives the restore command and unmaps any expansion it activated. */
void bank_restore(void) __naked {
    __asm
        ld   b, #0x7F
        ld   c, #0x7F
        ld   a, #0xC0
        out  (c), a
        ret
    __endasm;
}
