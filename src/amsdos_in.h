#ifndef AMSDOS_IN_H
#define AMSDOS_IN_H

/*
 * AMSDOS CAS IN wrappers for reading files from disk.
 *
 * Compile with -DAMSDOS_USB for Albireo/Unidos (CAS IN shifted +3).
 * Otherwise standard AMSDOS addresses are used (ULIfAC / real floppy).
 *
 * fname  — null-terminated filename
 * flen   — filename length (declared unsigned int so sdcccall(1) passes it
 *          in DE, avoiding the pushed-char ABI which requires __naked stack
 *          manipulation — see SDCC calling convention notes in README)
 */

/* Open file for reading. Returns 1 on success, 0 if not found. */
unsigned char cas_in_open(const char *fname, unsigned int flen);

/* Read next byte. Returns 0-254 on success, -1 on EOF/error.
 * (0xFF is returned as part of an AMSDOS header, not as a sentinel.) */
int cas_in_readbyte(void);

/* Close the open input file. */
void cas_in_close(void);

#endif /* AMSDOS_IN_H */
