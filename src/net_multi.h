#ifndef NET_MULTI_H
#define NET_MULTI_H

/*
 * Multi-socket TCP API for W5100S sockets 0-3.
 * All socket numbers are 0-3; socket register base = 0x0400 + (s << 8).
 * TX buffer base = 0x4000 + s*0x0800; RX buffer base = 0x6000 + s*0x0800.
 * Buffer size per socket: 2 KB (N_TMSR = N_RMSR = 0x55).
 */

/* Compute W5100S register address for socket s, register offset off */
#define SREG(s, off)   (0x0400u + ((unsigned int)(s) << 8) + (unsigned int)(off))
/* TX ring buffer base for socket s */
#define STX_BASE(s)    (0x4000u + ((unsigned int)(s) << 11))
/* RX ring buffer base for socket s */
#define SRX_BASE(s)    (0x6000u + ((unsigned int)(s) << 11))
/* Ring buffer mask for 2 KB */
#define S_BUF_MASK     0x07FFu

/* Socket register offsets (from socket base) */
#define SO_MR      0x00u   /* mode */
#define SO_CR      0x01u   /* command */
#define SO_IR      0x02u   /* interrupt */
#define SO_SR      0x03u   /* status */
#define SO_PORT0   0x04u   /* source port (2 bytes, big-endian) */
#define SO_TX_FSR  0x20u   /* TX free size (2 bytes) */
#define SO_TX_WR   0x24u   /* TX write pointer */
#define SO_RX_RSR  0x26u   /* RX received size (2 bytes) */
#define SO_RX_RD   0x28u   /* RX read pointer */

/* Open socket s in TCP LISTEN mode on port.
 * Returns 0 on success, -1 on error. */
int tcp_listen_sock(unsigned char s, unsigned int port);

/* Returns raw W5100S socket status byte (SSTAT_xxx). */
unsigned char tcp_get_status_sock(unsigned char s);

/* Send len bytes from buf over socket s.
 * Returns 0 on success, -1 on error. */
int tcp_send_sock(unsigned char s, const unsigned char *buf, unsigned int len);

/* Receive up to maxlen bytes from socket s into buf.
 * Returns bytes received (may be 0). */
unsigned int tcp_recv_sock(unsigned char s, unsigned char *buf, unsigned int maxlen);

/* Returns bytes waiting in socket s RX buffer. */
unsigned int tcp_rx_available_sock(unsigned char s);

/* Returns bytes free in socket s TX buffer. */
unsigned int tcp_tx_free_sock(unsigned char s);

/* Graceful close (DISCON then CLOSE) of socket s. */
void tcp_close_sock(unsigned char s);

#endif /* NET_MULTI_H */
