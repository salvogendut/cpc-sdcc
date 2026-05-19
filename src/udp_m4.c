#include "udp.h"
#include "m4io.h"

/*
 * M4 Board UDP driver.
 *
 * UNVERIFIED: M4 UDP support not confirmed in known example code.
 * Uses C_NETSOCKET with SOCK_DGRAM (type=2, protocol=17).
 * src_port binding is not guaranteed — M4 may auto-assign the source port.
 *
 * Packet format and response offsets same as net_m4.c.
 * TODO: verify on real hardware before relying on this for NTP.
 */

#define C_NETSOCKET  0x4331
#define C_NETCONNECT 0x4332
#define C_NETCLOSE   0x4333
#define C_NETSEND    0x4334
#define C_NETRECV    0x4335

#define M4_SOCK_BASE         0xFF06
#define M4_SOCK_STATE_CLOSED 3   /* TODO: verify */

static unsigned char m4_udp_socket;

static unsigned char m4_udp_state(void) {
    return *((unsigned char *)(M4_SOCK_BASE + (unsigned int)m4_udp_socket * 16));
}

int udp_open(unsigned int src_port) {
    unsigned char *resp;
    (void)src_port;   /* TODO: M4 may not support explicit source port binding */
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
    /* C_NETCONNECT sets the destination address for UDP */
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
    return (m4_udp_state() != M4_SOCK_STATE_CLOSED) ? 1 : 0;
}

unsigned int udp_recv(unsigned char *buf, unsigned int maxlen) {
    unsigned char *resp;
    unsigned int recv_len, i;
    if (maxlen > 2048) maxlen = 2048;
    /* C_NETRECV: socket, requested-size LE */
    m4_out(5);
    m4_out(0x35); m4_out(0x43);
    m4_out((unsigned int)m4_udp_socket);
    m4_out((unsigned int)(maxlen & 0xFF));
    m4_out((unsigned int)(maxlen >> 8));
    m4_strobe();
    m4_wait();
    resp = m4_resp();
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
