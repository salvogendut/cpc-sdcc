#include "dns.h"
#include "m4io.h"

/*
 * M4 DNS via C_NETHOSTIP (0x4336).
 * dns_server_ip is ignored — M4 uses its own configured DNS server.
 *
 * Request:  command(2) + NUL-terminated hostname
 * Response: 4 IP bytes at resp[3..6]
 *
 * TODO: verify response layout on real hardware.
 */

int dns_resolve(const unsigned char *dns_server_ip, const char *hostname,
                unsigned char *result_ip) {
    unsigned char len;
    unsigned char *resp;
    const char *p;

    (void)dns_server_ip;

    len = 0;
    p = hostname;
    while (*p++) len++;

    /* Payload length: 2 (command) + len (hostname) + 1 (NUL) */
    m4_out((unsigned int)(2 + len + 1));
    m4_out(0x36); m4_out(0x43);         /* C_NETHOSTIP LE */
    p = hostname;
    while (*p)
        m4_out((unsigned int)(unsigned char)*p++);
    m4_out(0);                           /* NUL terminator */
    m4_strobe();
    m4_wait();

    resp = m4_resp();
    if (!resp[3] && !resp[4] && !resp[5] && !resp[6])
        return -4;

    result_ip[0] = resp[3];
    result_ip[1] = resp[4];
    result_ip[2] = resp[5];
    result_ip[3] = resp[6];
    return 0;
}
