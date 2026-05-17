#include "../../src/cpcbios.h"
#include "../../src/netinit.h"
#include "../../src/net.h"

/* Connect to this server and do an HTTP GET / */
static const unsigned char server_ip[4] = { 93, 184, 216, 34 }; /* example.com */
#define SERVER_PORT 80

static const char http_request[] =
    "GET / HTTP/1.0\r\n"
    "Host: example.com\r\n"
    "Connection: close\r\n"
    "\r\n";

static unsigned char rxbuf[256];

static void print_uint(unsigned int n) {
    static const unsigned int powers[5] = { 10000, 1000, 100, 10, 1 };
    unsigned char i, d, leading = 1;
    for (i = 0; i < 5; i++) {
        for (d = 0; n >= powers[i]; d++) n -= powers[i];
        if (d || !leading || i == 4) { cpc_print_char('0' + d); leading = 0; }
    }
}

void main(void) {
    unsigned int received, total;
    int rc;

    cpc_set_mode(1);
    cpc_cls();
    cpc_print("TCP test\r\n");

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

    cpc_print("Open socket... ");
    if (net_socket_open()) {
        cpc_print("FAIL\r\n");
        return;
    }
    cpc_print("OK\r\n");

    cpc_print("Connecting... ");
    if (net_connect(server_ip, SERVER_PORT)) {
        cpc_print("FAIL\r\n");
        net_close();
        return;
    }
    cpc_print("OK\r\n");

    cpc_print("Sending GET...\r\n");
    net_send((const unsigned char *)http_request,
             sizeof(http_request) - 1);

    cpc_print("Response:\r\n");
    total = 0;
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

    cpc_print("\r\nDone. Bytes: ");
    print_uint(total);
    cpc_print("\r\n");

    net_close();

    while (1) {}
}
