#include "w5100.h"
#include "net.h"

/* -------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------*/

static void wait_cmd_done(void) {
    while (w5100_read_reg(S0_CR))
        ;
}

/* Read a 16-bit big-endian value from two consecutive W5100S registers. */
static unsigned int read16(unsigned int reg) {
    unsigned int hi = (unsigned int)w5100_read_reg(reg);
    return (hi << 8) | w5100_read_reg(reg + 1);
}

static void write16(unsigned int reg, unsigned int val) {
    w5100_write_reg(reg,     val >> 8);
    w5100_write_reg(reg + 1, val & 0xFF);
}

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------*/

int net_socket_open(void) {
    /* Clear any stale interrupt flags */
    w5100_write_reg(S0_IR, 0xFF);

    /* Set TCP mode */
    w5100_write_reg(S0_MR, SMODE_TCP);

    /* Source port 5000 (0x1388) */
    w5100_write_reg(S0_PORT0,     0x13);
    w5100_write_reg(S0_PORT0 + 1, 0x88);

    w5100_write_reg(S0_CR, SCMD_OPEN);
    wait_cmd_done();

    return (w5100_read_reg(S0_SR) == SSTAT_INIT) ? 0 : -1;
}

int net_connect(const unsigned char *ip, unsigned int port) {
    unsigned int timeout;

    /* Write destination IP */
    w5100_write_reg(S0_DIPR0,     (unsigned int)ip[0]);
    w5100_write_reg(S0_DIPR0 + 1, (unsigned int)ip[1]);
    w5100_write_reg(S0_DIPR0 + 2, (unsigned int)ip[2]);
    w5100_write_reg(S0_DIPR0 + 3, (unsigned int)ip[3]);

    /* Write destination port (big-endian) */
    write16(S0_DPORT0, port);

    w5100_write_reg(S0_CR, SCMD_CONNECT);
    wait_cmd_done();

    /* Poll for ESTABLISHED with a timeout (~5 seconds at 4 MHz) */
    timeout = 50000;
    while (timeout--) {
        unsigned char status = w5100_read_reg(S0_SR);
        if (status == SSTAT_ESTABLISHED)
            return 0;
        if (status == SSTAT_CLOSED || status == SSTAT_CLOSE_WAIT)
            return -1;
    }
    return -1;
}

int net_send(const unsigned char *buf, unsigned int len) {
    unsigned int wr, phys_start, buf_end;

    if (!len) return 0;

    wr = read16(S0_TX_WR0);
    phys_start = (wr & S0_TX_MASK) + S0_TX_BASE;
    buf_end    = S0_TX_BASE + S0_TX_MASK + 1;   /* 0x4800 */

    if (phys_start + len > buf_end) {
        /* Write wraps around the 2 KB TX ring buffer */
        unsigned int seg1 = buf_end - phys_start;
        w5100_write_buf(phys_start, buf,        seg1);
        w5100_write_buf(S0_TX_BASE, buf + seg1, len - seg1);
    } else {
        w5100_write_buf(phys_start, buf, len);
    }

    /* Advance write pointer and trigger send */
    write16(S0_TX_WR0, wr + len);
    w5100_write_reg(S0_CR, SCMD_SEND);
    wait_cmd_done();

    return 0;
}

unsigned int net_recv(unsigned char *buf, unsigned int maxlen) {
    unsigned int avail, actual, rd, phys_start, buf_end;

    avail = read16(S0_RX_RSR0);
    if (!avail) return 0;

    actual = (maxlen < avail) ? maxlen : avail;

    rd         = read16(S0_RX_RD0);
    phys_start = (rd & S0_RX_MASK) + S0_RX_BASE;
    buf_end    = S0_RX_BASE + S0_RX_MASK + 1;   /* 0x6800 */

    if (phys_start + actual > buf_end) {
        /* Data straddles the end of the 2 KB RX ring buffer */
        unsigned int seg1 = buf_end - phys_start;
        w5100_read_buf(phys_start,  buf,        seg1);
        w5100_read_buf(S0_RX_BASE,  buf + seg1, actual - seg1);
    } else {
        w5100_read_buf(phys_start, buf, actual);
    }

    /* Advance read pointer and issue RECV */
    write16(S0_RX_RD0, rd + actual);
    w5100_write_reg(S0_CR, SCMD_RECV);
    wait_cmd_done();

    return actual;
}

void net_close(void) {
    w5100_write_reg(S0_CR, SCMD_DISCON);
    wait_cmd_done();
    w5100_write_reg(S0_CR, SCMD_CLOSE);
    wait_cmd_done();
}

unsigned char net_is_connected(void) {
    unsigned char s = w5100_read_reg(S0_SR);
    return (s == SSTAT_ESTABLISHED) ? 1 : 0;
}

unsigned int net_rx_available(void) {
    return read16(S0_RX_RSR0);
}
