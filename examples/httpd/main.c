#include "../../src/cpcbios.h"
#include "../../src/cpcdetect.h"
#include "../../src/netinit.h"
#include "../../src/w5100.h"
#include "../../src/net_multi.h"
#include "../../src/bank.h"
#include "../../src/amsdos_in.h"

/*
 * Static HTTP server for Amstrad CPC + Net4CPC (W5100S).
 *
 * Startup:
 *   1. Read N4C.CFG and initialise network.
 *   2. Parse HTTPD.MAN (one "<url>=<cpcfile>" entry per line).
 *   3. Load each file from disk into iRAM1024 expansion banks 1-14.
 *   4. Put all 4 TCP sockets into LISTEN on port 80.
 *   5. Poll all sockets in round-robin, serving GET requests.
 *
 * HTTPD.MAN format (plain text, CR+LF):
 *   /=INDEX.HTM
 *   /about.htm=ABOUT.HTM
 *   /style.css=STYLE.CSS
 *
 * ESC key stops the server.
 */

#define HTTP_PORT    80u
#define MAX_FILES    24
#define MAX_URL      14
#define LOAD_CHUNK   64
#define MAX_REQ_LINE 80
#define SEND_CHUNK   512u

/* MIME type indices */
#define MIME_HTML  0
#define MIME_CSS   1
#define MIME_TEXT  2
#define MIME_GIF   3
#define MIME_JPEG  4
#define MIME_PNG   5
#define MIME_BIN   6

static const char * const mime_types[] = {
    "text/html",
    "text/css",
    "text/plain",
    "image/gif",
    "image/jpeg",
    "image/png",
    "application/octet-stream"
};

/* -------------------------------------------------------------------------
 * File table — populated at startup from disk
 * -------------------------------------------------------------------------*/

typedef struct {
    char          url[MAX_URL];   /* request path e.g. "/index.htm\0" */
    unsigned char start_bank;     /* iRAM bank 1-14 */
    unsigned int  start_offset;   /* offset within that bank (0-16383) */
    unsigned int  length;         /* byte count */
    unsigned char mime_idx;       /* index into mime_types[] */
} file_entry_t;

static file_entry_t files[MAX_FILES];
static unsigned char num_files;
static unsigned char load_bank;    /* next bank to fill during startup */
static unsigned int  load_offset;  /* next offset within load_bank */
static unsigned char load_buf[LOAD_CHUNK];

/* -------------------------------------------------------------------------
 * Per-connection state machine
 * -------------------------------------------------------------------------*/

#define CS_IDLE      0
#define CS_LISTEN    1
#define CS_RECV      2   /* accumulating first request line */
#define CS_SEND_HDR  3
#define CS_SEND_BODY 4
#define CS_CLOSE     5   /* issued DISCON, waiting for CLOSED */

typedef struct {
    unsigned char state;
    char          req_line[MAX_REQ_LINE];
    unsigned char req_len;
    unsigned char file_idx;      /* 0xFF = 404 */
    unsigned char cur_bank;
    unsigned int  cur_offset;
    unsigned int  body_remaining;
} conn_t;

static conn_t conns[4];
static unsigned char rx_tmp[32];
static char hdr_buf[128];

/* -------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------*/

static void print_uint(unsigned int n) {
    static const unsigned int powers[5] = { 10000, 1000, 100, 10, 1 };
    unsigned char i, d, leading = 1;
    for (i = 0; i < 5; i++) {
        for (d = 0; n >= powers[i]; d++) n -= powers[i];
        if (d || !leading || i == 4) { cpc_print_char('0' + d); leading = 0; }
    }
}

static unsigned char uint_to_str(unsigned int n, char *buf) {
    static const unsigned int powers[5] = { 10000, 1000, 100, 10, 1 };
    unsigned char i, d, leading = 1, len = 0;
    for (i = 0; i < 5; i++) {
        for (d = 0; n >= powers[i]; d++) n -= powers[i];
        if (d || !leading || i == 4) { buf[len++] = '0' + d; leading = 0; }
    }
    buf[len] = '\0';
    return len;
}

static unsigned int str_len(const char *s) {
    unsigned int n = 0;
    while (*s++) n++;
    return n;
}

/* Case-insensitive comparison of two null-terminated strings. */
static unsigned char str_ieq(const char *a, const char *b) {
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a | 32u;
        unsigned char cb = (unsigned char)*b | 32u;
        if (ca != cb) return 0;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

static unsigned char get_mime(const char *url) {
    const char *dot = (void *)0;
    const char *p = url;
    while (*p) { if (*p == '.') dot = p; p++; }
    if (!dot) return MIME_BIN;
    dot++;
    if ((dot[0]|32)=='h' && (dot[1]|32)=='t' && (dot[2]|32)=='m') return MIME_HTML;
    if ((dot[0]|32)=='c' && (dot[1]|32)=='s' && (dot[2]|32)=='s') return MIME_CSS;
    if ((dot[0]|32)=='g' && (dot[1]|32)=='i' && (dot[2]|32)=='f') return MIME_GIF;
    if ((dot[0]|32)=='j' && (dot[1]|32)=='p' && (dot[2]|32)=='g') return MIME_JPEG;
    if ((dot[0]|32)=='p' && (dot[1]|32)=='n' && (dot[2]|32)=='g') return MIME_PNG;
    if ((dot[0]|32)=='t' && (dot[1]|32)=='x' && (dot[2]|32)=='t') return MIME_TEXT;
    return MIME_BIN;
}

static unsigned char find_file(const char *url) {
    unsigned char i;
    for (i = 0; i < num_files; i++) {
        if (str_ieq(url, files[i].url)) return i;
    }
    return 0xFF;
}

/* -------------------------------------------------------------------------
 * File loading
 * -------------------------------------------------------------------------*/

static unsigned char load_file(unsigned char fi, const char *fname, unsigned char flen) {
    int c;
    unsigned int len = 0;
    unsigned char n, i;

    if (!cas_in_open(fname, (unsigned int)flen)) return 0;

    cpc_print("  ");
    cpc_print(fname);

    /* Skip 128-byte AMSDOS header if present (first byte = 0xFF) */
    c = cas_in_readbyte();
    if (c == 0xFF) {
        unsigned char hdr;
        for (hdr = 1; hdr < 128; hdr++) cas_in_readbyte();
        c = cas_in_readbyte();
    }

    files[fi].start_bank   = load_bank;
    files[fi].start_offset = load_offset;

    while (c >= 0 && load_bank <= BANK_MAX) {
        /* Read up to LOAD_CHUNK bytes into load_buf — no bank selected here */
        n = 0;
        while (n < LOAD_CHUNK && c >= 0) {
            load_buf[n++] = (unsigned char)c;
            c = cas_in_readbyte();
        }

        /* Write load_buf[0..n-1] to banked RAM, advancing bank as needed */
        i = 0;
        while (i < n && load_bank <= BANK_MAX) {
            unsigned int space = 0x4000u - load_offset;
            unsigned char write_n = (unsigned char)(n - i);
            unsigned char j;
            unsigned char *dst;

            if ((unsigned int)write_n > space) write_n = (unsigned char)space;

            bank_select(load_bank, BANK_CFG_C000);
            dst = (unsigned char *)(0xC000u + load_offset);
            for (j = 0; j < write_n; j++) dst[j] = load_buf[i + j];
            bank_restore();

            i          += write_n;
            len        += (unsigned int)write_n;
            load_offset += (unsigned int)write_n;
            if (load_offset >= 0x4000u) {
                load_offset = 0u;
                load_bank++;
            }
        }
    }

    cas_in_close();
    files[fi].length = len;

    cpc_print(" (");
    print_uint(len);
    cpc_print("b)\r\n");
    return 1;
}

/* Parse one line of HTTPD.MAN: "<url>=<cpcfile>" */
static void process_manifest_line(const char *line) {
    const char *eq = line;
    char fname[13];
    unsigned char url_len, flen, i;

    while (*eq && *eq != '=') eq++;
    if (!*eq) return;

    if (num_files >= MAX_FILES) return;

    /* URL */
    url_len = (unsigned char)(eq - line);
    if (url_len >= MAX_URL) url_len = MAX_URL - 1;
    for (i = 0; i < url_len; i++) files[num_files].url[i] = line[i];
    files[num_files].url[url_len] = '\0';

    /* CPC filename */
    eq++;
    flen = 0;
    while (*eq && flen < 12) fname[flen++] = *eq++;
    fname[flen] = '\0';

    files[num_files].mime_idx = get_mime(files[num_files].url);
    if (files[num_files].mime_idx == MIME_BIN)
        files[num_files].mime_idx = get_mime(fname);

    if (!load_file(num_files, fname, flen)) {
        cpc_print("  SKIP: not found\r\n");
        return;
    }

    num_files++;
}

/*
 * CAS supports only one input file open at a time.  If we called load_file
 * (which opens the data file) while the manifest was still open, the first
 * cas_in_open inside load_file would close the manifest and we would only
 * ever process the first entry.  Fix: read the whole manifest into a RAM
 * buffer, close it, then parse and load each entry.
 */
#define MANIFEST_BUF 512

static void load_manifest(void) {
    static const char man_name[] = "HTTPD.MAN";
    static char man_buf[MANIFEST_BUF];
    unsigned int man_len = 0;
    char line[28];
    unsigned char pos;
    unsigned int i;
    int c;

    cpc_print("Loading HTTPD.MAN...\r\n");

    if (!cas_in_open(man_name, 9u)) {
        cpc_print("  not found\r\n");
        return;
    }

    c = cas_in_readbyte();
    if (c == 0xFF) c = cas_in_readbyte();

    while (c >= 0 && man_len < MANIFEST_BUF - 1) {
        man_buf[man_len++] = (char)c;
        c = cas_in_readbyte();
    }
    man_buf[man_len] = '\0';
    cas_in_close();

    pos = 0;
    for (i = 0; i <= man_len; i++) {
        unsigned char b = (i < man_len) ? (unsigned char)man_buf[i] : '\n';
        if (b == '\r') continue;
        if (b == '\n' || pos >= 27) {
            line[pos] = '\0';
            pos = 0;
            if (line[0] != '\0') process_manifest_line(line);
            continue;
        }
        line[pos++] = (char)b;
    }

    cpc_print("Files loaded: ");
    print_uint(num_files);
    cpc_print("\r\n");
}

/* -------------------------------------------------------------------------
 * HTTP request parsing
 * -------------------------------------------------------------------------*/

static void parse_request(unsigned char s) {
    conn_t *c = &conns[s];
    char *p = c->req_line;
    char url[MAX_URL];
    unsigned char i;

    /* Skip method e.g. "GET " */
    while (*p && *p != ' ') p++;
    if (!*p) { c->file_idx = 0xFF; return; }
    p++;

    /* Extract path up to space or end */
    i = 0;
    while (*p && *p != ' ' && *p != '\r' && i < MAX_URL - 1)
        url[i++] = *p++;
    url[i] = '\0';

    c->file_idx = find_file(url);

    if (c->file_idx != 0xFF) {
        file_entry_t *f = &files[c->file_idx];
        c->cur_bank      = f->start_bank;
        c->cur_offset    = f->start_offset;
        c->body_remaining = f->length;
    }
}

/* -------------------------------------------------------------------------
 * HTTP response
 * -------------------------------------------------------------------------*/

static const char resp_404[] =
    "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";

static void send_header(unsigned char s) {
    conn_t *c = &conns[s];
    char *p = hdr_buf;
    char num_str[6];
    const char *src;

    if (c->file_idx == 0xFF) {
        tcp_send_sock(s, (const unsigned char *)resp_404, sizeof(resp_404) - 1);
        tcp_close_sock(s);
        c->state = CS_CLOSE;
        return;
    }

    /* Assemble full response header in hdr_buf, then send in one shot */
    for (src = "HTTP/1.0 200 OK\r\nContent-Type: "; *src; ) *p++ = *src++;
    for (src = mime_types[files[c->file_idx].mime_idx]; *src; ) *p++ = *src++;
    for (src = "\r\nContent-Length: "; *src; ) *p++ = *src++;
    uint_to_str(files[c->file_idx].length, num_str);
    for (src = num_str; *src; ) *p++ = *src++;
    for (src = "\r\n\r\n"; *src; ) *p++ = *src++;

    tcp_send_sock(s, (const unsigned char *)hdr_buf, (unsigned int)(p - hdr_buf));
    c->state = CS_SEND_BODY;
}

/*
 * Send up to SEND_CHUNK bytes of the response body from banked RAM.
 * Caps at the current bank boundary so we never straddle banks in one send.
 *
 * IMPORTANT: tcp_send_sock is called while the bank is selected (0xC000 =
 * expansion RAM).  This is safe because:
 *   - tcp_send_sock code is at 0x4000+, not at 0xC000.
 *   - w5100_write_buf reads from 0xC000 + offset = expansion RAM data.
 *   - All W5100S accesses are I/O ports (0xFD20-0xFD23), unaffected by banking.
 *   - No firmware calls happen inside tcp_send_sock.
 */
static void serve_body_chunk(unsigned char s) {
    conn_t *c = &conns[s];
    unsigned int bytes_in_bank = 0x4000u - c->cur_offset;
    unsigned int chunk = c->body_remaining;
    unsigned char status = tcp_get_status_sock(s);

    if (status != SSTAT_ESTABLISHED && status != SSTAT_CLOSE_WAIT) {
        c->state = CS_IDLE;
        return;
    }

    if (chunk > SEND_CHUNK)     chunk = SEND_CHUNK;
    if (chunk > bytes_in_bank)  chunk = bytes_in_bank;

    bank_select(c->cur_bank, BANK_CFG_C000);
    tcp_send_sock(s, (const unsigned char *)(0xC000u + c->cur_offset), chunk);
    bank_restore();

    c->cur_offset     += chunk;
    c->body_remaining -= chunk;

    if (c->cur_offset >= 0x4000u) {
        c->cur_offset = 0u;
        c->cur_bank++;
    }

    if (!c->body_remaining) {
        tcp_close_sock(s);
        c->state = CS_CLOSE;
    }
}

/* -------------------------------------------------------------------------
 * Receive polling
 * -------------------------------------------------------------------------*/

static void poll_recv(unsigned char s) {
    conn_t *c = &conns[s];
    unsigned int avail, n, i;
    unsigned char status = tcp_get_status_sock(s);

    if (status != SSTAT_ESTABLISHED && status != SSTAT_CLOSE_WAIT) {
        c->state = CS_IDLE;
        return;
    }

    avail = tcp_rx_available_sock(s);
    while (avail > 0 && c->state == CS_RECV) {
        n = (avail > 32u) ? 32u : avail;
        n = tcp_recv_sock(s, rx_tmp, n);
        if (!n) break;
        avail -= n;

        for (i = 0; i < n && c->state == CS_RECV; i++) {
            unsigned char b = rx_tmp[i];
            if (b == '\n') {
                /* \r is not stored, so \n alone marks end of request line */
                c->req_line[c->req_len] = '\0';
                parse_request(s);
                c->state = CS_SEND_HDR;
            } else if (b != '\r' && c->req_len < MAX_REQ_LINE - 1) {
                c->req_line[c->req_len++] = (char)b;
            }
        }
    }

    /* Buffer full without \r\n — parse whatever we have */
    if (c->state == CS_RECV && c->req_len >= MAX_REQ_LINE - 1) {
        c->req_line[c->req_len] = '\0';
        parse_request(s);
        c->state = CS_SEND_HDR;
    }
}

/* -------------------------------------------------------------------------
 * Main serve loop
 * -------------------------------------------------------------------------*/

static void serve_forever(void) {
    unsigned char s;

    for (s = 0; s < 4; s++) {
        conns[s].state = CS_IDLE;
        if (tcp_listen_sock(s, HTTP_PORT) == 0)
            conns[s].state = CS_LISTEN;
    }

    cpc_print("Listening on port 80.\r\nPress | to stop.\r\n");

    for (;;) {
        if (cpc_read_key() == '|') break;

        for (s = 0; s < 4; s++) {
            conn_t *c = &conns[s];
            unsigned char status = tcp_get_status_sock(s);

            switch (c->state) {

            case CS_IDLE:
                if (tcp_listen_sock(s, HTTP_PORT) == 0) {
                    c->state  = CS_LISTEN;
                    c->req_len = 0;
                }
                break;

            case CS_LISTEN:
                if (status == SSTAT_ESTABLISHED) {
                    c->state  = CS_RECV;
                    c->req_len = 0;
                }
                break;

            case CS_RECV:
                poll_recv(s);
                break;

            case CS_SEND_HDR:
                send_header(s);
                break;

            case CS_SEND_BODY:
                serve_body_chunk(s);
                break;

            case CS_CLOSE:
                if (status == SSTAT_CLOSED)
                    c->state = CS_IDLE;
                break;
            }
        }
    }

    for (s = 0; s < 4; s++) tcp_close_sock(s);
}

/* -------------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------------*/

static const char * const model_names[] = {
    "Unknown", "CPC 464", "CPC 664", "CPC 6128",
    "464 Plus", "6128 Plus", "GX4000"
};

void main(void) {
    int rc;
    unsigned char model;

    cpc_set_mode(1);
    cpc_cls();
    cpc_print("HTTP Server / Net4CPC\r\n");
    cpc_print("======================\r\n");

    model = cpc_detect_model();
    cpc_print("Machine: ");
    cpc_print(model_names[model < 7u ? model : 0u]);
    cpc_print("  RAM: ");
    print_uint(cpc_detect_ram_kb());
    cpc_print("KB\r\n");

    cpc_print("Network init...");
    rc = net_init_from_file();
    if (rc == -1) { cpc_print("N4C.CFG not found\r\n"); goto done; }
    if (rc == -2) { cpc_print("no chip\r\n");            goto done; }
    cpc_print(" OK\r\n");

    num_files  = 0;
    load_bank  = 1;
    load_offset = 0;
    load_manifest();

    if (!num_files) {
        cpc_print("No files — nothing to serve.\r\n");
        goto done;
    }

    serve_forever();

done:
    cpc_print("Stopped. Press any key.\r\n");
    cpc_wait_key();
}
