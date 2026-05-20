#include "dns.h"
#include "m4io.h"

/*
 * M4 DNS via C_NETHOSTIP (0x4336).
 *
 * Packet format:
 *   byte 0:    hostname_length + 3   (= length of remaining bytes)
 *   byte 1:    0x36 (C_NETHOSTIP LE)
 *   byte 2:    0x43
 *   bytes 3..: NUL-terminated hostname
 *
 * After the strobe the M4 switches its bank to RAM mode, writing the
 * response and socket data at the addresses pointed to by 0xFF02/0xFF06.
 * Calling KL_ROM_SELECT (m4_select_rom) AFTER the strobe forces the M4
 * back to ROM mode, making those addresses return stale ROM bytes.
 *
 * Correct sequence:
 *   1. m4_select_rom() — select slot, M4 in ROM mode; read 0xFF02/0xFF06
 *   2. send command bytes
 *   3. m4_strobe() — M4 processes, switches to RAM mode
 *   4. m4_wait()
 *   5. read resp/sock using pointers obtained in step 1, NO further
 *      m4_select_rom() calls (which would flip M4 back to ROM)
 */

#define M4_SOCK_STATE_DNS 5

unsigned char dns_diag_resp[8];   /* resp[0..7] */
unsigned char dns_diag_sock[8];   /* sock0[0..7] */

/* Legacy aliases */
unsigned char dns_diag_resp3;
unsigned char dns_diag_sock0;
unsigned char dns_diag_ip[4];

int dns_resolve(const unsigned char *dns_server_ip, const char *hostname,
                unsigned char *result_ip) {
    unsigned char len;
    unsigned char *resp, *sock0;
    const char *p;
    unsigned long timeout;
    unsigned char i;

    (void)dns_server_ip;

    len = 0;
    p = hostname;
    while (*p++) len++;

    /*
     * Select M4 ROM once to read the buffer/socket pointers, then send
     * the command.  Do not call m4_select_rom() again after the strobe.
     */
    m4_select_rom();
    resp  = (unsigned char *)(*(unsigned int *)0xFF02);
    sock0 = (unsigned char *)(*(unsigned int *)0xFF06);

    /* Send C_NETHOSTIP: [len+3, 0x36, 0x43, hostname, NUL] */
    m4_out((unsigned int)(len + 3));
    m4_out(0x36); m4_out(0x43);
    p = hostname;
    while (*p)
        m4_out((unsigned int)(unsigned char)*p++);
    m4_out(0);
    m4_strobe();
    m4_wait();

    /*
     * M4 is now in RAM mode.  Read resp[0] directly — any value other
     * than 0xFF means the command was accepted.
     */
    for (i = 0; i < 8; i++) dns_diag_resp[i] = resp[i];
    dns_diag_resp3 = resp[3];

    if (resp[0] == 0xFF)
        return -1;

    /* Poll sock0[0] until DNS state clears (state 5 = in progress). */
    timeout = 3000000UL;
    while (timeout--) {
        if (sock0[0] != M4_SOCK_STATE_DNS) break;
    }

    for (i = 0; i < 8; i++) dns_diag_sock[i] = sock0[i];
    dns_diag_sock0 = sock0[0];
    dns_diag_ip[0] = sock0[4]; dns_diag_ip[1] = sock0[5];
    dns_diag_ip[2] = sock0[6]; dns_diag_ip[3] = sock0[7];

    if (!timeout)      return -3;
    if (sock0[0] != 0) return -4;

    if (!sock0[4] && !sock0[5] && !sock0[6] && !sock0[7])
        return -4;

    result_ip[0] = sock0[4];
    result_ip[1] = sock0[5];
    result_ip[2] = sock0[6];
    result_ip[3] = sock0[7];
    return 0;
}
