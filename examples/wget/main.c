#include "../../src/cpcbios.h"
#include "../../src/amsdos.h"
#include "../../src/netinit.h"
#include "../../src/dns.h"
#include "../../src/net.h"
#include "../../src/w5100.h"

#define RECV_BUF_SIZE 512
#define REQ_BUF_SIZE  400

/*
 * Parameters written into fixed RAM by WGET.BAS before CALL &4000.
 * MEMORY &3DFF in the BASIC loader keeps BASIC's workspace below these.
 */
#define CFG_HOST     ((const char *)0x3E00)
#define CFG_PATH     ((const char *)0x3E80)
#define CFG_FNAME    ((const char *)0x3F00)
#define CFG_FLEN     (*(const unsigned char *)0x3F0B)
#define CFG_PORT_LO  (*(const unsigned char *)0x3F0C)
#define CFG_PORT_HI  (*(const unsigned char *)0x3F0D)

static unsigned char recv_buf[RECV_BUF_SIZE];
static char          req_buf[REQ_BUF_SIZE];

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

/* Parse "a.b.c.d\0" into ip[4]. */
static void parse_dotted_ip(const char *p, unsigned char *ip) {
    unsigned char i;
    for (i = 0; i < 4; i++) {
        unsigned int v = 0;
        while (*p >= '0' && *p <= '9')
            v = v * 10 + (*p++ - '0');
        ip[i] = (unsigned char)v;
        if (i < 3) p++;     /* skip '.' */
    }
}

static char *str_append(char *dst, const char *src) {
    while (*src) *dst++ = *src++;
    return dst;
}

void main(void) {
    unsigned char server_ip[4];
    unsigned char dns_server[4];
    unsigned int  port;
    unsigned int  byte_count;
    unsigned char hdr_done, hdr_state;
    unsigned char http_status, status_cnt;
    unsigned int  received;
    int rc;
    char *p;

    cpc_set_mode(1);
    cpc_cls();
    cpc_print("WGET for CPC / Net4CPC\r\n");
    cpc_print("============================\r\n");

    /* Step 1: initialise network from N4C.CFG */
    cpc_print("Initialising network...");
    rc = net_init_from_file();
    if (rc == -1) { cpc_print("file not found\r\n"); goto done; }
    if (rc == -2) { cpc_print("no chip\r\n"); goto done; }
    cpc_print(" OK\r\n");

    /* Step 2: resolve hostname or parse dotted IP */
    if (*CFG_HOST >= '0' && *CFG_HOST <= '9') {
        parse_dotted_ip(CFG_HOST, server_ip);
    } else {
        cpc_print("Resolving: ");
        cpc_print(CFG_HOST);
        cpc_print("\r\n");
        dns_server[0] = w5100_read_reg(N_DNS0);
        dns_server[1] = w5100_read_reg(N_DNS0 + 1);
        dns_server[2] = w5100_read_reg(N_DNS0 + 2);
        dns_server[3] = w5100_read_reg(N_DNS0 + 3);
        rc = dns_resolve(dns_server, CFG_HOST, server_ip);
        if (rc != 0) { cpc_print("ERROR: DNS resolution failed\r\n"); goto done; }
    }
    cpc_print("Server IP: ");
    print_ip(server_ip);
    cpc_print("\r\n");

    port = (unsigned int)CFG_PORT_LO | ((unsigned int)CFG_PORT_HI << 8);

    /* Step 3: open TCP socket */
    if (net_socket_open()) {
        cpc_print("ERROR: Could not open socket\r\n");
        goto done;
    }

    /* Step 4: connect */
    cpc_print("Connecting...");
    if (net_connect(server_ip, port)) {
        cpc_print(" FAIL\r\n");
        cpc_print("ERROR: Connection refused or timeout\r\n");
        net_close();
        goto done;
    }
    cpc_print(" OK\r\n");

    /* Step 5: build and send HTTP/1.0 GET request */
    p = req_buf;
    p = str_append(p, "GET ");
    p = str_append(p, CFG_PATH);
    p = str_append(p, " HTTP/1.0\r\nHost: ");
    p = str_append(p, CFG_HOST);
    p = str_append(p, "\r\nConnection: close\r\n\r\n");

    cpc_print("Sending GET request...\r\n");
    net_send((const unsigned char *)req_buf, (unsigned int)(p - req_buf));

    /* Step 6: open output file */
    if (!cas_out_open(CFG_FNAME, (int)CFG_FLEN)) {
        cpc_print("ERROR: Could not open output file\r\n");
        net_close();
        goto done;
    }

    cpc_print("Receiving");

    byte_count  = 0;
    hdr_done    = 0;
    hdr_state   = 0;
    http_status = 0;
    status_cnt  = 0;

    /* Step 7: receive loop — skip HTTP headers, write body to file */
    while (net_is_connected() || net_rx_available()) {
        received = net_recv(recv_buf, RECV_BUF_SIZE);
        if (!received) continue;

        {
            unsigned int i;
            for (i = 0; i < received; i++) {
                unsigned char c = recv_buf[i];

                /* Capture first digit of HTTP status (response byte index 9):
                   "HTTP/1.0 " is 9 bytes, then the first status digit. */
                if (status_cnt != 0xFF) {
                    if (status_cnt == 9) {
                        http_status = c;
                        status_cnt  = 0xFF;
                    } else {
                        status_cnt++;
                    }
                }

                if (hdr_done) {
                    /* Body: write to file */
                    if (!cas_out_char(c)) {
                        cpc_print("\r\nERROR: Disk write failed (full?)\r\n");
                        cas_out_abandon();
                        net_close();
                        goto done;
                    }
                    byte_count++;
                    if ((byte_count & 0xFF) == 0)
                        cpc_print_char('.');
                } else {
                    /* Header: scan for \r\n\r\n (state 0→1→2→3→done) */
                    if (c == '\r') {
                        hdr_state = (hdr_state == 2) ? 3 : 1;
                    } else if (c == '\n') {
                        if      (hdr_state == 1) hdr_state = 2;
                        else if (hdr_state == 3) hdr_done  = 1;
                        else                     hdr_state = 0;
                    } else {
                        hdr_state = 0;
                    }
                }
            }
        }
    }

    cpc_print("\r\n");

    /* Step 8: close or abandon file based on HTTP status */
    if (http_status == '2') {
        if (cas_out_close()) {
            cpc_print("Done! ");
            print_uint(byte_count);
            cpc_print(" bytes saved.\r\n");
        } else {
            cas_out_abandon();
            cpc_print("ERROR: File close failed\r\n");
            cpc_print("Delete the .BAK file then retry.\r\n");
        }
    } else {
        cas_out_abandon();
        cpc_print("ERROR: HTTP error ");
        cpc_print_char(http_status ? http_status : '?');
        cpc_print("\r\n");
    }

    net_close();

done:
    cpc_print("Press any key.\r\n");
    cpc_wait_key();
}
