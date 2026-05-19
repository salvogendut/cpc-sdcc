#include "m4io.h"

static unsigned char m4_rom_num = 0xFF;

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

/* KL_ROM_SELECT (0xB90F): C = ROM slot number. */
static void kl_rom_select_slot(unsigned char slot) __naked {
    (void)slot;
    __asm
        ld      c, l            ; slot in L (sdcccall(1): first unsigned char in HL)
        call    0xB90F
        ret
    __endasm;
}

void m4_select_rom(void) {
    if (m4_rom_num != 0xFF)
        kl_rom_select_slot(m4_rom_num);
}

/*
 * Scan upper ROM slots 127..1 for the M4 ROM, identified by the RSX name
 * string "M4 BOAR\xC4" ('D'|0x80 = end-of-string marker used by CPC ROMs).
 * Stores slot number in m4_rom_num so m4_select_rom() can re-select it.
 */
void m4_rom_init(void) {
    static const unsigned char name[] = {
        'M', '4', ' ', 'B', 'O', 'A', 'R', 0xC4  /* 'D' | 0x80 */
    };
    unsigned char slot;
    unsigned char *rom_str;
    unsigned char i;

    for (slot = 127; slot > 0; slot--) {
        kl_rom_select_slot(slot);
        if (*(volatile unsigned char *)0xC000 != 1) continue;  /* not background ROM */
        rom_str = (unsigned char *)(*(unsigned int *)0xC004);   /* RSX command table */
        for (i = 0; i < 8; i++) {
            if (rom_str[i] != name[i]) goto next_slot;
            if (name[i] & 0x80) { m4_rom_num = slot; return; } /* end marker matched */
        }
    next_slot:;
    }
}

/* Select M4 ROM first so 0xFF02 is readable, then return response buffer pointer. */
unsigned char *m4_resp(void) {
    m4_select_rom();
    return (unsigned char *)(*(unsigned int *)0xFF02);
}

void m4_wait(void) {
    unsigned int i = 5000U;
    while (i--)
        ;
}
