#include "../../src/cpcbios.h"
#include "../../src/netinit.h"
#include "../../src/dns.h"
#include "../../src/net.h"
#ifndef NET_M4
#include "../../src/w5100.h"
#endif
#include "screen.h"
#include "ansi.h"
#include "keyboard.h"

/*
 * Parameters written by TELNET.BAS before CALL &4000.
 *   &3E00  hostname (null-terminated)
 *   &3F0C  port low byte
 *   &3F0D  port high byte
 */
#define CFG_HOST    ((const char *)0x3E00)
#define CFG_PORT_LO (*(const unsigned char *)0x3F0C)
#define CFG_PORT_HI (*(const unsigned char *)0x3F0D)

#define RECV_BUF_SIZE 256

/* Telnet protocol constants (RFC 854) */
#define T_IAC   0xFF
#define T_WILL  0xFB
#define T_WONT  0xFC
#define T_DO    0xFD
#define T_DONT  0xFE
#define T_SB    0xFA
#define T_SE    0xF0

/* Telnet options */
#define T_OPT_ECHO  1   /* server echoes our input */
#define T_OPT_SGA   3   /* suppress go-ahead (character mode) */
#define T_OPT_TTYPE 24  /* terminal type (RFC 1091) */
#define T_OPT_NAWS  31  /* negotiate about window size */

/* SB sub-commands */
#define T_SB_IS     0
#define T_SB_SEND   1

/* IAC state machine */
#define S_NORMAL   0
#define S_IAC      1    /* saw 0xFF */
#define S_CMD      2    /* saw WILL/DO/WONT/DONT, awaiting option byte */
#define S_SB       3    /* inside subnegotiation */
#define S_SB_IAC   4    /* saw IAC inside subnegotiation */

static unsigned char recv_buf[RECV_BUF_SIZE];

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

static void parse_dotted_ip(const char *p, unsigned char *ip) {
    unsigned char i;
    for (i = 0; i < 4; i++) {
        unsigned int v = 0;
        while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
        ip[i] = (unsigned char)v;
        if (i < 3) p++;
    }
}

/* Send a 3-byte IAC response inline — avoids a static buffer and a function call. */
static void send_iac(unsigned char cmd, unsigned char opt) {
    unsigned char resp[3];
    resp[0] = T_IAC;
    resp[1] = cmd;
    resp[2] = opt;
    net_send(resp, 3);
}

/* Send IAC SB TTYPE IS "vt100" IAC SE */
static void send_ttype(void) {
    unsigned char buf[11];
    buf[0]  = T_IAC;
    buf[1]  = T_SB;
    buf[2]  = T_OPT_TTYPE;
    buf[3]  = T_SB_IS;
    buf[4]  = 'v'; buf[5]  = 't'; buf[6]  = '1';
    buf[7]  = '0'; buf[8]  = '0';
    buf[9]  = T_IAC;
    buf[10] = T_SE;
    net_send(buf, 11);
}

/* Send IAC SB NAWS 0 80 0 25 IAC SE */
static void send_naws(void) {
    unsigned char buf[9];
    buf[0] = T_IAC;
    buf[1] = T_SB;
    buf[2] = T_OPT_NAWS;
    buf[3] = 0;
    buf[4] = 80;
    buf[5] = 0;
    buf[6] = 25;
    buf[7] = T_IAC;
    buf[8] = T_SE;
    net_send(buf, 9);
}


void main(void) {
    unsigned char dns_server[4];
    unsigned char server_ip[4];
    unsigned int  port;
    unsigned char iac_state, iac_cmd;
    unsigned char sb_opt, sb_pos;
    unsigned char local_echo;
    unsigned int  received;
    int rc, key;

    cpc_set_mode(1);
    cpc_cls();
#ifdef NET_M4
    cpc_print("TELNET for CPC / M4 WiFi\r\n");
    cpc_print("==========================\r\n");
    cpc_print("Initialising network...");
    (void)net_init_from_file();
    cpc_print(" OK\r\n");

#else
    cpc_print("TELNET for CPC / Net4CPC\r\n");
    cpc_print("==========================\r\n");
    /* Step 1: network init */
    cpc_print("Initialising network...");
    rc = net_init_from_file();
    if (rc == -1) { cpc_print("file not found\r\n"); goto done; }
    if (rc == -2) { cpc_print("no chip\r\n");        goto done; }
    cpc_print(" OK\r\n");
#endif

    /* Step 2: resolve or parse host */
    if (*CFG_HOST >= '0' && *CFG_HOST <= '9') {
        parse_dotted_ip(CFG_HOST, server_ip);
    } else {
        cpc_print("Resolving: ");
        cpc_print(CFG_HOST);
        cpc_print("\r\n");
#ifdef NET_M4
        dns_server[0] = dns_server[1] = dns_server[2] = dns_server[3] = 0;
#else
        dns_server[0] = w5100_read_reg(N_DNS0);
        dns_server[1] = w5100_read_reg(N_DNS0 + 1);
        dns_server[2] = w5100_read_reg(N_DNS0 + 2);
        dns_server[3] = w5100_read_reg(N_DNS0 + 3);
#endif
        rc = dns_resolve(dns_server, CFG_HOST, server_ip);
        if (rc != 0) {
            cpc_print("ERROR: DNS failed\r\n");
            goto done;
        }
    }
    cpc_print("Server IP: ");
    print_ip(server_ip);
    cpc_print("\r\n");

    port = (unsigned int)CFG_PORT_LO | ((unsigned int)CFG_PORT_HI << 8);

    /* Step 3: open socket and connect */
    if (net_socket_open()) {
        cpc_print("ERROR: Could not open socket\r\n");
        goto done;
    }

    cpc_print("Connecting...");
    if (net_connect(server_ip, port)) {
        cpc_print(" FAIL\r\n");
        net_close();
        goto done;
    }
    cpc_print(" OK\r\n");

    /* Switch to Mode 2 (80×25) for full ANSI terminal display */
    screen_init();

    iac_state  = S_NORMAL;
    iac_cmd    = 0;
    sb_opt     = 0;
    sb_pos     = 0;
    local_echo = 1;

    /* Reprogram cursor and ESC keys to return unambiguous control codes. */
    keyboard_init();

    /* Advertise our capabilities up front */
    send_iac(T_WILL, T_OPT_TTYPE);
    send_iac(T_WILL, T_OPT_NAWS);

    screen_cursor_draw();

    /* Step 4: main telnet loop.
     * Exit when net_recv() returns no data AND socket is closed.
     * Checking net_is_connected() after net_recv() uses the socket state
     * captured inside net_recv() while M4 was in RAM mode — reliable even
     * after screen writes that call romen() / KL_ROM_RESTORE. */
    while (1) {

        /* Receive and process server data */
        received = net_recv(recv_buf, RECV_BUF_SIZE);
        if (!received && !net_is_connected()) break;
        if (received) {
            unsigned int i;
            screen_begin();
            screen_cursor_erase();
            for (i = 0; i < received; i++) {
                unsigned char c = recv_buf[i];

                switch (iac_state) {
                case S_NORMAL:
                    if (c == T_IAC) {
                        iac_state = S_IAC;
                    } else {
                        screen_write(c);
                    }
                    break;

                case S_IAC:
                    if (c == T_WILL || c == T_WONT || c == T_DO || c == T_DONT) {
                        iac_cmd   = c;
                        iac_state = S_CMD;
                    } else if (c == T_SB) {
                        sb_pos = 0;
                        iac_state = S_SB;
                    } else if (c == T_IAC) {
                        /* IAC IAC = literal 0xFF in data stream */
                        cpc_print_char(0xFF);
                        iac_state = S_NORMAL;
                    } else {
                        /* Single-byte command (AYT, GA, etc.) — ignore */
                        iac_state = S_NORMAL;
                    }
                    break;

                case S_CMD:
                    if (iac_cmd == T_WILL && c == T_OPT_ECHO) {
                        local_echo = 0;
                        send_iac(T_DO, c);
                    } else if (iac_cmd == T_WONT && c == T_OPT_ECHO) {
                        local_echo = 1;
                        send_iac(T_DONT, c);
                    } else if (iac_cmd == T_WILL) {
                        send_iac(T_DONT, c);
                    } else if (iac_cmd == T_DO) {
                        if (c == T_OPT_NAWS) {
                            send_iac(T_WILL, c);
                            send_naws();
                        } else if (c == T_OPT_TTYPE) {
                            /* already sent WILL TTYPE; server now confirms */
                        } else {
                            send_iac(T_WONT, c);
                        }
                    }
                    iac_state = S_NORMAL;
                    break;

                case S_SB:
                    if (c == T_IAC) {
                        iac_state = S_SB_IAC;
                    } else if (sb_pos == 0) {
                        sb_opt = c; sb_pos = 1;
                    } else if (sb_pos == 1) {
                        /* sub-command byte — store in iac_cmd to reuse a register */
                        iac_cmd = c; sb_pos = 2;
                    }
                    break;

                case S_SB_IAC:
                    if (c == T_SE) {
                        if (sb_opt == T_OPT_TTYPE && iac_cmd == T_SB_SEND)
                            send_ttype();
                        iac_state = S_NORMAL;
                    } else if (c != T_IAC) {
                        iac_state = S_SB;
                    }
                    break;
                }
            }
            screen_cursor_draw();
            screen_end();
        }

        /* Send any pending keypress */
        key = cpc_read_key();
        if (key == 0x1D) break;     /* Ctrl+] = disconnect */

        if (key >= 0) {
            unsigned char k = (unsigned char)key;
            unsigned char seq[3];
            screen_cursor_erase();
            /* Cursor keys now return unique codes via KM_SET_TRANSLATE:
             *   0x0B=up  0x0A=down  0x08=left  0x0E=right  0x1B=ESC */
            seq[0] = 0x1B; seq[1] = '[';
            if      (k == KEY_CURSOR_UP)    { seq[2] = 'A'; net_send(seq, 3); }
            else if (k == KEY_CURSOR_DOWN)  { seq[2] = 'B'; net_send(seq, 3); }
            else if (k == KEY_CURSOR_LEFT)  { seq[2] = 'D'; net_send(seq, 3); }
            else if (k == KEY_CURSOR_RIGHT) { seq[2] = 'C'; net_send(seq, 3); }
            else if (k == 0x0D) {
                unsigned char crlf[2];
                crlf[0] = 0x0D; crlf[1] = 0x0A;
                if (local_echo) { screen_write('\r'); screen_write('\n'); }
                net_send(crlf, 2);
            } else {
                if (local_echo && k != 0x1B) screen_write(k);
                net_send(&k, 1);
            }
            screen_cursor_draw();
        }
    }

    screen_cursor_erase();
    keyboard_restore();
    net_close();

    /* Restore Mode 1 so BASIC gets its normal 40-column screen back. */
    cpc_set_mode(1);
    cpc_cls();

done:
    cpc_print("Disconnected. Press any key.\r\n");
    cpc_wait_key();
}
