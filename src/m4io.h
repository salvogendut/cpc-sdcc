#ifndef M4IO_H
#define M4IO_H

/*
 * M4 Board low-level I/O helpers shared by net_m4.c, dns_m4.c, udp_m4.c.
 *
 * Ports:
 *   0xFE00  data    — write one byte per OUT (C), L with BC=0xFE00
 *   0xFC00  strobe  — strobe after last command byte to signal end of packet
 *
 * Response buffer: 16-bit LE pointer at memory address 0xFF02.
 * Socket info table: 16-bit LE pointer at memory address 0xFF06.
 *
 * Both addresses are in the M4 ROM's address space (0xC000-0xFFFF).
 * Call m4_select_rom() before dereferencing either pointer; firmware calls
 * (txt_output, km_read_char, etc.) may change the active upper ROM.
 *
 * Call m4_rom_init() once at program startup before any other m4_ calls.
 */

void m4_rom_init(void);         /* scan upper ROMs, find and store M4 slot */
void m4_select_rom(void);       /* re-select M4 upper ROM (via KL_ROM_SELECT 0xB90F) */
unsigned char m4_rom_slot(void); /* return found ROM slot (0xFF = not found) */

void m4_out(unsigned int b);
void m4_strobe(void);
unsigned char *m4_resp(void);   /* selects M4 ROM then returns *(uint16_t*)0xFF02 */
void m4_wait(void);
void m4_select_basic(void);     /* select BASIC ROM slot 0 — call after M4 ops complete */

#endif /* M4IO_H */
