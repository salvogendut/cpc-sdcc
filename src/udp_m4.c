#include "udp.h"
#include "m4io.h"

/*
 * M4 Board UDP driver.
 *
 * The same ROM/RAM rule as dns_m4.c applies: calling KL_ROM_SELECT (via
 * m4_select_rom) AFTER a strobe forces the M4 back to ROM mode, so any
 * memory read at 0xC000–0xFFFF returns static ROM bytes instead of live
 * data.  The rule for every operation is:
 *
 *   1. m4_select_rom() — slot 7 selected; read any needed ROM pointers.
 *   2. Send command bytes via m4_out().
 *   3. m4_strobe() → M4 processes and switches to RAM mode.
 *   4. m4_wait().
 *   5. Read resp / socket data WITHOUT calling m4_select_rom() again.
 *
 * Polling strategy: udp_rx_available() is not reliable because checking
 * the socket table requires a ROM select (which defeats step 5). Instead,
 * the caller should poll udp_recv() directly in a timeout loop; each call
 * sends C_NETRECV, which triggers the M4 to check for data and write the
 * result to the response buffer in RAM mode.
 */

#define C_NETSOCKET  0x4331
#define C_NETCONNECT 0x4332
#define C_NETCLOSE   0x4333
#define C_NETSEND    0x4334
#define C_NETRECV    0x4335

static unsigned char  m4_udp_socket;
static unsigned char *m4_resp_buf;   /* cached response buffer pointer */

/* Diagnostics updated on every udp_recv() call */
unsigned char udp_diag_socket;
unsigned char udp_diag_resp3;
unsigned int  udp_diag_len;

int udp_open(unsigned int src_port) {
    (void)src_port;

    /* Read response buffer pointer from ROM, then send command. */
    m4_select_rom();
    m4_resp_buf = (unsigned char *)(*(unsigned int *)0xFF02);

    /* C_NETSOCKET: domain=0, type=2 (SOCK_DGRAM), protocol=17 (UDP) */
    m4_out(5);
    m4_out(0x31); m4_out(0x43);
    m4_out(0); m4_out(2); m4_out(17);
    m4_strobe();
    m4_wait();

    /* M4 now in RAM mode — read resp without re-selecting ROM. */
    m4_udp_socket = m4_resp_buf[3];
    udp_diag_socket = m4_udp_socket;
    return (m4_udp_socket == 0xFF) ? -1 : 0;
}

int udp_sendto(const unsigned char *dst_ip, unsigned int dst_port,
               const unsigned char *buf, unsigned int len) {
    unsigned char chunk, i;

    /* C_NETCONNECT: set destination IP and port for this socket. */
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

    /* C_NETSEND: send data. */
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

/*
 * udp_rx_available: not reliable with M4 ROM/RAM switching — always
 * returns 1 so the caller's loop can reach udp_recv() immediately.
 * Use udp_recv() in a timeout loop instead of polling here.
 */
unsigned int udp_rx_available(void) {
    return 1;
}

/*
 * udp_recv: send C_NETRECV to M4, wait, and read result from RAM buffer.
 * Returns 0 if no data arrived yet; returns byte count on success.
 * Intended to be called in a timeout loop from the caller.
 */
unsigned int udp_recv(unsigned char *buf, unsigned int maxlen) {
    unsigned int recv_len, i;

    if (maxlen > 2048) maxlen = 2048;

    /* Pre-select ROM to (re)confirm resp buffer pointer, then send command. */
    m4_select_rom();
    m4_resp_buf = (unsigned char *)(*(unsigned int *)0xFF02);

    m4_out(5);
    m4_out(0x35); m4_out(0x43);
    m4_out((unsigned int)m4_udp_socket);
    m4_out((unsigned int)(maxlen & 0xFF));
    m4_out((unsigned int)(maxlen >> 8));
    m4_strobe();
    m4_wait();

    /* M4 now in RAM mode — read response without re-selecting ROM. */
    udp_diag_resp3 = m4_resp_buf[3];
    recv_len = (unsigned int)m4_resp_buf[4] | ((unsigned int)m4_resp_buf[5] << 8);
    udp_diag_len = recv_len;
    if (m4_resp_buf[3] != 0) return 0;
    if (!recv_len) return 0;
    if (recv_len > maxlen) recv_len = maxlen;
    for (i = 0; i < recv_len; i++)
        buf[i] = m4_resp_buf[6 + i];
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
