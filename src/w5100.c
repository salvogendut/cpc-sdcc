#include "w5100.h"

/*
 * W5100S indirect-bus access on the Net4CPC.
 *
 * Protocol: write high-address byte to 0xFD21, low-address byte to 0xFD22,
 * then read/write data at 0xFD23.  The W5100S auto-increments its internal
 * address after each DATA port access (AI bit set in MR at power-on),
 * enabling burst transfers by looping on 0xFD23 after setting the start
 * address once.
 *
 * SDCC sdcccall(1) usage:
 *   w5100_read_reg  (unsigned int addr)             addr → HL
 *   w5100_write_reg (unsigned int addr, unsigned int val) addr → HL, val → DE
 *   Char return value → A.
 */

unsigned char w5100_read_reg(unsigned int addr) __naked {
    (void)addr;
    __asm
        push    bc
        ld      bc, #0xFD21
        out     (c), h          ; high address byte
        inc     c
        out     (c), l          ; low address byte
        inc     c               ; C = 0xFD23 (DATA port)
        in      a, (c)          ; A = data  -- sdcccall(1) char return in A
        pop     bc
        ret
    __endasm;
}

void w5100_write_reg(unsigned int addr, unsigned int val) __naked {
    (void)addr; (void)val;
    __asm
        push    bc
        ld      bc, #0xFD21
        out     (c), h          ; high address byte
        inc     c
        out     (c), l          ; low address byte
        inc     c               ; C = 0xFD23
        out     (c), e          ; E = low byte of val (DE = second int arg)
        pop     bc
        ret
    __endasm;
}

/*
 * Buffer copy via repeated single-register calls.
 *
 * Each call to w5100_write_reg sets the W5100S address (2 OUTs) then writes
 * 1 data byte (1 OUT) = 3 OUTs per byte.  A burst path (1 OUT per byte after
 * the first setup) is possible but requires the IX frame to be present, which
 * SDCC omits for pure-asm bodies when the third sdcccall(1) argument is
 * stack-passed.  Plain C loops are correct and fast enough for terminal buffers.
 */
void w5100_write_buf(unsigned int w5100addr, const unsigned char *buf,
                     unsigned int len) {
    while (len--)
        w5100_write_reg(w5100addr++, (unsigned int)*buf++);
}

void w5100_read_buf(unsigned int w5100addr, unsigned char *buf,
                    unsigned int len) {
    while (len--)
        *buf++ = w5100_read_reg(w5100addr++);
}
