#include "dns.h"
#include "udp.h"

/* CPC frame counter at 0xB5CB; increments at 50 Hz.
 * n_time() returns elapsed milliseconds (frames * 20). */
static unsigned int n_time(void) __naked {
    __asm
        ld      hl, (#0xB5CB)
        ld      d, h
        ld      e, l
        add     hl, hl      ; * 2
        add     hl, hl      ; * 4
        add     hl, de      ; * 5
        add     hl, hl      ; * 10
        add     hl, hl      ; * 20
        ret
    __endasm;
}

#define DNS_PORT    53
#define DNS_SRC_PORT 1053
#define DNS_TIMEOUT 3000    /* ms */
#define DNS_BUF_SZ  512

static unsigned char dns_qbuf[DNS_BUF_SZ];
static unsigned char dns_rbuf[DNS_BUF_SZ];

/* Encode hostname into DNS wire-format QNAME at dst.
 * "www.example.com" -> \x03www\x07example\x03com\x00
 * Returns pointer to byte after the trailing 0x00. */
static unsigned char *dns_encode_name(unsigned char *dst, const char *hostname) {
    const char *p = hostname;
    unsigned char *len_byte;

    while (*p) {
        len_byte = dst++;
        *len_byte = 0;
        while (*p && *p != '.') {
            *dst++ = (unsigned char)*p++;
            (*len_byte)++;
        }
        if (*p == '.')
            p++;
    }
    *dst++ = 0x00;  /* root label */
    return dst;
}

/* Build a DNS query into dns_qbuf for hostname.
 * Returns total query length. */
static unsigned int dns_build_query(const char *hostname) {
    unsigned char *p = dns_qbuf;

    /* Transaction ID = 0x1234 */
    *p++ = 0x12; *p++ = 0x34;
    /* Flags: standard query, recursion desired */
    *p++ = 0x01; *p++ = 0x00;
    /* QDCOUNT=1 */
    *p++ = 0x00; *p++ = 0x01;
    /* ANCOUNT=0, NSCOUNT=0, ARCOUNT=0 */
    *p++ = 0x00; *p++ = 0x00;
    *p++ = 0x00; *p++ = 0x00;
    *p++ = 0x00; *p++ = 0x00;

    p = dns_encode_name(p, hostname);

    /* QTYPE=A (1), QCLASS=IN (1) */
    *p++ = 0x00; *p++ = 0x01;
    *p++ = 0x00; *p++ = 0x01;

    return (unsigned int)(p - dns_qbuf);
}

/*
 * Advance past a DNS name (label chain or compression pointer).
 * Returns pointer to byte after the name.
 * base is the start of the full response buffer (for pointer bounds check).
 */
static const unsigned char *skip_name(const unsigned char *p,
                                       const unsigned char *base,
                                       const unsigned char *end) {
    while (p < end) {
        if (*p == 0x00) {
            return p + 1;
        }
        if ((*p & 0xC0) == 0xC0) {
            /* Compression pointer: 2 bytes total, then name is done */
            return p + 2;
        }
        p += (unsigned int)*p + 1;
    }
    (void)base;
    return end;  /* malformed — return end to signal error */
}

/*
 * Parse DNS response in dns_rbuf (len bytes).
 * Extracts first TYPE A answer and writes 4 bytes into result_ip.
 * Returns 0 on success, -4 on failure.
 */
static int dns_parse_response(unsigned int len, unsigned char *result_ip) {
    const unsigned char *p   = dns_rbuf;
    const unsigned char *end = dns_rbuf + len;
    unsigned int qdcount, ancount, i;
    unsigned int rtype, rdlen;

    if (len < 12)
        return -4;

    /* Check transaction ID matches */
    if (p[0] != 0x12 || p[1] != 0x34)
        return -4;

    /* Check QR=1 (response) and RCODE=0 (no error) */
    if (!(p[2] & 0x80))
        return -4;
    if ((p[3] & 0x0F) != 0)
        return -4;

    qdcount = ((unsigned int)p[4] << 8) | p[5];
    ancount = ((unsigned int)p[6] << 8) | p[7];

    if (ancount == 0)
        return -4;

    p += 12;

    /* Skip question section */
    for (i = 0; i < qdcount; i++) {
        p = skip_name(p, dns_rbuf, end);
        p += 4;  /* QTYPE + QCLASS */
        if (p > end)
            return -4;
    }

    /* Walk answer records */
    for (i = 0; i < ancount; i++) {
        if (p >= end)
            return -4;

        p = skip_name(p, dns_rbuf, end);
        if (p + 10 > end)
            return -4;

        rtype = ((unsigned int)p[0] << 8) | p[1];
        /* skip TYPE(2) CLASS(2) TTL(4) */
        rdlen = ((unsigned int)p[8] << 8) | p[9];
        p += 10;

        if (rtype == 1 && rdlen == 4) {
            /* TYPE A, 4-byte IPv4 */
            if (p + 4 > end)
                return -4;
            result_ip[0] = p[0];
            result_ip[1] = p[1];
            result_ip[2] = p[2];
            result_ip[3] = p[3];
            return 0;
        }

        p += rdlen;
    }

    return -4;
}

int dns_resolve(const unsigned char *dns_server_ip, const char *hostname,
                unsigned char *result_ip) {
    unsigned int qlen, rlen;
    unsigned int t0, now, elapsed;

    if (udp_open(DNS_SRC_PORT) != 0)
        return -1;

    qlen = dns_build_query(hostname);

    if (udp_sendto(dns_server_ip, DNS_PORT, dns_qbuf, qlen) != 0) {
        udp_close();
        return -2;
    }

    t0 = n_time();
    rlen = 0;

    for (;;) {
        now     = n_time();
        /* Handle the 16-bit millisecond counter wrapping (~65 seconds) */
        elapsed = (now >= t0) ? (now - t0) : (unsigned int)(now + (65535u - t0) + 1u);
        if (elapsed >= DNS_TIMEOUT) {
            udp_close();
            return -3;
        }
        if (udp_rx_available() >= 8) {
            rlen = udp_recv(dns_rbuf, DNS_BUF_SZ);
            if (rlen > 0)
                break;
        }
    }

    udp_close();
    return dns_parse_response(rlen, result_ip);
}
