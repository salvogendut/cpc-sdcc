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

/* Diagnostic fields populated on every call. */
unsigned char dns_diag_resp3;
unsigned char dns_diag_sock0;
unsigned char dns_diag_ip[4];

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

    /* C_NETHOSTIP payload is fixed at 16 bytes (matching m4ewenterm cmdlookup).
     * Layout: 2 bytes command + up to 13 bytes hostname + NUL + zero-padding.
     * Hostnames longer than 13 characters are not supported. */
    m4_out(16);
    m4_out(0x36); m4_out(0x43);         /* C_NETHOSTIP LE */
    p = hostname;
    while (*p && len < 13)
        { m4_out((unsigned int)(unsigned char)*p++); len++; }
    /* NUL + zero-pad to fill the 14-byte hostname area */
    { unsigned char pad; for (pad = len; pad < 14; pad++) m4_out(0); }
    m4_strobe();
    m4_wait();

    resp = m4_resp();
    dns_diag_resp3 = resp[3];

    /* Socket 0 info base (no N*16 offset — C_NETHOSTIP always uses socket 0). */
    m4_select_rom();
    sock0 = (unsigned char *)(*(unsigned int *)0xFF06);

    if (resp[3] == 1) {
        /* Async path: lookup started, poll until state leaves 5 */
        timeout = 3000000UL;
        while (timeout--) {
            if (sock0[0] != M4_SOCK_STATE_DNS) break;
        }
        dns_diag_sock0 = sock0[0];
        if (!timeout)      return -3;    /* timeout */
        if (sock0[0] != 0) return -4;    /* lookup failed */
    } else if (resp[3] == 0) {
        /* Sync path: some firmware versions return 0 (OK) with result already
         * in sock0[4..7] and sock0[0]==0 (IDLE). */
        dns_diag_sock0 = sock0[0];
        dns_diag_ip[0] = sock0[4]; dns_diag_ip[1] = sock0[5];
        dns_diag_ip[2] = sock0[6]; dns_diag_ip[3] = sock0[7];
        if (sock0[0] != 0) return -4;
    } else {
        return -1;                       /* unknown response */
    }

    /* Resolved IP at socket 0 info offset 4 */
    dns_diag_ip[0] = result_ip[0] = sock0[4];
    dns_diag_ip[1] = result_ip[1] = sock0[5];
    dns_diag_ip[2] = result_ip[2] = sock0[6];
    dns_diag_ip[3] = result_ip[3] = sock0[7];
    return 0;
}
