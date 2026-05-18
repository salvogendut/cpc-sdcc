#include "amsdos_in.h"

#ifdef AMSDOS_USB
  #define CAS_IN_OPEN_ADDR   0xBC77
  #define CAS_IN_CLOSE_ADDR  0xBC7A
  #define CAS_IN_DIRECT_ADDR 0xBC83
#else
  #define CAS_IN_OPEN_ADDR   0xBC74
  #define CAS_IN_CLOSE_ADDR  0xBC77
  #define CAS_IN_DIRECT_ADDR 0xBC80
#endif

/*
 * sdcccall(1): fname (1st ptr) → HL,  flen (2nd int) → DE.
 * CAS_IN_OPEN wants: HL = filename address, B = length, A = type (0xFF = any).
 * We load B from E (low byte of DE).
 * Returns 1 on success (carry set by firmware), 0 on failure.
 */
unsigned char cas_in_open(const char *fname, unsigned int flen) __naked {
    (void)fname; (void)flen;
    __asm
        ld   b, e
        ld   a, #0xFF
        call CAS_IN_OPEN_ADDR
        ld   l, #0
        jr   nc, 00001$
        ld   l, #1
    00001$:
        ld   h, #0
        ret
    __endasm;
}

/*
 * CAS_IN_DIRECT: carry set = byte in A, carry clear = EOF.
 * Returns int: byte value 0-255, or -1 on EOF.
 */
int cas_in_readbyte(void) __naked {
    __asm
        call CAS_IN_DIRECT_ADDR
        jr   c, 00001$
        ld   hl, #-1
        ret
    00001$:
        ld   h, #0
        ld   l, a
        ret
    __endasm;
}

void cas_in_close(void) __naked {
    __asm
        call CAS_IN_CLOSE_ADDR
        ret
    __endasm;
}
