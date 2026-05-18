#include "w5100.h"
#include "net_multi.h"

static void wait_sock_cmd(unsigned char s) {
    while (w5100_read_reg(SREG(s, SO_CR)))
        ;
}

static unsigned int read16s(unsigned char s, unsigned int off) {
    unsigned int reg = SREG(s, off);
    return ((unsigned int)w5100_read_reg(reg) << 8) | w5100_read_reg(reg + 1);
}

static void write16s(unsigned char s, unsigned int off, unsigned int val) {
    unsigned int reg = SREG(s, off);
    w5100_write_reg(reg,     val >> 8);
    w5100_write_reg(reg + 1, val & 0xFF);
}

int tcp_listen_sock(unsigned char s, unsigned int port) {
    w5100_write_reg(SREG(s, SO_IR), 0xFF);
    w5100_write_reg(SREG(s, SO_MR), SMODE_TCP);
    w5100_write_reg(SREG(s, SO_PORT0),     (unsigned char)(port >> 8));
    w5100_write_reg(SREG(s, SO_PORT0 + 1), (unsigned char)(port & 0xFF));
    w5100_write_reg(SREG(s, SO_CR), SCMD_OPEN);
    wait_sock_cmd(s);
    if (w5100_read_reg(SREG(s, SO_SR)) != SSTAT_INIT) return -1;
    w5100_write_reg(SREG(s, SO_CR), SCMD_LISTEN);
    wait_sock_cmd(s);
    return (w5100_read_reg(SREG(s, SO_SR)) == SSTAT_LISTEN) ? 0 : -1;
}

unsigned char tcp_get_status_sock(unsigned char s) {
    return w5100_read_reg(SREG(s, SO_SR));
}

int tcp_send_sock(unsigned char s, const unsigned char *buf, unsigned int len) {
    unsigned int wr, phys_start, tx_base, buf_end;

    if (!len) return 0;

    tx_base    = STX_BASE(s);
    wr         = read16s(s, SO_TX_WR);
    phys_start = (wr & S_BUF_MASK) + tx_base;
    buf_end    = tx_base + S_BUF_MASK + 1u;

    if (phys_start + len > buf_end) {
        unsigned int seg1 = buf_end - phys_start;
        w5100_write_buf(phys_start, buf,        seg1);
        w5100_write_buf(tx_base,    buf + seg1, len - seg1);
    } else {
        w5100_write_buf(phys_start, buf, len);
    }

    write16s(s, SO_TX_WR, wr + len);
    w5100_write_reg(SREG(s, SO_CR), SCMD_SEND);
    wait_sock_cmd(s);
    return 0;
}

unsigned int tcp_recv_sock(unsigned char s, unsigned char *buf, unsigned int maxlen) {
    unsigned int avail, actual, rd, phys_start, rx_base, buf_end;

    avail = read16s(s, SO_RX_RSR);
    if (!avail) return 0;

    actual     = (maxlen < avail) ? maxlen : avail;
    rx_base    = SRX_BASE(s);
    rd         = read16s(s, SO_RX_RD);
    phys_start = (rd & S_BUF_MASK) + rx_base;
    buf_end    = rx_base + S_BUF_MASK + 1u;

    if (phys_start + actual > buf_end) {
        unsigned int seg1 = buf_end - phys_start;
        w5100_read_buf(phys_start, buf,        seg1);
        w5100_read_buf(rx_base,    buf + seg1, actual - seg1);
    } else {
        w5100_read_buf(phys_start, buf, actual);
    }

    write16s(s, SO_RX_RD, rd + actual);
    w5100_write_reg(SREG(s, SO_CR), SCMD_RECV);
    wait_sock_cmd(s);
    return actual;
}

unsigned int tcp_rx_available_sock(unsigned char s) {
    return read16s(s, SO_RX_RSR);
}

unsigned int tcp_tx_free_sock(unsigned char s) {
    return read16s(s, SO_TX_FSR);
}

void tcp_close_sock(unsigned char s) {
    w5100_write_reg(SREG(s, SO_CR), SCMD_DISCON);
    wait_sock_cmd(s);
    w5100_write_reg(SREG(s, SO_CR), SCMD_CLOSE);
    wait_sock_cmd(s);
}
