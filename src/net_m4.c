#include "net.h"
#include "m4io.h"

/*
 * M4 Board TCP driver — verified against m4ewenterm/src/telnetfunc2.s.
 *
 * Packet format:
 *   byte 0:    payload length (bytes that follow, NOT counting itself)
 *   bytes 1-2: command word, little-endian
 *   bytes 3+:  parameters
 *
 * Response buffer: pointer stored at 0xFF02 (16-bit LE).
 *   resp[3]   = status / socket number (C_NETSOCKET: socket#; others: 0=ok)
 *   resp[4:5] = received length LE (C_NETRECV only)
 *   resp[6+]  = received data (C_NETRECV only)
 *
 * Socket info table: pointer stored at 0xFF06 (16-bit LE).
 *   Entry for socket N starts at table_base + N*16.
 *   entry[0] = state byte
 *   entry[2] = RX byte count lo
 *   entry[3] = RX byte count hi
 *
 * Socket states (confirmed from m4ewenterm source):
 *   0 = IDLE / OK  (connected; also state after send completes)
 *   1 = connecting in progress
 *   2 = send in progress
 *   3 = remote closed connection
 */

#define C_NETSOCKET  0x4331
#define C_NETCONNECT 0x4332
#define C_NETCLOSE   0x4333
#define C_NETSEND    0x4334
#define C_NETRECV    0x4335

#define M4_SOCK_STATE_IDLE    0   /* connected / OK */
#define M4_SOCK_STATE_CONN    1   /* connecting */
#define M4_SOCK_STATE_SEND    2   /* send in progress */
#define M4_SOCK_STATE_CLOSED  3   /* remote closed */

/* Current allocated socket (1-4); 0 = none open */
static unsigned char m4_socket;

/* Select M4 ROM then return pointer to socket N's info entry. */
static unsigned char *m4_sock_info(unsigned char sock) {
    m4_select_rom();
    return (unsigned char *)(*(unsigned int *)0xFF06) + (unsigned int)sock * 16;
}

/* -------------------------------------------------------------------------
 * net.h API
 * -------------------------------------------------------------------------*/

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
    unsigned char *sock;
    /* C_NETCONNECT: socket, 4×IP, 2×port LE */
    m4_out(9);
    m4_out(0x32); m4_out(0x43);
    m4_out((unsigned int)m4_socket);
    m4_out((unsigned int)ip[0]);
    m4_out((unsigned int)ip[1]);
    m4_out((unsigned int)ip[2]);
    m4_out((unsigned int)ip[3]);
    m4_out((unsigned int)(port & 0xFF));
    m4_out((unsigned int)(port >> 8));
    m4_strobe();
    m4_wait();
    if (m4_resp()[3] == 0xFF) return -1;   /* command-level error */

    /* Poll socket state: 0=connected, 1=connecting, 3=closed/error */
    sock = m4_sock_info(m4_socket);
    timeout = 3000000UL;
    while (timeout--) {
        unsigned char state = sock[0];
        if (state == M4_SOCK_STATE_IDLE)   return 0;
        if (state == M4_SOCK_STATE_CLOSED) return -1;
    }
    return -1;
}

int net_send(const unsigned char *buf, unsigned int len) {
    unsigned char *sock;
    sock = m4_sock_info(m4_socket);
    while (len) {
        unsigned char chunk = (len > 250) ? 250 : (unsigned char)len;
        unsigned char i;
        /* C_NETSEND: socket, size-lo, size-hi, data (max 250 bytes/call) */
        m4_out((unsigned int)(5 + chunk));
        m4_out(0x34); m4_out(0x43);
        m4_out((unsigned int)m4_socket);
        m4_out((unsigned int)chunk);
        m4_out(0);
        for (i = 0; i < chunk; i++)
            m4_out((unsigned int)buf[i]);
        m4_strobe();
        /* Wait for send to complete: poll state until != 2 (send in progress) */
        { unsigned int w = 60000U; while (sock[0] == M4_SOCK_STATE_SEND && w--) ; }
        buf += chunk;
        len -= chunk;
    }
    return 0;
}

unsigned int net_recv(unsigned char *buf, unsigned int maxlen) {
    unsigned char *sock, *resp;
    unsigned int recv_len, i;
    sock = m4_sock_info(m4_socket);
    /* Skip the command if connected and RX counter says empty.
     * When state is CLOSED, always try C_NETRECV — the M4 may clear sock[2:3]
     * when the FIN arrives even if data was buffered just before it. */
    if (sock[0] != M4_SOCK_STATE_CLOSED && !sock[2] && !sock[3]) return 0;
    if (maxlen > 2048) maxlen = 2048;
    /* C_NETRECV: socket, requested-size LE */
    m4_out(5);
    m4_out(0x35); m4_out(0x43);
    m4_out((unsigned int)m4_socket);
    m4_out((unsigned int)(maxlen & 0xFF));
    m4_out((unsigned int)(maxlen >> 8));
    m4_strobe();
    m4_wait();
    resp = m4_resp();
    if (resp[3] != 0) return 0;    /* error status */
    recv_len = (unsigned int)resp[4] | ((unsigned int)resp[5] << 8);
    if (!recv_len) return 0;
    if (recv_len > maxlen) recv_len = maxlen;
    for (i = 0; i < recv_len; i++)
        buf[i] = resp[6 + i];
    return recv_len;
}

void net_close(void) {
    m4_out(3);
    m4_out(0x33); m4_out(0x43);
    m4_out((unsigned int)m4_socket);
    m4_strobe();
    m4_wait();
    m4_socket = 0;
}

unsigned char net_is_connected(void) {
    if (!m4_socket) return 0;
    return (m4_sock_info(m4_socket)[0] != M4_SOCK_STATE_CLOSED) ? 1 : 0;
}

unsigned int net_rx_available(void) {
    unsigned char *sock;
    if (!m4_socket) return 0;
    sock = m4_sock_info(m4_socket);
    /* Do NOT return 0 on CLOSED — remote may have sent data before closing.
     * The loop caller handles CLOSED via net_is_connected(); we just report bytes. */
    return (unsigned int)sock[2] | ((unsigned int)sock[3] << 8);
}
