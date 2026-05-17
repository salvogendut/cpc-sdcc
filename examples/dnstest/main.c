#include "../../src/cpcbios.h"
#include "../../src/netinit.h"
#include "../../src/dns.h"

/* -----------------------------------------------------------------------
 * Edit these to match your network.
 * ----------------------------------------------------------------------- */
static const net_config_t cfg = {
    { 192, 168,   1, 100 },   /* ip      */
    { 255, 255, 255,   0 },   /* netmask */
    { 192, 168,   1,   1 },   /* gateway */
    {   8,   8,   8,   8 },   /* dns     */
    { 0x00, 0x08, 0xDC, 0x01, 0x02, 0x03 }  /* mac */
};

static const unsigned char dns_server[4] = { 8, 8, 8, 8 };
static const char hostname[] = "example.com";

/* ----------------------------------------------------------------------- */

static void print_byte_dec(unsigned char n) {
    /* subtraction-based decimal — avoids __divuint from stdlib */
    unsigned char d;
    unsigned char leading = 1;

    for (d = 0; n >= 100; d++) n -= 100;
    if (d) { cpc_print_char('0' + d); leading = 0; }

    for (d = 0; n >= 10; d++) n -= 10;
    if (d || !leading) { cpc_print_char('0' + d); leading = 0; }

    cpc_print_char('0' + n);
}

static void print_ip(const unsigned char *ip) {
    print_byte_dec(ip[0]); cpc_print_char('.');
    print_byte_dec(ip[1]); cpc_print_char('.');
    print_byte_dec(ip[2]); cpc_print_char('.');
    print_byte_dec(ip[3]);
}

void main(void) {
    unsigned char result[4];
    int rc;

    cpc_set_mode(1);
    cpc_cls();
    cpc_print("DNS test\r\n");

    cpc_print("Init net... ");
    if (net_init(&cfg)) {
        cpc_print("FAIL (no chip?)\r\n");
        return;
    }
    cpc_print("OK\r\n");

    cpc_print("Resolving: ");
    cpc_print(hostname);
    cpc_print("\r\n");

    rc = dns_resolve(dns_server, hostname, result);

    if (rc == 0) {
        cpc_print("Result: ");
        print_ip(result);
        cpc_print("\r\n");
    } else if (rc == -3) {
        cpc_print("TIMEOUT\r\n");
    } else {
        cpc_print("FAIL (");
        cpc_print_char('0' - rc);   /* -1..-4 -> '1'..'4' */
        cpc_print(")\r\n");
    }

    while (1) {}
}
