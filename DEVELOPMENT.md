# cpc-sdcc — Development Reference

## API reference

### Network init (`src/netinit.h`)

```c
/* Read N4C.CFG and initialise the chip.
 * Returns 0, -1 (file not found, USB build only), or -2 (chip not present). */
int net_init_from_file(void);

/* Initialise directly from a config struct. Returns 0 or -1 (no chip). */
int net_init(const net_config_t *cfg);
```

### TCP (`src/net.h`)

```c
int           net_socket_open(void);
int           net_connect(const unsigned char *ip, unsigned int port);
int           net_send(const unsigned char *buf, unsigned int len);
unsigned int  net_recv(unsigned char *buf, unsigned int maxlen);
void          net_close(void);
unsigned char net_is_connected(void);
unsigned int  net_rx_available(void);
```

### UDP (`src/udp.h`)

```c
int          udp_open(unsigned int src_port);
int          udp_sendto(const unsigned char *dst_ip, unsigned int dst_port,
                        const unsigned char *buf, unsigned int len);
unsigned int udp_rx_available(void);
unsigned int udp_recv(unsigned char *buf, unsigned int maxlen);
void         udp_close(void);
```

### DNS (`src/dns.h`)

```c
/* Resolve hostname to a 4-byte IPv4 address.
 * Returns 0, -1 (socket error), -2 (send error), -3 (timeout), -4 (no A record). */
int dns_resolve(const unsigned char *dns_server_ip, const char *hostname,
                unsigned char *result_ip);
```

### CPC firmware (`src/cpcbios.h`)

```c
void          cpc_print_char(char c);        /* TXT_OUTPUT */
void          cpc_print(const char *s);      /* print null-terminated string */
void          cpc_cls(void);                 /* clear text window */
void          cpc_set_mode(char mode);       /* 0=160×200/16col, 1=320×200/4col, 2=640×200/2col */
int           cpc_read_key(void);            /* KM_READ_CHAR — non-blocking; -1 if none */
char          cpc_wait_key(void);            /* KM_WAIT_CHAR — blocks until keypress */
unsigned char cpc_test_key(unsigned char k); /* KM_TEST_KEY — raw matrix state */
unsigned int  cpc_time_ms(void);             /* 50 Hz frame counter × 20; wraps ~65 s */
```

### AMSDOS file output (`src/amsdos.h`)

```c
/* Open new output file. Returns 1 on success, 0 on failure. */
unsigned char cas_out_open(const char *fname, int flen);

/* Write one byte. Returns 1 on success, 0 on failure. */
unsigned char cas_out_char(char c);

/* Close file (renames .$$$ to final name). Returns 1 on success. */
unsigned char cas_out_close(void);

/* Discard file without renaming — use on error to clean up. */
void cas_out_abandon(void);
```

CAS_OUT addresses are standard on both ULIfAC and Albireo (no shift).

### AMSDOS file input (`src/amsdos_in.h`)

```c
/* Open file for reading. Returns 1 on success, 0 if not found. */
unsigned char cas_in_open(const char *fname, unsigned int flen);

/* Read next byte. Returns 0-255 on success, -1 on EOF/error. */
int cas_in_readbyte(void);

/* Close the open input file. */
void cas_in_close(void);
```

Compile with `-DAMSDOS_USB` for Albireo (CAS IN addresses shifted +3 from standard ROM).

**Binary files on FAT:** `CAS_IN_OPEN` with `A=0xFF` on a headerless file defaults to
text mode; `CAS_IN_DIRECT` stops at the first `0x1A` byte — a problem for PNG files
(magic header contains `0x1A` at byte 6).  Fix: wrap binary files with a 128-byte AMSDOS
type-2 header using `amsdos_wrap.py` (load address `0000` for data files).  Text files
(HTML, manifests) need no wrapping.

### Multi-socket TCP (`src/net_multi.h`)

For use when all four W5100S sockets are needed (e.g. httpd).

```c
#define SREG(s, off)   /* W5100S socket register address */
#define STX_BASE(s)    /* TX ring buffer base for socket s */
#define SRX_BASE(s)    /* RX ring buffer base for socket s */

int           tcp_listen_sock(unsigned char s, unsigned int port);
unsigned char tcp_get_status_sock(unsigned char s);
int           tcp_send_sock(unsigned char s, const unsigned char *buf, unsigned int len);
unsigned int  tcp_recv_sock(unsigned char s, unsigned char *buf, unsigned int maxlen);
unsigned int  tcp_rx_available_sock(unsigned char s);
unsigned int  tcp_tx_free_sock(unsigned char s);
void          tcp_close_sock(unsigned char s);
```

### iRAM1024 banking (`src/bank.h`)

For the [iRAM1024](https://github.com/etomuc/CPC464-iRAM1024) DK'Tronics/Yarek compatible
1 MB expansion RAM.  Banks 1–14 × 16 KB = 224 KB usable (288 KB total with base 64 KB).
Banks 1–7 use port 0x7F7F (standard DK'Tronics block); banks 8–14 use port 0x7F7E (Yarek
extended block).

```c
#define BANK_CFG_C000  1u   /* map last 16 KB of bank to &C000-&FFFF */
#define BANK_MAX      14u   /* highest usable bank */

void bank_select(unsigned char bank, unsigned int cfg);
void bank_restore(void);
```

Both functions use `OUT (C), A` with B=0x7F (A15=0).  Using `OUT (n), A` sets A15=1 and
the iRAM1024 PAL ignores the command (symptom: 64 KB reported).

**Costdown CPC 464 ASIC:** also responds to port 0x7F7F.  Both functions must use the same
`OUT (C), A` form — mixing forms leaves the ASIC with expansion RAM mapped and causes the
firmware ISR to corrupt `&C000` after EI (symptom: system hang).

When a bank is selected, `&C000–&FFFF` contains expansion RAM.  `tcp_send_sock` can be
called with a pointer into `&C000` to stream data directly from expansion RAM to the chip.

### httpd manifest — HTTPD.MAN

Plain-text file (CR+LF), one entry per line:

```
/=INDEX.HTM
/index.htm=INDEX.HTM
/about.htm=ABOUT.HTM
/style.css=STYLE.CSS
/logo.gif=LOGO.GIF
```

URL path (left of `=`) up to 13 characters; CPC filename (right) up to 12 characters (8.3).
MIME type is inferred from the URL extension (`htm`/`html` → `text/html`, `css`, `gif`,
`jpg`, `png`, `txt`, else `application/octet-stream`).  Up to 24 entries.  Files with an
AMSDOS binary header (first byte `0xFF`) have the 128-byte header stripped on load.

The manifest is read entirely into RAM and closed before any web file is opened — AMSDOS
only supports one CAS input file open at a time.

---

## Network configuration — how each target reads N4C.CFG

**ULIfAC / real floppy** — the BASIC loader reads `N4C.CFG` with `OPENIN` and POKEs the
parsed addresses into RAM at `&3F10`–`&3F1F` before calling the binary.  The binary reads
those 16 bytes directly; no file I/O in machine code.

**Albireo (Unidos)** — the binary opens `N4C.CFG` itself via `CAS_IN_DIRECT`.  The BASIC
loader collects any user input, then issues `MEMORY &3FFF` to lower HIMEM below `&4000`
before loading the binary.  This is required because Unidos sets the default HIMEM above
`&4000`, and CPC BASIC refuses to `LOAD` a binary into its managed memory area without
this.  Run BASIC loaders with `RUN` or `CHAIN` — both work on Unidos.  The USB/FAT AMSDOS
shifts CAS IN routine addresses by +3 from the standard ROM; handled at compile time with
`-DAMSDOS_USB`.

**httpd (both targets)** — the binary always reads `N4C.CFG` itself.  ULIfAC build uses
`-DAMSDOS_STD`; Albireo build uses `-DAMSDOS_USB`.

**M4** — `net_init_from_file()` is a no-op.  No N4C.CFG needed.

---

## W5100S hardware (Net4CPC)

| Port   | Purpose                    |
|--------|----------------------------|
| 0xFD20 | Mode register (reads 0x03) |
| 0xFD21 | High address byte          |
| 0xFD22 | Low address byte           |
| 0xFD23 | Data (auto-increments)     |

Socket 0 is used for TCP.  Socket 1 is used for UDP/DNS.  httpd uses all four sockets.

| Socket      | TX base | RX base | Size |
|-------------|---------|---------|------|
| 0 (TCP)     | 0x4000  | 0x6000  | 2 KB |
| 1 (UDP/DNS) | 0x4800  | 0x6800  | 2 KB |
| 2 (httpd)   | 0x5000  | 0x7000  | 2 KB |
| 3 (httpd)   | 0x5800  | 0x7800  | 2 KB |

`N_TMSR` (0x001B) and `N_RMSR` (0x001C) are both written to `0x55` by `net_init()` to
allocate 2 KB per socket on all four sockets.

---

## M4 WiFi card protocol

### I/O ports

| Port   | Use |
|--------|-----|
| 0xFE00 | Write command bytes one at a time (`OUT (C), A` with BC=0xFE00) |
| 0xFC00 | Strobe — write any value after the last command byte |

Response data lives in the M4 ROM area when M4 is selected:
- `*(uint16_t*)0xFF02` — pointer to the response buffer
- `*(uint16_t*)0xFF06` — pointer to the socket info table

**Packet format:** `byte[0]` = payload length (not counting itself), `byte[1:2]` = command
word LE, `byte[3+]` = parameters.

### Commands

| Constant       | Value  | Parameters                              | Response |
|----------------|--------|-----------------------------------------|----------|
| `C_NETSOCKET`  | 0x4331 | domain=0, type=0, protocol=6           | socket# in resp[3]; 0xFF=error |
| `C_NETCONNECT` | 0x4332 | socket, 4×IP, 2×port LE               | resp[3]=0xFF=error; then poll state |
| `C_NETCLOSE`   | 0x4333 | socket                                  | — |
| `C_NETSEND`    | 0x4334 | socket, size-lo, size-hi, data          | poll state until != 2; **max 250 bytes/call** |
| `C_NETRECV`    | 0x4335 | socket, size-lo, size-hi               | resp[3]=0=ok; len at resp[4:5]; data at resp[6+] |
| `C_NETHOSTIP`  | 0x4336 | NUL-terminated hostname                 | resp[0]!=0xFF=accepted; poll sock0 state!=5; IP at sock0[4..7] |
| `C_NETSTAT`    | 0x4323 | —                                       | WiFi: 0=idle,1=conn,2=wrong-pw,3=no-AP,4=fail,5=ok |
| `C_SETNETWORK` | 0x4321 | config string                           | — |
| `C_GETNETWORK` | 0x433B | —                                       | full config structure |

Max `C_NETRECV` response: 2048 bytes.

### Socket states

| State | Meaning |
|-------|---------|
| 0 | IDLE / OK — **connected** state; also state after send completes |
| 1 | Connecting in progress |
| 2 | Send in progress |
| 3 | Remote closed connection |
| 5 | DNS lookup in progress (socket 0 only) |

User sockets: 1–4.  Socket 0 is reserved for DNS.

### ROM/RAM mode switching

The M4 has two memory modes: ROM mode (static ROM bytes) and RAM mode (live
response/socket data).  After a strobe the M4 switches to RAM mode.  Calling
`KL_ROM_SELECT` (`m4_select_rom`) after the strobe forces the M4 back to ROM mode,
returning stale bytes from `0xFF02`/`0xFF06`.

**Correct sequence for every M4 command:**
1. `m4_refresh()` — call `m4_select_rom()`, cache resp and socket-table pointers from 0xFF02/0xFF06
2. Send command bytes via `m4_out()`
3. `m4_strobe()` — M4 processes, switches to RAM mode
4. `m4_wait()`
5. Read response/socket data using the **cached** pointers — no further `m4_select_rom()`
6. `m4_select_basic()` — restore BASIC ROM (slot 0)

### Interrupt safety — m4_select_basic()

After every completed M4 command, `m4_select_basic()` calls `KL_ROM_SELECT(0)` + `ei`.
BASIC cursor-blink and timer ISRs access `0xC000–0xFFFF`; if the M4 ROM is still selected
when an interrupt fires, those ISRs execute M4 ROM bytes as Z80 instructions and the machine
crashes/reboots.  Every `net_send`, `net_recv`, `net_connect`, `net_socket_open`, `net_close`,
and `dns_resolve` ends with `m4_select_basic()`.

### Cached socket state in net_recv()

`net_recv()` captures `sock[0]` into `m4_last_sock_state` while M4 is in RAM mode (after
strobe, before `m4_select_basic()`).  `net_is_connected()` returns this cached value and is
safe to call after screen writes that invoke `romen()`/`KL_ROM_RESTORE` — those would
re-enable the M4 ROM if it were last selected, making a fresh socket read return stale data.

### TCP receive loop patterns

**Interactive connection (telnet):** exit immediately when socket closes with no data.
```c
while (1) {
    received = net_recv(buf, maxlen);
    if (!received && !net_is_connected()) break;
    if (received) { /* process data */ }
}
```

**Request/response connection (ntp, tcptest):** drain until idle or threshold reached.
```c
unsigned int total = 0, attempts;
for (attempts = 0; attempts < 200; attempts++) {
    unsigned int n = net_recv(buf + total, maxlen - total);
    if (n) { total += n; if (total >= threshold) break; }
}
```

### ROM slot scanning

`m4_rom_init()` scans upper ROM slots 127..1 for the M4 ROM by matching the RSX name
string `"M4 BOAR\xC4"` at each slot's command table.  After that, `m4_select_rom()` uses
`KL_ROM_SELECT` (0xB90F) to re-select the found slot.  On the author's CPC, M4 is at slot 7.

### UDP

M4 firmware is TCP only.  `C_NETSOCKET` with protocol=6 creates TCP sockets only.  UDP is
not supported.

---

## SDCC calling convention

SDCC 4.x with `-mz80` uses **sdcccall(1)** by default:

| Argument | Type    | Register |
|----------|---------|----------|
| 1st      | int/ptr | HL       |
| 1st      | char    | A        |
| 2nd      | int/ptr | DE       |
| 2nd      | char    | pushed   |
| 3rd+     | any     | pushed   |
| Return char |      | A        |
| Return int  |      | HL       |

`w5100_write_reg` declares `val` as `unsigned int` (not `char`) so SDCC passes it in DE —
the low byte E is used directly in `OUT (C), E`.

`cas_out_open` declares `flen` as `int` for the same reason — it lands in DE/E, avoiding
manual stack cleanup in `__naked` asm.

`__naked` functions must not use `4(ix)` for parameters — SDCC omits the IX frame for
pure inline-asm functions.  Use the register the calling convention already places the
argument in.

---

## Memory map

| Range         | Use |
|---------------|-----|
| 0x0000–0x3FFF | Lower ROM (BASIC/firmware) |
| 0x3E00–0x3E7F | wget: hostname (filled by BASIC loader) |
| 0x3E80–0x3EFF | wget: path (filled by BASIC loader) |
| 0x3F00–0x3F0D | wget: filename, length, port (filled by BASIC loader) |
| 0x3F0C–0x3F0D | telnet: port lo/hi (filled by BASIC loader) |
| 0x3E00–0x3EFF | telnet: hostname (filled by BASIC loader) |
| 0x3F10–0x3F1F | N4C.CFG config block (ULIfAC build, filled by BASIC loader) |
| 0x4000–0x6FFF | Program code (`--code-loc 0x4000`) |
| 0x6800–0x6FFF | telnet: Code Page 437 charset (loaded by BASIC at runtime) |
| 0x7000–0xBFEF | Static data and BSS (`--data-loc 0x7000`) |
| 0xBFF0–0xBFFF | Stack (grows down from 0xBFF0) |
| 0xC000–0xFFFF | Screen RAM + upper ROM |
