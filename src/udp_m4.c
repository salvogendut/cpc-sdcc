#include "udp.h"
#include "m4io.h"

/*
 * M4 Board UDP driver — socket state encoding verified from m4ewenterm source.
 *
 * UNVERIFIED: M4 UDP socket support not confirmed in known examples.
 * Uses C_NETSOCKET with SOCK_DGRAM (type=2, protocol=17).
 * src_port binding is not guaranteed — M4 may auto-assign source port.
 *
 * Socket info table: pointer at 0xFF06 (16-bit LE).
 *   Entry for socket N at table_base + N*16.
 *   entry[0] = state, entry[2:3] = RX byte count LE
 */

#define C_NETSOCKET  0x4331
#define C_NETCONNECT 0x4332
#define C_NETCLOSE   0x4333
#define C_NETSEND    0x4334
#define C_NETRECV    0x4335

#define M4_SOCK_STATE_CLOSED 3

static unsigned char m4_udp_socket;

static unsigned char *m4_udp_info(void) {
    return (unsigned char *)(*(unsigned int *)0xFF06) +
           (unsigned int)m4_udp_socket * 16;
}

int udp_open(unsigned int src_port) {
    unsigned char *resp;
    (void)src_port;   /* M4 may not support explicit source port binding */
    /* C_NETSOCKET: domain=0, type=2 (SOCK_DGRAM), protocol=17 (UDP) */
    m4_out(5);
    m4_out(0x31); m4_out(0x43);
    m4_out(0); m4_out(2); m4_out(17);
    m4_strobe();
    m4_wait();
    resp = m4_resp();
    m4_udp_socket = resp[3];
    return (m4_udp_socket == 0xFF) ? -1 : 0;
}

int udp_sendto(const unsigned char *dst_ip, unsigned int dst_port,
               const unsigned char *buf, unsigned int len) {
    unsigned char chunk, i;
    /* C_NETCONNECT sets destination for this UDP datagram */
    m4_out(9);
    m4_out(0x32); m4_out(0x43);
    m4_out((unsigned int)m4_udp_socket);
    m4_out((unsigned int)dst_ip[0]);
    m4_out((unsigned int)dst_ip[1]);
    m4_out((unsigned int)dst_ip[2]);
    m4_out((unsigned int)dst_ip[3]);
    m4_out((unsigned int)(dst_port & 0xFF));
    m4_out((unsigned int)(dst_port >> 8));
    m4_strobe();
    m4_wait();

    if (len > 250) len = 250;
    chunk = (unsigned char)len;
    m4_out((unsigned int)(5 + chunk));
    m4_out(0x34); m4_out(0x43);
    m4_out((unsigned int)m4_udp_socket);
    m4_out((unsigned int)chunk);
    m4_out(0);
    for (i = 0; i < chunk; i++)
        m4_out((unsigned int)buf[i]);
    m4_strobe();
    m4_wait();
    return 0;
}

unsigned int udp_rx_available(void) {
    unsigned char *sock = m4_udp_info();
    if (sock[0] == M4_SOCK_STATE_CLOSED) return 0;
    return (unsigned int)sock[2] | ((unsigned int)sock[3] << 8);
}

unsigned int udp_recv(unsigned char *buf, unsigned int maxlen) {
    unsigned char *sock, *resp;
    unsigned int recv_len, i;
    sock = m4_udp_info();
    if (!sock[2] && !sock[3]) return 0;
    if (maxlen > 2048) maxlen = 2048;
    m4_out(5);
    m4_out(0x35); m4_out(0x43);
    m4_out((unsigned int)m4_udp_socket);
    m4_out((unsigned int)(maxlen & 0xFF));
    m4_out((unsigned int)(maxlen >> 8));
    m4_strobe();
    m4_wait();
    resp = m4_resp();
    if (resp[3] != 0) return 0;
    recv_len = (unsigned int)resp[4] | ((unsigned int)resp[5] << 8);
    if (!recv_len) return 0;
    if (recv_len > maxlen) recv_len = maxlen;
    for (i = 0; i < recv_len; i++)
        buf[i] = resp[6 + i];
    return recv_len;
}

void udp_close(void) {
    m4_out(3);
    m4_out(0x33); m4_out(0x43);
    m4_out((unsigned int)m4_udp_socket);
    m4_strobe();
    m4_wait();
    m4_udp_socket = 0;
}
