#include "../../src/cpcbios.h"
#include "../../src/netinit.h"
#include "../../src/dns.h"
#include "../../src/udp.h"
#ifndef NET_M4
#include "../../src/w5100.h"
#endif

#define NTP_PORT     123
#define NTP_PKT_SIZE 48
#define NTP_TIMEOUT  3000   /* ms */

/* NTP epoch offset: seconds from 1900-01-01 to 1970-01-01 */
#define NTP_EPOCH_OFFSET 2208988800UL

static const char ntp_host[] = "time.cloudflare.com";

static unsigned char ntp_packet[NTP_PKT_SIZE];
static unsigned char ntp_reply[NTP_PKT_SIZE];

/* -------------------------------------------------------------------------
 * Print helpers
 * -------------------------------------------------------------------------*/

static void print_uint(unsigned int n) {
    static const unsigned int powers[5] = { 10000, 1000, 100, 10, 1 };
    unsigned char i, d, leading = 1;
    for (i = 0; i < 5; i++) {
        for (d = 0; n >= powers[i]; d++) n -= powers[i];
        if (d || !leading || i == 4) { cpc_print_char('0' + d); leading = 0; }
    }
}

/* 2-digit decimal with leading zero — for HH, MM, SS, DD, MM */
static void print_dec2(unsigned char n) {
    unsigned char tens = 0;
    while (n >= 10) { n -= 10; tens++; }
    cpc_print_char('0' + tens);
    cpc_print_char('0' + n);
}

static void print_ip(const unsigned char *ip) {
    print_uint(ip[0]); cpc_print_char('.');
    print_uint(ip[1]); cpc_print_char('.');
    print_uint(ip[2]); cpc_print_char('.');
    print_uint(ip[3]);
}

/* -------------------------------------------------------------------------
 * Date/time conversion: Unix timestamp -> calendar fields
 * -------------------------------------------------------------------------*/

static unsigned char is_leap(unsigned int y) {
    if (y % 4 != 0) return 0;
    if (y == 2100)  return 0;
    return 1;
}

/* Divide 32-bit t by small divisor d; return remainder, t becomes quotient.
 * Uses subtraction — only suitable for small divisors (≤ 60). */
static unsigned char divmod32(unsigned long *t, unsigned char d) {
    unsigned long q = 0;
    unsigned long tmp = *t;
    unsigned long step = (unsigned long)d;
    /* Find highest bit position of tmp relative to d */
    unsigned long cur = step;
    unsigned long qbit = 1;
    while (cur <= tmp >> 1) { cur <<= 1; qbit <<= 1; }
    while (qbit) {
        if (tmp >= cur) { tmp -= cur; q += qbit; }
        cur >>= 1; qbit >>= 1;
    }
    *t = q;
    return (unsigned char)tmp;
}

static void unix_to_datetime(unsigned long t,
    unsigned int *year, unsigned char *month, unsigned char *day,
    unsigned char *hour, unsigned char *min,  unsigned char *sec) {

    static const unsigned char mdays[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    unsigned int days;
    unsigned int y;
    unsigned char m, md;

    *sec  = divmod32(&t, 60);
    *min  = divmod32(&t, 60);
    *hour = divmod32(&t, 24);
    days  = (unsigned int)t;

    for (y = 1970; ; y++) {
        unsigned int yd = is_leap(y) ? 366 : 365;
        if (days < yd) break;
        days -= yd;
    }
    *year = y;

    for (m = 0; m < 12; m++) {
        md = mdays[m];
        if (m == 1 && is_leap(y)) md = 29;
        if (days < md) break;
        days -= md;
    }
    *month = m + 1;
    *day   = (unsigned char)(days + 1);
}

/* -------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------*/

void main(void) {
    unsigned char dns_server[4];
    unsigned char ntp_ip[4];
    unsigned long ntp_secs;
    unsigned int  t0;
    unsigned int  received;
    int rc;

    unsigned int  year;
    unsigned char month, day, hour, min, sec;

    cpc_set_mode(1);
    cpc_cls();
    cpc_print("NTP Client for CPC / Net4CPC\r\n");
    cpc_print("==============================\r\n");

    /* Step 1: network init */
    cpc_print("Initialising network...");
    rc = net_init_from_file();
    if (rc == -1) { cpc_print("file not found\r\n"); goto done; }
    if (rc == -2) { cpc_print("no chip\r\n");        goto done; }
    cpc_print(" OK\r\n");

    /* Step 2: DNS resolve NTP host */
    cpc_print("Resolving: ");
    cpc_print(ntp_host);
    cpc_print("\r\n");

#ifdef NET_M4
    dns_server[0] = dns_server[1] = dns_server[2] = dns_server[3] = 0;
#else
    dns_server[0] = w5100_read_reg(N_DNS0);
    dns_server[1] = w5100_read_reg(N_DNS0 + 1);
    dns_server[2] = w5100_read_reg(N_DNS0 + 2);
    dns_server[3] = w5100_read_reg(N_DNS0 + 3);
#endif

    rc = dns_resolve(dns_server, ntp_host, ntp_ip);
    if (rc != 0) {
        cpc_print("ERROR: DNS rc=");
        print_uint((unsigned int)(rc < 0 ? (unsigned int)(-rc) : (unsigned int)rc));
#ifdef NET_M4
        cpc_print(" resp3="); print_uint(dns_diag_resp3);
        cpc_print(" sock0="); print_uint(dns_diag_sock0);
#endif
        cpc_print("\r\n");
        goto done;
    }

    cpc_print("NTP server IP: ");
    print_ip(ntp_ip);
    cpc_print("\r\n");

    /* Step 3: open UDP socket (source port 12300) */
    if (udp_open(12300)) {
        cpc_print("ERROR: Socket failed\r\n");
        goto done;
    }

    /* Step 4: build and send 48-byte SNTPv4 request
     * Byte 0: LI=0, VN=4, Mode=3 → 0x23; rest zeros */
    {
        unsigned char i;
        for (i = 0; i < NTP_PKT_SIZE; i++) ntp_packet[i] = 0;
    }
    ntp_packet[0] = 0x23;

    cpc_print("Sending NTP request...\r\n");
    if (udp_sendto(ntp_ip, NTP_PORT, ntp_packet, NTP_PKT_SIZE)) {
        cpc_print("ERROR: Send failed\r\n");
        udp_close();
        goto done;
    }

    /* Step 5: wait up to 3 seconds for reply */
    cpc_print("Waiting for reply...\r\n");
    t0 = cpc_time_ms();
    while ((unsigned int)(cpc_time_ms() - t0) < NTP_TIMEOUT) {
        if (udp_rx_available()) goto recv;
    }
    cpc_print("ERROR: Timeout - no NTP reply\r\n");
    udp_close();
    goto done;

recv:
    /* Step 6: receive reply — udp_recv strips the 8-byte W5100S UDP header */
    received = udp_recv(ntp_reply, NTP_PKT_SIZE);
    udp_close();

    if (received < NTP_PKT_SIZE) {
        cpc_print("ERROR: Short reply\r\n");
        goto done;
    }

    /* Step 7: validate mode (bits 0-2 must be 4 or 5) */
    {
        unsigned char mode = ntp_reply[0] & 0x07;
        if (mode != 4 && mode != 5) {
            cpc_print("ERROR: Not an NTP server reply\r\n");
            goto done;
        }
    }

    /* Step 8: validate stratum (0 = Kiss-o'-Death) */
    if (ntp_reply[1] == 0) {
        cpc_print("ERROR: Kiss-o-Death packet received\r\n");
        goto done;
    }

    /* Step 9: extract Transmit Timestamp (bytes 40-43, big-endian NTP seconds) */
    ntp_secs  = (unsigned long)ntp_reply[40] << 24;
    ntp_secs |= (unsigned long)ntp_reply[41] << 16;
    ntp_secs |= (unsigned long)ntp_reply[42] <<  8;
    ntp_secs |= (unsigned long)ntp_reply[43];

    /* Step 10: NTP epoch → Unix epoch */
    ntp_secs -= NTP_EPOCH_OFFSET;

    /* Step 11: convert to calendar date/time */
    unix_to_datetime(ntp_secs, &year, &month, &day, &hour, &min, &sec);

    /* Step 12: display */
    cpc_print("UTC time: ");
    print_uint(year);
    cpc_print_char('-');
    print_dec2(month);
    cpc_print_char('-');
    print_dec2(day);
    cpc_print_char(' ');
    print_dec2(hour);
    cpc_print_char(':');
    print_dec2(min);
    cpc_print_char(':');
    print_dec2(sec);
    cpc_print(" UTC\r\n");

done:
    cpc_print("Press any key.\r\n");
    cpc_wait_key();
}
