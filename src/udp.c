#include "udp.h"
#include "w5100.h"

static unsigned int read16(unsigned int reg) {
    unsigned int hi = w5100_read_reg(reg);
    return (hi << 8) | w5100_read_reg(reg + 1);
}

static void write16(unsigned int reg, unsigned int val) {
    w5100_write_reg(reg,     (val >> 8) & 0xFF);
    w5100_write_reg(reg + 1, val & 0xFF);
}

static void wait_cmd(void) {
    while (w5100_read_reg(S1_CR))
        ;
}

int udp_open(unsigned int src_port) {
    w5100_write_reg(S1_CR, SCMD_CLOSE);
    wait_cmd();
    w5100_write_reg(S1_MR, SMODE_UDP);
    write16(S1_PORT0, src_port);
    w5100_write_reg(S1_CR, SCMD_OPEN);
    wait_cmd();
    if (w5100_read_reg(S1_SR) != SSTAT_UDP)
        return -1;
    return 0;
}

int udp_sendto(const unsigned char *dst_ip, unsigned int dst_port,
               const unsigned char *buf, unsigned int len) {
    unsigned int wr, phys, space, first;

    /* Set destination IP and port */
    w5100_write_reg(S1_DIPR0,     dst_ip[0]);
    w5100_write_reg(S1_DIPR0 + 1, dst_ip[1]);
    w5100_write_reg(S1_DIPR0 + 2, dst_ip[2]);
    w5100_write_reg(S1_DIPR0 + 3, dst_ip[3]);
    write16(S1_DPORT0, dst_port);

    /* Wait for TX free space */
    space = read16(S1_TX_FSR0);
    if (space < len)
        return -1;

    wr   = read16(S1_TX_WR0);
    phys = S1_TX_BASE + (wr & S1_TX_MASK);

    if (phys + len > S1_TX_BASE + S1_TX_MASK + 1) {
        /* Data straddles the ring buffer boundary */
        first = (S1_TX_BASE + S1_TX_MASK + 1) - phys;
        w5100_write_buf(phys, buf, first);
        w5100_write_buf(S1_TX_BASE, buf + first, len - first);
    } else {
        w5100_write_buf(phys, buf, len);
    }

    write16(S1_TX_WR0, wr + len);
    w5100_write_reg(S1_CR, SCMD_SEND);
    wait_cmd();
    return 0;
}

unsigned int udp_rx_available(void) {
    return read16(S1_RX_RSR0);
}

unsigned int udp_recv(unsigned char *buf, unsigned int maxlen) {
    unsigned int rd, phys, total, data_len, first;
    unsigned char hdr[8];
    unsigned int i;

    total = read16(S1_RX_RSR0);
    if (total < 8)
        return 0;

    rd   = read16(S1_RX_RD0);
    phys = S1_RX_BASE + (rd & S1_RX_MASK);

    /* Read the 8-byte W5100S UDP header byte-by-byte to handle wrap */
    for (i = 0; i < 8; i++) {
        hdr[i] = w5100_read_reg(phys);
        phys++;
        if (phys >= S1_RX_BASE + S1_RX_MASK + 1)
            phys = S1_RX_BASE;
    }

    /* Bytes [6..7] of header = payload length (big-endian) */
    data_len = ((unsigned int)hdr[6] << 8) | hdr[7];

    if (data_len == 0 || data_len > maxlen) {
        /* Advance past the whole datagram and issue RECV to discard */
        write16(S1_RX_RD0, rd + 8 + data_len);
        w5100_write_reg(S1_CR, SCMD_RECV);
        wait_cmd();
        return 0;
    }

    /* Read payload with wrap handling */
    if (phys + data_len > S1_RX_BASE + S1_RX_MASK + 1) {
        first = (S1_RX_BASE + S1_RX_MASK + 1) - phys;
        w5100_read_buf(phys, buf, first);
        w5100_read_buf(S1_RX_BASE, buf + first, data_len - first);
    } else {
        w5100_read_buf(phys, buf, data_len);
    }

    write16(S1_RX_RD0, rd + 8 + data_len);
    w5100_write_reg(S1_CR, SCMD_RECV);
    wait_cmd();
    return data_len;
}

void udp_close(void) {
    w5100_write_reg(S1_CR, SCMD_CLOSE);
    wait_cmd();
}
