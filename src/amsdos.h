#ifndef AMSDOS_H
#define AMSDOS_H

/*
 * AMSDOS CAS output wrappers for SDCC -mz80.
 *
 * CAS_OUT routines use standard firmware addresses on both ULIfAC/floppy
 * and Albireo/USB — no address shift.
 *
 * All routines return 1 on success, 0 on failure (mirroring carry set/clear).
 *
 * Calling-convention note for cas_out_open:
 *   CAS_OUT_OPEN needs HL=fname, B=len, A=type.
 *   Signature (const char *, int) → HL=fname, DE=len via sdcccall(1).
 *   flen declared as int (not char) so it lands in DE/E, avoiding the
 *   (ptr, char) stack-push path that would require manual cleanup.
 */

/* Open a new output file. fname = null-terminated name, flen = length,
   ftype = 2 for binary.  Carry set = success. */
static unsigned char cas_out_open(const char *fname, int flen) __naked {
    (void)fname; (void)flen;
    __asm
        ; HL = fname (first arg), DE = flen (second arg, E = byte length)
        ld      b, e            ; B = filename length
        ld      a, #2           ; A = file type: 2 = binary
        call    0xBC8C          ; CAS_OUT_OPEN
        ld      a, #0
        ret     nc
        ld      a, #1
        ret
    __endasm;
}

/* Write one byte to the open output file.  Carry set = success. */
static unsigned char cas_out_char(char c) __naked {
    (void)c;
    __asm
        call    0xBC95          ; CAS_OUT_CHAR  (A = byte to write)
        ld      a, #0
        ret     nc
        ld      a, #1
        ret
    __endasm;
}

/* Close the output file (renames .$$$).  Carry set = success. */
static unsigned char cas_out_close(void) __naked {
    __asm
        call    0xBC8F          ; CAS_OUT_CLOSE
        ld      a, #0
        ret     nc
        ld      a, #1
        ret
    __endasm;
}

/* Discard the output file without renaming (leaves no .$$$). */
static void cas_out_abandon(void) __naked {
    __asm
        call    0xBC92          ; CAS_OUT_ABANDON
        ret
    __endasm;
}

#endif /* AMSDOS_H */
