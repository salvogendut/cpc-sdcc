#include "net.h"
#include "m4io.h"

/*
 * M4 Board TCP driver.
 *
 * Packet format:
 *   byte 0:    payload length (number of bytes that follow, NOT counting itself)
 *   bytes 1-2: command word, little-endian
 *   bytes 3+:  parameters
 *
 * Response buffer at address in 0xFF02 (LE pointer):
 *   resp[3]    socket number returned by C_NETSOCKET (0xFF = error)
 *   resp[4:5]  received length LE returned by C_NETRECV
 *   resp[6+]   received data from C_NETRECV
 *
 * Socket state byte at (socket * 16) + 0xFF06:
 *   TODO: verify exact encoding — assumed 0=idle, 1=connecting, 2=active, 3=closed
 *
 * Max C_NETSEND payload per call: 250 bytes (pkt[0] <= 255 with 5-byte overhead).
 * Max C_NETRECV response per call: 2048 bytes.
 */

#define C_NETSOCKET  0x4331
#define C_NETCONNECT 0x4332
#define C_NETCLOSE   0x4333
#define C_NETSEND    0x4334
#define C_NETRECV    0x4335

#define M4_SOCK_BASE         0xFF06
#define M4_SOCK_STATE_ACTIVE 2   /* TODO: verify */
#define M4_SOCK_STATE_CLOSED 3   /* TODO: verify */

static unsigned char m4_socket;

static unsigned char m4_sock_state(void) {
    return *((unsigned char *)(M4_SOCK_BASE + (unsigned int)m4_socket * 16));
}

int net_socket_open(void) {
    unsigned char *resp;
    /* C_NETSOCKET: domain=0, type=0, protocol=6 (TCP) */
    m4_out(5);
    m4_out(0x31); m4_out(0x43);    /* C_NETSOCKET LE */
    m4_out(0); m4_out(0); m4_out(6);
    m4_strobe();
    m4_wait();
    resp = m4_resp();
    m4_socket = resp[3];
    return (m4_socket == 0xFF) ? -1 : 0;
}

int net_listen(unsigned int port) {
    (void)port;
    return -1; /* TODO: implement server mode when needed */
}

int net_connect(const unsigned char *ip, unsigned int port) {
    unsigned long timeout;
    /* C_NETCONNECT: socket, 4×IP, 2×port (LE) */
    m4_out(9);
    m4_out(0x32); m4_out(0x43);                /* C_NETCONNECT LE */
    m4_out((unsigned int)m4_socket);
    m4_out((unsigned int)ip[0]);
    m4_out((unsigned int)ip[1]);
    m4_out((unsigned int)ip[2]);
    m4_out((unsigned int)ip[3]);
    m4_out((unsigned int)(port & 0xFF));
    m4_out((unsigned int)(port >> 8));
    m4_strobe();
    /* Poll socket state until connected or closed.
     * TODO: calibrate count — 3000000 aims for ~10–15 s at 4 MHz. */
    timeout = 3000000UL;
    while (timeout--) {
        unsigned char state = m4_sock_state();
        if (state == M4_SOCK_STATE_ACTIVE) return 0;
        if (state == M4_SOCK_STATE_CLOSED) return -1;
    }
    return -1;
}

int net_send(const unsigned char *buf, unsigned int len) {
    while (len) {
        /* Max 250 data bytes per call so pkt[0] stays <= 255. */
        unsigned char chunk = (len > 250) ? 250 : (unsigned char)len;
        unsigned char i;
        /* C_NETSEND: socket, size-lo, size-hi, data */
        m4_out((unsigned int)(5 + chunk));
        m4_out(0x34); m4_out(0x43);            /* C_NETSEND LE */
        m4_out((unsigned int)m4_socket);
        m4_out((unsigned int)chunk);           /* size lo */
        m4_out(0);                             /* size hi */
        for (i = 0; i < chunk; i++)
            m4_out((unsigned int)buf[i]);
        m4_strobe();
        m4_wait();
        buf += chunk;
        len -= chunk;
    }
    return 0;
}

unsigned int net_recv(unsigned char *buf, unsigned int maxlen) {
    unsigned char *resp;
    unsigned int recv_len, i;
    if (maxlen > 2048) maxlen = 2048;
    /* C_NETRECV: socket, requested-size LE */
    m4_out(5);
    m4_out(0x35); m4_out(0x43);                /* C_NETRECV LE */
    m4_out((unsigned int)m4_socket);
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

void net_close(void) {
    /* C_NETCLOSE: socket */
    m4_out(3);
    m4_out(0x33); m4_out(0x43);                /* C_NETCLOSE LE */
    m4_out((unsigned int)m4_socket);
    m4_strobe();
    m4_wait();
    m4_socket = 0;
}

unsigned char net_is_connected(void) {
    if (!m4_socket) return 0;
    return (m4_sock_state() != M4_SOCK_STATE_CLOSED) ? 1 : 0;
}

unsigned int net_rx_available(void) {
    return net_is_connected() ? 1 : 0;
}
