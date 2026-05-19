#include "amsdos_in.h"

#ifdef AMSDOS_USB
  #define CAS_IN_OPEN_ADDR   0xBC77
  #define CAS_IN_CLOSE_ADDR  0xBC7A
  #define CAS_IN_READ_ADDR   0xBC80  /* ULIfAC CAS_IN_DIRECT — works for text; binary TBD */
#else
  #define CAS_IN_OPEN_ADDR   0xBC74
  #define CAS_IN_CLOSE_ADDR  0xBC77
  #define CAS_IN_READ_ADDR   0xBC7D  /* standard CPC CAS_IN_DIRECT — binary-safe */
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
        push ix
        call CAS_IN_OPEN_ADDR
        pop  ix
        ei                  ; ULIfAC may leave interrupts disabled; restore
        ld   a, #0
        jr   nc, 00001$
        ld   a, #1
    00001$:
        ret
    __endasm;
}

/* carry set = byte in A, carry clear = EOF.
 * Returns int: byte value 0-255, or -1 on EOF. */
int cas_in_readbyte(void) __naked {
    __asm
        push ix
        call CAS_IN_READ_ADDR
        pop  ix             ; POP does not alter carry flag on Z80
        ei                  ; restore interrupts; USB may need ISR between reads
        jr   c, 00001$
        ld   de, #-1        ; int return in DE (sdcccall(1))
        ret
    00001$:
        ld   e, a
        ld   d, #0
        ret
    __endasm;
}

void cas_in_close(void) __naked {
    __asm
        push ix
        call CAS_IN_CLOSE_ADDR
        pop  ix
        ei
        ret
    __endasm;
}
