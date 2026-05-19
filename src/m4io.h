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
 *
 * TODO: all response-buffer offsets and socket state encodings need
 *       verification against M4 firmware source or real hardware.
 */

void m4_out(unsigned int b);
void m4_strobe(void);
unsigned char *m4_resp(void);
void m4_wait(void);

#endif /* M4IO_H */
