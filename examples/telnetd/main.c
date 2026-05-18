#include "../../src/cpcbios.h"
#include "../../src/netinit.h"
#include "../../src/net.h"
#include "../../src/w5100.h"

/*
 * TELNETD: TCP server that hooks into the CPC firmware jump table so that
 * BASIC's own I/O is transparently redirected over the network.
 *
 * After main() returns:
 *   - TXT_OUTPUT (0xBB5A) is patched to buffer chars for TCP then call ROM.
 *   - KM_READ_CHAR (0xBB09) is patched to feed chars from TCP.
 * BASIC continues to the READY prompt; the remote client drives the session.
 * When the TCP connection closes the hooks restore themselves automatically.
 */

#define TELNETD_PORT 23
#define TX_BUF_SZ    64

/* TCP output buffer */
static unsigned char  tx_buf[TX_BUF_SZ];
static unsigned char  tx_pos;

/* Saved original JP targets from firmware jump table */
static unsigned int   orig_txt_output;
static unsigned int   orig_km_read_char;

/* State flags */
static unsigned char  hooks_installed;
static unsigned char  tcp_active;

/* Result of last tcp_poll() call */
static unsigned char  tcp_char_ready;
static unsigned char  tcp_char_val;

/* Incoming IAC state machine */
static unsigned char  srv_iac_state;
static unsigned char  srv_iac_cmd;
static unsigned char  srv_iac_resp[3];

/* -------------------------------------------------------------------------
 * TCP output buffering — called from hook_txt_output
 * -------------------------------------------------------------------------*/
static void on_txt_output(char c) {
    if (!tcp_active) return;
    tx_buf[tx_pos++] = (unsigned char)c;
    if (tx_pos == TX_BUF_SZ || c == '\n' || c == '\r') {
        net_send(tx_buf, tx_pos);
        tx_pos = 0;
    }
}

/* -------------------------------------------------------------------------
 * TCP input polling — called from hook_km_read_char
 * Sets tcp_char_ready / tcp_char_val; handles IAC negotiation.
 * -------------------------------------------------------------------------*/
static void tcp_poll(void) {
    unsigned char c;
    tcp_char_ready = 0;
    if (!tcp_active) return;
    if (!net_is_connected()) {
        tcp_active = 0;
        return;
    }
    while (net_rx_available()) {
        net_recv(&c, 1);
        switch (srv_iac_state) {
        case 0:
            if (c == 0xFF) { srv_iac_state = 1; break; }
            tcp_char_ready = 1;
            tcp_char_val   = c;
            return;
        case 1:
            if (c >= 0xFB && c <= 0xFE) { srv_iac_cmd = c; srv_iac_state = 2; break; }
            if (c == 0xFF) {
                /* IAC IAC = literal 0xFF */
                tcp_char_ready = 1; tcp_char_val = 0xFF;
                srv_iac_state = 0; return;
            }
            srv_iac_state = 0;
            break;
        case 2:
            srv_iac_resp[0] = 0xFF;
            if (srv_iac_cmd == 0xFD) {        /* DO   -> WONT */
                srv_iac_resp[1] = 0xFC; srv_iac_resp[2] = c;
                net_send(srv_iac_resp, 3);
            } else if (srv_iac_cmd == 0xFB) { /* WILL -> DONT */
                srv_iac_resp[1] = 0xFE; srv_iac_resp[2] = c;
                net_send(srv_iac_resp, 3);
            }
            srv_iac_state = 0;
            break;
        }
    }
}

/* -------------------------------------------------------------------------
 * Restore original firmware vectors (idempotent)
 * -------------------------------------------------------------------------*/
static void restore_hooks(void) {
    unsigned char *p;
    if (!hooks_installed) return;
    hooks_installed = 0;
    tcp_active      = 0;
    p    = (unsigned char *)0xBB5A;
    p[0] = 0xC3;
    p[1] = (unsigned char)(orig_txt_output & 0xFF);
    p[2] = (unsigned char)(orig_txt_output >> 8);
    p    = (unsigned char *)0xBB09;
    p[0] = 0xC3;
    p[1] = (unsigned char)(orig_km_read_char & 0xFF);
    p[2] = (unsigned char)(orig_km_read_char >> 8);
}

/* -------------------------------------------------------------------------
 * Firmware intercept hooks
 *
 * hook_txt_output: replaces JP at 0xBB5A.
 *   On entry: A = character (CPC TXT_OUTPUT calling convention).
 *   Buffers the char for TCP then calls the original ROM routine.
 *
 * hook_km_read_char: replaces JP at 0xBB09.
 *   Returns: carry set + A = char, or carry clear (no key).
 *   Feeds from TCP; falls back to ROM keyboard if nothing from TCP.
 * -------------------------------------------------------------------------*/
static void hook_txt_output(void) __naked {
    __asm
        push    af
        push    bc
        push    de
        push    hl
        ld      l, a
        call    _on_txt_output
        pop     hl
        pop     de
        pop     bc
        pop     af
        ld      hl, (_orig_txt_output)
        jp      (hl)
    __endasm;
}

static void hook_km_read_char(void) __naked {
    __asm
        push    bc
        push    de
        push    hl
        call    _tcp_poll
        ld      a, (_tcp_char_ready)
        or      a
        jr      z, 00001$
        ld      a, (_tcp_char_val)
        scf
        pop     hl
        pop     de
        pop     bc
        ret
    00001$:
        ld      a, (_tcp_active)
        or      a
        jr      nz, 00002$
        call    _restore_hooks
    00002$:
        pop     hl
        pop     de
        pop     bc
        ld      hl, (_orig_km_read_char)
        jp      (hl)
    __endasm;
}

/* -------------------------------------------------------------------------
 * Patch the firmware jump table and mark hooks as active
 * -------------------------------------------------------------------------*/
static void install_hooks(void) {
    unsigned char *p;
    unsigned int txt_fn = (unsigned int)hook_txt_output;
    unsigned int km_fn  = (unsigned int)hook_km_read_char;

    /* Save original JP targets (bytes 1-2 of the 3-byte JP instruction) */
    p = (unsigned char *)0xBB5A;
    orig_txt_output   = (unsigned int)p[1] | ((unsigned int)p[2] << 8);
    p = (unsigned char *)0xBB09;
    orig_km_read_char = (unsigned int)p[1] | ((unsigned int)p[2] << 8);

    /* Patch TXT_OUTPUT at 0xBB5A */
    p    = (unsigned char *)0xBB5A;
    p[0] = 0xC3;
    p[1] = (unsigned char)(txt_fn & 0xFF);
    p[2] = (unsigned char)(txt_fn >> 8);

    /* Patch KM_READ_CHAR at 0xBB09 */
    p    = (unsigned char *)0xBB09;
    p[0] = 0xC3;
    p[1] = (unsigned char)(km_fn & 0xFF);
    p[2] = (unsigned char)(km_fn >> 8);

    hooks_installed = 1;
    tcp_active      = 1;
    tx_pos          = 0;
    srv_iac_state   = 0;
    tcp_char_ready  = 0;
}

/* -------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------*/
void main(void) {
    static unsigned char banner[9];
    int rc;

    cpc_set_mode(1);
    cpc_cls();
    cpc_print("TELNETD for CPC / Net4CPC\r\n");
    cpc_print("==========================\r\n");

    cpc_print("Initialising network...");
    rc = net_init_from_file();
    if (rc == -1) { cpc_print("file not found\r\n"); goto done; }
    if (rc == -2) { cpc_print("no chip\r\n");        goto done; }
    cpc_print(" OK\r\n");

    cpc_print("Opening listen socket...");
    if (net_listen(TELNETD_PORT)) {
        cpc_print(" FAIL\r\n");
        goto done;
    }
    cpc_print(" OK\r\n");
    cpc_print("Waiting for connection on port 23.\r\n");
    cpc_print("Press ESC to cancel.\r\n");

    while (!net_is_connected()) {
        int k = cpc_read_key();
        if (k == 0x1B) { net_close(); goto done; }
    }
    cpc_print("Client connected!\r\n");

    /*
     * Announce ourselves and request raw character mode:
     *   IAC WILL ECHO  (0xFF 0xFB 0x01) - server echoes keystrokes
     *   IAC WILL SGA   (0xFF 0xFB 0x03) - suppress go-ahead
     *   IAC DO SGA     (0xFF 0xFD 0x03) - ask client to suppress go-ahead
     */
    banner[0] = 0xFF; banner[1] = 0xFB; banner[2] = 0x01;
    banner[3] = 0xFF; banner[4] = 0xFB; banner[5] = 0x03;
    banner[6] = 0xFF; banner[7] = 0xFD; banner[8] = 0x03;
    net_send(banner, 9);

    install_hooks();
    /*
     * Return to BASIC.  BASIC goes to the READY prompt; from here all
     * screen output is buffered and sent over TCP, and all keyboard
     * input is read from TCP.  When the client disconnects, the hooks
     * silently restore 0xBB5A and 0xBB09 and local I/O resumes.
     */
    return;

done:
    cpc_print("Press any key.\r\n");
    cpc_wait_key();
}
