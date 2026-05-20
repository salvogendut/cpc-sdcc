#include "../../src/cpcbios.h"
#include "../../src/netinit.h"
#include "../../src/dns.h"
#ifdef NET_M4
#include "../../src/net.h"
#else
#include "../../src/udp.h"
#include "../../src/w5100.h"
#endif

#define NTP_PORT     123
#define NTP_PKT_SIZE 48
#define HTTP_PORT    80

/* NTP epoch offset: seconds from 1900-01-01 to 1970-01-01 */
#define NTP_EPOCH_OFFSET 2208988800UL

static const char ntp_host[] = "example.com";

#ifdef NET_M4
static unsigned char http_buf[512];
static const char http_req[] =
    "GET / HTTP/1.0\r\nHost: example.com\r\nConnection: close\r\n\r\n";
#else
static unsigned char ntp_packet[NTP_PKT_SIZE];
static unsigned char ntp_reply[NTP_PKT_SIZE];
#endif

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
 * M4 path: parse HTTP Date header
 * Date: Www, DD Mon YYYY HH:MM:SS GMT
 * -------------------------------------------------------------------------*/
#ifdef NET_M4
static unsigned char parse_http_date(const unsigned char *buf, unsigned int len,
    unsigned int *year, unsigned char *month, unsigned char *day,
    unsigned char *hour, unsigned char *min, unsigned char *sec) {

    static const char mon_names[12][4] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    const unsigned char *p;
    unsigned int i;
    unsigned char m;

    /* Find "Date: " */
    for (i = 0; i + 25 <= len; i++) {
        if (buf[i]   == 'D' && buf[i+1] == 'a' && buf[i+2] == 't' &&
            buf[i+3] == 'e' && buf[i+4] == ':' && buf[i+5] == ' ')
            goto found;
    }
    return 0;
found:
    p = buf + i + 6;    /* now at "Www, DD Mon YYYY HH:MM:SS GMT" */
    p += 5;             /* skip "Www, " */

    /* Day (possibly space-padded by some servers) */
    if (*p == ' ') { *day = *(p+1) - '0'; }
    else           { *day = (*p - '0') * 10 + (*(p+1) - '0'); }
    p += 3;             /* skip "DD " */

    /* Month name */
    *month = 0;
    for (m = 0; m < 12; m++) {
        if (p[0] == (unsigned char)mon_names[m][0] &&
            p[1] == (unsigned char)mon_names[m][1] &&
            p[2] == (unsigned char)mon_names[m][2]) {
            *month = m + 1;
            break;
        }
    }
    p += 4;             /* skip "Mon " */

    /* Year */
    *year = (unsigned int)(*p     - '0') * 1000u +
            (unsigned int)(*(p+1) - '0') * 100u  +
            (unsigned int)(*(p+2) - '0') * 10u   +
            (unsigned int)(*(p+3) - '0');
    p += 5;             /* skip "YYYY " */

    /* HH:MM:SS */
    *hour = (*p - '0') * 10 + (*(p+1) - '0'); p += 3;
    *min  = (*p - '0') * 10 + (*(p+1) - '0'); p += 3;
    *sec  = (*p - '0') * 10 + (*(p+1) - '0');
    return 1;
}
#endif /* NET_M4 */

/* -------------------------------------------------------------------------
 * W5100S path: Unix timestamp -> calendar fields
 * -------------------------------------------------------------------------*/
#ifndef NET_M4
static unsigned char is_leap(unsigned int y) {
    if (y % 4 != 0) return 0;
    if (y == 2100)  return 0;
    return 1;
}

static unsigned char divmod32(unsigned long *t, unsigned char d) {
    unsigned long q = 0;
    unsigned long tmp = *t;
    unsigned long step = (unsigned long)d;
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
#endif /* !NET_M4 */

/* -------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------*/

void main(void) {
    unsigned char dns_server[4];
    unsigned char ntp_ip[4];
    int rc;

    unsigned int  year;
    unsigned char month, day, hour, min, sec;

#ifndef NET_M4
    unsigned long ntp_secs;
    unsigned int  received;
#endif

    cpc_set_mode(1);
    cpc_cls();
    cpc_print("NTP Client for CPC\r\n");
    cpc_print("==================\r\n");

    /* Step 1: network init */
    cpc_print("Initialising network...");
    rc = net_init_from_file();
    if (rc == -1) { cpc_print("file not found\r\n"); goto done; }
    if (rc == -2) { cpc_print("no chip\r\n");        goto done; }
    cpc_print(" OK\r\n");

    /* Step 2: DNS resolve */
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
        cpc_print("ERROR: DNS failed\r\n");
        goto done;
    }

    cpc_print("Server IP: ");
    print_ip(ntp_ip);
    cpc_print("\r\n");

#ifdef NET_M4
    /* -----------------------------------------------------------------------
     * M4 path: HTTP GET to time.akamai.com:80, parse Date: header
     * -----------------------------------------------------------------------*/

    /* Step 3: open TCP socket */
    if (net_socket_open()) {
        cpc_print("ERROR: Socket open failed\r\n");
        goto done;
    }

    /* Step 4: connect */
    cpc_print("Connecting to port 80...\r\n");
    if (net_connect(ntp_ip, HTTP_PORT)) {
        cpc_print("ERROR: Connect failed\r\n");
        net_close();
        goto done;
    }
    cpc_print("Connected\r\n");

    /* Step 5: send HTTP GET */
    cpc_print("Sending request...\r\n");
    net_send((const unsigned char *)http_req,
             (unsigned int)(sizeof(http_req) - 1));

    /* Step 6: receive response into http_buf */
    cpc_print("Waiting for reply...\r\n");
    {
        unsigned int total = 0;
        unsigned int attempts;
        for (attempts = 0; attempts < 200; attempts++) {
            unsigned int n = net_recv(http_buf + total,
                                      (unsigned int)(sizeof(http_buf) - 1) - total);
            if (n) {
                total += n;
                if (total >= 256) break;   /* enough to have all headers */
            }
        }
        net_close();

        if (!total) {
            cpc_print("ERROR: No reply\r\n");
            goto done;
        }

        /* Step 7: parse Date header */
        if (!parse_http_date(http_buf, total,
                             &year, &month, &day, &hour, &min, &sec)) {
            cpc_print("ERROR: No Date header\r\n");
            goto done;
        }
    }

#else
    /* -----------------------------------------------------------------------
     * W5100S path: UDP NTP
     * -----------------------------------------------------------------------*/

    /* Step 3: open UDP socket */
    if (udp_open(12300)) {
        cpc_print("ERROR: Socket failed\r\n");
        goto done;
    }

    /* Step 4: build and send NTP request */
    {
        unsigned char i;
        for (i = 0; i < NTP_PKT_SIZE; i++) ntp_packet[i] = 0;
    }
    ntp_packet[0] = 0x23;   /* LI=0, VN=4, Mode=3 */

    cpc_print("Sending NTP request...\r\n");
    if (udp_sendto(ntp_ip, NTP_PORT, ntp_packet, NTP_PKT_SIZE)) {
        cpc_print("ERROR: Send failed\r\n");
        udp_close();
        goto done;
    }

    /* Step 5: poll for reply */
    cpc_print("Waiting for reply...\r\n");
    received = 0;
    {
        unsigned int attempts;
        for (attempts = 0; attempts < 100; attempts++) {
            received = udp_recv(ntp_reply, NTP_PKT_SIZE);
            if (received) goto recv;
        }
    }
    cpc_print("ERROR: Timeout - no NTP reply\r\n");
    udp_close();
    goto done;

recv:
    udp_close();

    if (received < NTP_PKT_SIZE) {
        cpc_print("ERROR: Short reply\r\n");
        goto done;
    }

    /* Step 6: validate */
    {
        unsigned char mode = ntp_reply[0] & 0x07;
        if (mode != 4 && mode != 5) {
            cpc_print("ERROR: Not an NTP server reply\r\n");
            goto done;
        }
    }
    if (ntp_reply[1] == 0) {
        cpc_print("ERROR: Kiss-o-Death packet received\r\n");
        goto done;
    }

    /* Step 7: extract Transmit Timestamp (bytes 40-43, big-endian) */
    ntp_secs  = (unsigned long)ntp_reply[40] << 24;
    ntp_secs |= (unsigned long)ntp_reply[41] << 16;
    ntp_secs |= (unsigned long)ntp_reply[42] <<  8;
    ntp_secs |= (unsigned long)ntp_reply[43];

    /* NTP epoch -> Unix epoch */
    ntp_secs -= NTP_EPOCH_OFFSET;

    unix_to_datetime(ntp_secs, &year, &month, &day, &hour, &min, &sec);
#endif /* NET_M4 */

    /* Display */
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
