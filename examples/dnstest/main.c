#include "../../src/cpcbios.h"
#include "../../src/netinit.h"
#include "../../src/dns.h"
#include "../../src/w5100.h"

static const char hostname[] = "example.com";

static void print_byte_dec(unsigned char n) {
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
    unsigned char dns_server[4];
    unsigned char result[4];
    int rc;

    cpc_set_mode(1);
    cpc_cls();
    cpc_print("DNS test\r\n");

    cpc_print("Reading N4C.CFG... ");
    rc = net_init_from_file();
    if (rc == -1) {
        cpc_print("file not found\r\n");
        return;
    }
    if (rc == -2) {
        cpc_print("no chip\r\n");
        return;
    }
    cpc_print("OK\r\n");

    dns_server[0] = w5100_read_reg(N_DNS0);
    dns_server[1] = w5100_read_reg(N_DNS0 + 1);
    dns_server[2] = w5100_read_reg(N_DNS0 + 2);
    dns_server[3] = w5100_read_reg(N_DNS0 + 3);

    cpc_print("DNS: ");
    print_ip(dns_server);
    cpc_print("\r\n");

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
        cpc_print_char('0' - rc);
        cpc_print(")\r\n");
    }
}
