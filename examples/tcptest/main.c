#include "../../src/cpcbios.h"
#include "../../src/netinit.h"
#include "../../src/net.h"
#ifdef NET_M4
/* Local gateway */
#define M4_TEST_IP0 192
#define M4_TEST_IP1 168
#define M4_TEST_IP2  68
#define M4_TEST_IP3   1
#else
#include "../../src/w5100.h"
#endif

#define SERVER_PORT 80

#ifdef NET_M4
/* HTTP/1.1: persistent connection (default). Server delivers data while
 * state=0, so sock[2:3] is populated before any FIN arrives.
 * We drain until idle then close ourselves. */
static const char http_request[] =
    "GET / HTTP/1.1\r\n"
    "Host: 192.168.68.1\r\n"
    "\r\n";
#else
static const char http_request[] =
    "GET / HTTP/1.0\r\n"
    "Connection: close\r\n"
    "\r\n";
#endif

static unsigned char rxbuf[256];

static void print_hex_byte(unsigned char b) {
    static const char hex[] = "0123456789ABCDEF";
    cpc_print_char(hex[b >> 4]);
    cpc_print_char(hex[b & 0x0F]);
}

static void print_uint(unsigned int n) {
    static const unsigned int powers[5] = { 10000, 1000, 100, 10, 1 };
    unsigned char i, d, leading = 1;
    for (i = 0; i < 5; i++) {
        for (d = 0; n >= powers[i]; d++) n -= powers[i];
        if (d || !leading || i == 4) { cpc_print_char('0' + d); leading = 0; }
    }
}

static void print_ip(const unsigned char *ip) {
    print_uint(ip[0]); cpc_print_char('.');
    print_uint(ip[1]); cpc_print_char('.');
    print_uint(ip[2]); cpc_print_char('.');
    print_uint(ip[3]);
}

void main(void) {
    unsigned char server_ip[4];
    unsigned int received, total;
#ifndef NET_M4
    int rc;
#endif

    cpc_set_mode(1);
    cpc_cls();
    cpc_print("TCP test\r\n");

#ifdef NET_M4
    cpc_print("M4 init... ");
    net_init_from_file();
    cpc_print("OK\r\n");
#else
    cpc_print("Reading N4C.CFG... ");
    rc = net_init_from_file();
    if (rc == -1) { cpc_print("file not found\r\n"); return; }
    if (rc == -2) { cpc_print("no chip\r\n"); return; }
    cpc_print("OK\r\n");
#endif

#ifdef NET_M4
    server_ip[0] = M4_TEST_IP0;
    server_ip[1] = M4_TEST_IP1;
    server_ip[2] = M4_TEST_IP2;
    server_ip[3] = M4_TEST_IP3;
    cpc_print("Target: "); print_ip(server_ip); cpc_print("\r\n");
#else
    server_ip[0] = w5100_read_reg(N_GAR0);
    server_ip[1] = w5100_read_reg(N_GAR0 + 1);
    server_ip[2] = w5100_read_reg(N_GAR0 + 2);
    server_ip[3] = w5100_read_reg(N_GAR0 + 3);

    {
        const unsigned char *p = (const unsigned char *)0x3F10;
        cpc_print("IP:   "); print_ip(p);      cpc_print("\r\n");
        cpc_print("MASK: "); print_ip(p + 4);  cpc_print("\r\n");
        cpc_print("GW:   "); print_ip(p + 8);  cpc_print("\r\n");
        cpc_print("DNS:  "); print_ip(p + 12); cpc_print("\r\n");
    }
#endif

    cpc_print("Open socket... ");
    if (net_socket_open()) {
        cpc_print("FAIL\r\n");
        return;
    }
    cpc_print("OK\r\n");

    cpc_print("Connecting... ");
    if (net_connect(server_ip, SERVER_PORT)) {
#ifndef NET_M4
        { unsigned char sr = w5100_read_reg(S0_SR);
          cpc_print("FAIL (SR=0x"); print_hex_byte(sr); cpc_print(")\r\n"); }
#else
        cpc_print("FAIL\r\n");
#endif
        net_close();
        return;
    }
    cpc_print("OK\r\n");

    cpc_print("Sending GET...\r\n");
    net_send((const unsigned char *)http_request,
             sizeof(http_request) - 1);

    cpc_print("Response:\r\n");
    total = 0;
#ifdef NET_M4
    /* HTTP/1.1 keep-alive: server holds connection open after sending.
     * Drain until idle (no new bytes for ~4000 empty polls), then close. */
    {
        unsigned int idle = 0;
        while (net_is_connected() && idle < 4000U) {
            received = net_recv(rxbuf, sizeof(rxbuf));
            if (received) {
                unsigned int i;
                for (i = 0; i < received; i++) {
                    unsigned char c = rxbuf[i];
                    if (c >= 0x20 || c == '\r' || c == '\n')
                        cpc_print_char(c);
                }
                total += received;
                idle = 0;
            } else {
                idle++;
            }
        }
    }
#else
    while (net_is_connected() || net_rx_available()) {
        received = net_recv(rxbuf, sizeof(rxbuf));
        if (received) {
            unsigned int i;
            for (i = 0; i < received; i++) {
                unsigned char c = rxbuf[i];
                if (c >= 0x20 || c == '\r' || c == '\n')
                    cpc_print_char(c);
            }
            total += received;
        }
    }
#endif

    cpc_print("\r\nDone. Bytes: ");
    print_uint(total);
    cpc_print("\r\n");

    net_close();
}
