#include "dns.h"
#include "m4io.h"

/*
 * M4 DNS via C_NETHOSTIP (0x4336) — verified against m4ewenterm/src/urlmenu.s.
 *
 * dns_server_ip is ignored — M4 uses its own configured DNS server.
 *
 * Protocol:
 *   Send C_NETHOSTIP with NUL-terminated hostname.
 *   resp[3] == 1  means lookup started successfully.
 *   Socket 0 info is at *(uint16_t*)0xFF06 (the table base, no N*16 offset).
 *   Poll socket 0 state until != 5 (DNS in progress).
 *   Resolved IP is at socket_0_info[4..7].
 */

#define C_NETHOSTIP           0x4336
#define M4_SOCK_STATE_DNS     5   /* DNS lookup in progress */

int dns_resolve(const unsigned char *dns_server_ip, const char *hostname,
                unsigned char *result_ip) {
    unsigned char len;
    unsigned char *resp, *sock0;
    const char *p;
    unsigned long timeout;

    (void)dns_server_ip;

    len = 0;
    p = hostname;
    while (*p++) len++;

    /* C_NETHOSTIP payload: command(2) + hostname(len) + NUL(1) */
    m4_out((unsigned int)(2 + len + 1));
    m4_out(0x36); m4_out(0x43);         /* C_NETHOSTIP LE */
    p = hostname;
    while (*p)
        m4_out((unsigned int)(unsigned char)*p++);
    m4_out(0);                           /* NUL terminator */
    m4_strobe();
    m4_wait();

    resp = m4_resp();
    if (resp[3] != 1) return -1;         /* 1 = lookup started; anything else = error */

    /* Socket 0 info starts at the socket table base (no N*16 offset for socket 0) */
    sock0 = (unsigned char *)(*(unsigned int *)0xFF06);

    /* Poll until DNS lookup completes (state leaves 5) */
    timeout = 3000000UL;
    while (timeout--) {
        if (sock0[0] != M4_SOCK_STATE_DNS) break;
    }
    if (!timeout) return -3;             /* timeout */

    if (sock0[0] != 0) return -4;        /* lookup failed */

    /* Resolved IP at socket 0 info offset 4 */
    result_ip[0] = sock0[4];
    result_ip[1] = sock0[5];
    result_ip[2] = sock0[6];
    result_ip[3] = sock0[7];
    return 0;
}
