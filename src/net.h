#ifndef NET_H
#define NET_H

/*
 * TCP socket API — socket 0 only.
 * All functions return 0 on success, -1 on error unless noted otherwise.
 */

/* Open socket 0 in TCP mode, ready to connect. */
int net_socket_open(void);

/* Connect to ip[4] (big-endian) on the given port (host byte order). */
int net_connect(const unsigned char *ip, unsigned int port);

/* Send len bytes from buf.  Returns 0 on success, -1 on error. */
int net_send(const unsigned char *buf, unsigned int len);

/*
 * Receive up to maxlen bytes into buf.
 * Returns number of bytes actually received (may be 0 if nothing ready).
 */
unsigned int net_recv(unsigned char *buf, unsigned int maxlen);

/* Graceful close: DISCON then CLOSE. */
void net_close(void);

/* Returns 1 if TCP connection is established, 0 otherwise. */
unsigned char net_is_connected(void);

/* Returns number of bytes waiting in the RX buffer (non-blocking check). */
unsigned int net_rx_available(void);

#endif /* NET_H */
