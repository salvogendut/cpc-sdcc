#include "w5100.h"
#include "netinit.h"

/* Verify chip presence: MR must read back as 0x03 (AI + IND bits set). */
static unsigned char chip_present(void) __naked {
    __asm
        push    bc
        ld      bc, #0xFD20
        in      a, (c)
        cp      #3
        ld      a, #0
        jr      nz, 00001$
        ld      a, #1
    00001$:
        pop     bc
        ret
    __endasm;
}

static void write_bytes(unsigned int reg, const unsigned char *src,
                        unsigned int n) {
    while (n--)
        w5100_write_reg(reg++, (unsigned int)*src++);
}

/*
 * CAS firmware wrappers.
 *
 * Build with -DAMSDOS_USB for USB/FAT drives (Albireo, GoTek with Unidos):
 *   CAS IN routines are shifted +3 from the standard ROM addresses.
 * Without -DAMSDOS_USB: standard AMSDOS addresses (ULIfAC, real floppy).
 *
 * CAS_IN_OPEN:   B=filename length, HL=filename addr, A=&FF (any type).
 *                Returns carry set on success; CAS keeps internal state.
 * CAS_IN_DIRECT: no args; reads next raw byte from the open stream.
 *                Returns carry set + A=byte; carry clear = EOF.
 * CAS_IN_CLOSE:  no args; closes the currently open input stream.
 *
 * AMSDOS sometimes prepends a 0xFF header byte — discarded on open.
 */
#ifdef AMSDOS_USB
#define CAS_IN_OPEN   0xBC77
#define CAS_IN_CLOSE  0xBC7A
#define CAS_IN_DIRECT 0xBC83
#else
#define CAS_IN_OPEN   0xBC74
#define CAS_IN_CLOSE  0xBC77
#define CAS_IN_DIRECT 0xBC80
#endif

static const char cfg_filename[] = "N4C.CFG";

/* Returns 1 on success, 0 on failure. */
static unsigned char cas_open(void) __naked {
    __asm
        ld      hl, #_cfg_filename
        ld      b,  #7          ; length of "N4C.CFG"
        ld      a,  #0xFF       ; accept any file type
        call    CAS_IN_OPEN
        ld      a,  #0
        jr      nc, 00001$      ; carry clear = failure
        ld      a,  #1
    00001$:
        ld      l, a
        ld      h, #0
        ret
    __endasm;
}

/* Returns next byte (0xFF = EOF sentinel). */
static unsigned char cas_readbyte(void) __naked {
    __asm
        call    CAS_IN_DIRECT
        jr      c,  00001$      ; carry set = valid byte in A
        ld      a,  #0xFF       ; carry clear = EOF
    00001$:
        ret
    __endasm;
}

static void cas_close(void) __naked {
    __asm
        call    CAS_IN_CLOSE
        ret
    __endasm;
}

/* Parse "a.b.c.d" into a 4-byte array. p points to first digit.
 * Returns pointer to first char after the last digit. */
static const char *parse_ip(const char *p, unsigned char *out) {
    unsigned char i, v;
    for (i = 0; i < 4; i++) {
        v = 0;
        while (*p >= '0' && *p <= '9') {
            v = v * 10 + (*p - '0');
            p++;
        }
        out[i] = v;
        if (*p == '.' || *p == '\r' || *p == '\n' || *p == '\0')
            p++;
    }
    return p;
}

#define LINEBUF_SZ 32

int net_init_from_file(void) {
    static net_config_t cfg = {
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF }
    };
    static char line[LINEBUF_SZ];
    unsigned char c;
    unsigned char pos;

    if (!cas_open())
        return -1;

    /* Discard AMSDOS 0xFF header byte if present */
    c = cas_readbyte();
    if (c != 0xFF)
        goto process;   /* not a header byte — process it as data */

    /* It was an AMSDOS header: read and discard the rest (128 bytes total)
     * but we only need to skip 127 more since we already read one.
     * Actually just skip the first byte and proceed normally — if it was
     * truly an AMSDOS binary header the content will be garbage anyway.
     * The n4c-netinit-kv.s approach: if byte[0]==0xFF shift buffer left 1.
     * Here: simply discard the first byte (already done) and continue. */
    c = cas_readbyte();

process:
    pos = 0;
    for (;;) {
        if (c == 0xFF)
            break;          /* EOF */
        if (c == '\r') {
            c = cas_readbyte();
            continue;
        }
        if (c == '\n' || pos == LINEBUF_SZ - 1) {
            line[pos] = '\0';
            pos = 0;
            if (line[0]=='I' && line[1]=='P' && line[2]=='=')
                parse_ip(line + 3, cfg.ip);
            else if (line[0]=='M' && line[1]=='A' && line[2]=='S' &&
                     line[3]=='K' && line[4]=='=')
                parse_ip(line + 5, cfg.netmask);
            else if (line[0]=='G' && line[1]=='W' && line[2]=='=')
                parse_ip(line + 3, cfg.gateway);
            else if (line[0]=='D' && line[1]=='N' && line[2]=='S' &&
                     line[3]=='=')
                parse_ip(line + 4, cfg.dns);
            c = cas_readbyte();
            continue;
        }
        line[pos++] = (char)c;
        c = cas_readbyte();
    }

    cas_close();
    return net_init(&cfg);
}

int net_init(const net_config_t *cfg) {
    if (!chip_present())
        return -1;

    /* 1-second ARP retry time (10000 = 0x2710) */
    w5100_write_reg(N_RTR0,     0x27);
    w5100_write_reg(N_RTR0 + 1, 0x10);
    w5100_write_reg(N_RCR,      10);

    write_bytes(N_SHAR0, cfg->mac,     6);
    write_bytes(N_GAR0,  cfg->gateway, 4);
    write_bytes(N_SUBR0, cfg->netmask, 4);
    write_bytes(N_SIPR0, cfg->ip,      4);
    write_bytes(N_DNS0,  cfg->dns,     4);

    return 0;
}
