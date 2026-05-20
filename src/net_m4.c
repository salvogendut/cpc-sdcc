#include "net.h"
#include "m4io.h"

/*
 * M4 Board TCP driver.
 *
 * ROM/RAM rule: after a strobe the M4 switches to RAM mode.  Calling
 * KL_ROM_SELECT (via m4_select_rom / m4_refresh) AFTER a strobe forces the
 * M4 back to ROM mode, returning stale static bytes instead of live data.
 *
 * Correct sequence for every command:
 *   1. m4_refresh() — pre-select ROM; cache resp and sock-table pointers.
 *   2. Send command bytes via m4_out().
 *   3. m4_strobe() — M4 processes, switches to RAM mode.
 *   4. m4_wait().
 *   5. Read resp / sock data using CACHED pointers — NO further m4_refresh().
 *   6. m4_select_basic() — restore BASIC ROM so BASIC ISRs can access it safely.
 *
 * m4_last_sock_state is captured inside net_recv() at step 5 (M4 in RAM mode).
 * net_is_connected() returns this cached value so callers need not re-enter
 * M4 ROM mode just to check the connection state.
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

static unsigned char  m4_socket;
static unsigned char *m4_resp_buf;   /* cached *(uint16_t*)0xFF02 */
static unsigned char *m4_sock_base;  /* cached *(uint16_t*)0xFF06 */

/* Socket state captured by net_recv() while M4 is in RAM mode.
 * Used by net_is_connected() so it never has to re-enter M4 ROM mode. */
static unsigned char m4_last_sock_state;

static void m4_refresh(void) {
    m4_select_rom();
    m4_resp_buf  = (unsigned char *)(*(unsigned int *)0xFF02);
    m4_sock_base = (unsigned char *)(*(unsigned int *)0xFF06);
}

static unsigned char *sock_entry(unsigned char s) {
    return m4_sock_base + (unsigned int)s * 16;
}

int net_socket_open(void) {
    m4_refresh();
    /* C_NETSOCKET: domain=0, type=0, protocol=6 (TCP) */
    m4_out(5);
    m4_out(0x31); m4_out(0x43);
    m4_out(0); m4_out(0); m4_out(6);
    m4_strobe();
    m4_wait();
    /* M4 now in RAM mode — read resp without re-selecting. */
    m4_socket = m4_resp_buf[3];
    m4_last_sock_state = M4_SOCK_STATE_IDLE;
    m4_select_basic();
    return (m4_socket == 0xFF) ? -1 : 0;
}

int net_listen(unsigned int port) {
    (void)port;
    return -1;
}

int net_connect(const unsigned char *ip, unsigned int port) {
    unsigned long timeout;
    unsigned char *sock;

    m4_refresh();
    sock = sock_entry(m4_socket);   /* pointer from ROM; data live after strobe */

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

    /* M4 now in RAM mode — check resp then poll sock WITHOUT re-selecting. */
    if (m4_resp_buf[3] == 0xFF) { m4_select_basic(); return -1; }

    /* Poll until state leaves "connecting in progress" (1).
     * Timeout ~2 s at 4 MHz (each iteration ~40 cycles). */
    timeout = 200000UL;
    while (timeout--) {
        unsigned char state = sock[0];
        if (state != M4_SOCK_STATE_CONN) {
            m4_last_sock_state = state;
            m4_select_basic();
            if (state == M4_SOCK_STATE_IDLE) return 0;
            return -1;
        }
    }
    m4_select_basic();
    return -1;
}

int net_send(const unsigned char *buf, unsigned int len) {
    unsigned char *sock;

    m4_refresh();
    sock = sock_entry(m4_socket);   /* pointer from ROM; data live after strobe */

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
        /* M4 now in RAM mode — poll until send finishes, no re-select. */
        { unsigned int w = 60000U; while (sock[0] == M4_SOCK_STATE_SEND && w--) ; }
        buf += chunk;
        len -= chunk;
    }
    m4_select_basic();
    return 0;
}

unsigned int net_recv(unsigned char *buf, unsigned int maxlen) {
    unsigned char *sock;
    unsigned int recv_len, i;

    if (maxlen > 2048) maxlen = 2048;

    m4_refresh();
    sock = sock_entry(m4_socket);

    /* C_NETRECV: socket, requested-size LE */
    m4_out(5);
    m4_out(0x35); m4_out(0x43);
    m4_out((unsigned int)m4_socket);
    m4_out((unsigned int)(maxlen & 0xFF));
    m4_out((unsigned int)(maxlen >> 8));
    m4_strobe();
    m4_wait();

    /* M4 now in RAM mode — capture socket state for net_is_connected(), then
     * read response data.  Must not call m4_select_basic() until after this. */
    m4_last_sock_state = sock[0];

    if (m4_resp_buf[3] != 0) { m4_select_basic(); return 0; }
    recv_len = (unsigned int)m4_resp_buf[4] | ((unsigned int)m4_resp_buf[5] << 8);
    if (!recv_len) { m4_select_basic(); return 0; }
    if (recv_len > maxlen) recv_len = maxlen;
    for (i = 0; i < recv_len; i++)
        buf[i] = m4_resp_buf[6 + i];

    m4_select_basic();
    return recv_len;
}

void net_close(void) {
    m4_out(3);
    m4_out(0x33); m4_out(0x43);
    m4_out((unsigned int)m4_socket);
    m4_strobe();
    m4_wait();
    m4_socket = 0;
    m4_last_sock_state = M4_SOCK_STATE_CLOSED;
    m4_select_basic();
}

/* net_is_connected: returns the socket state captured by the last net_recv()
 * call (while M4 was in RAM mode).  Does NOT re-enter M4 ROM mode, so it is
 * safe to call after screen writes that have called romen() / KL_ROM_RESTORE. */
unsigned char net_is_connected(void) {
    if (!m4_socket) return 0;
    return (m4_last_sock_state != M4_SOCK_STATE_CLOSED) ? 1 : 0;
}

unsigned int net_rx_available(void) {
    unsigned char *sock;
    if (!m4_socket) return 0;
    sock = sock_entry(m4_socket);
    return (unsigned int)sock[2] | ((unsigned int)sock[3] << 8);
}
