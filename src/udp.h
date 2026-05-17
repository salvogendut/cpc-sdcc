#ifndef UDP_H
#define UDP_H

/*
 * UDP API — Socket 1 on the W5100S.
 *
 * W5100S UDP receive format: the chip prepends an 8-byte header to every
 * received datagram stored in the RX ring buffer:
 *   [0..3]  source IP (4 bytes)
 *   [4..5]  source port (2 bytes, big-endian)
 *   [6..7]  data length (2 bytes, big-endian)
 * udp_recv() strips this header and returns only the payload.
 */

/* Open UDP socket 1 bound to src_port (big-endian, 1–65535). */
int udp_open(unsigned int src_port);

/*
 * Send len bytes from buf to dst_ip:dst_port via UDP.
 * Returns 0 on success, -1 on error.
 */
int udp_sendto(const unsigned char *dst_ip, unsigned int dst_port,
               const unsigned char *buf, unsigned int len);

/* Return number of bytes waiting in socket 1 RX buffer (including headers). */
unsigned int udp_rx_available(void);

/*
 * Receive one UDP datagram into buf (up to maxlen payload bytes).
 * Strips the 8-byte W5100S UDP header. Issues RECV command when done.
 * Returns payload length, or 0 if nothing available / buffer too small.
 */
unsigned int udp_recv(unsigned char *buf, unsigned int maxlen);

/* Close socket 1. */
void udp_close(void);

#endif /* UDP_H */
