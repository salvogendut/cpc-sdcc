![Example Image](n4c-config.png)
# cpc-sdcc

C development for the Amstrad CPC using [SDCC](https://sdcc.sourceforge.net/), targeting the Z80.  
Includes a W5100S Ethernet driver for the [Net4CPC](https://www.cpcwiki.eu/index.php/Net4CPC) hardware,
with TCP, UDP/DNS support, an HTTP file downloader (wget), an NTP time client, an ANSI telnet client,
a telnet daemon that hooks the CPC firmware to serve an interactive BASIC session over TCP,
and a static HTTP server that pre-loads files into iRAM1024 expansion RAM and serves them
concurrently over four TCP sockets.

## Prerequisites

- **SDCC 4.x** with Z80 target (`sdcc`, `sdasz80`, `makebin`)  
  Set `SDCC_BIN` in each build script to your SDCC `bin/` directory.

## Project structure

```
src/
  crt0.s        Startup stub: saves BASIC SP, sets SP=0xBFF0, runs
                static initialisers (gsinit), calls main(), returns to BASIC
  cpcbios.h     CPC firmware wrappers: cpc_print_char, cpc_print,
                cpc_cls, cpc_set_mode, cpc_wait_key, cpc_time_ms
  amsdos.h      AMSDOS file output wrappers: cas_out_open/char/close/abandon
  amsdos_in.h/c AMSDOS file input wrappers: cas_in_open/readbyte/close
                (handles standard and -DAMSDOS_USB shifted addresses)
  w5100.h       W5100S register map and low-level I/O prototypes
  w5100.c       w5100_read_reg / w5100_write_reg (__naked asm), buffer ops
  netinit.h/c   Network init: net_init() and net_init_from_file()
  net.h/c       TCP socket API (socket 0): open/connect/send/recv/close
  net_multi.h/c Multi-socket TCP API (sockets 0-3): listen/send/recv/close
                parameterised by socket number; uses SREG/STX_BASE/SRX_BASE macros
  udp.h/c       UDP socket API (socket 1): open/sendto/recv/close
  dns.h/c       DNS A-record resolver: dns_resolve()
  cpcdetect.h/c CPC model detection (ROM signatures) and expansion RAM probe
  bank.h/c      iRAM1024 DK'Tronics/Yarek banking driver: bank_select/bank_restore
  amsdos_wrap.py  Adds 128-byte AMSDOS type-2 binary header to a raw binary

examples/
  hello/        Prints "Hello, CPC!" and returns to BASIC
  tcptest/      Opens a TCP connection and performs an HTTP GET
  dnstest/      Resolves a hostname via DNS and prints the IP
  wget/         HTTP file downloader — prompts for URL, saves file to disk
  ntp/          NTP/SNTP time client — resolves time.cloudflare.com,
                displays current UTC date and time
  telnet/       ANSI/VT100 telnet client — Mode 2 (80×25) direct screen
                writes, Code Page 437 charset, hardware scroll, ESC[ cursor
                movement / erase / SGR colour support
  telnetd/      Telnet daemon — patches TXT_OUTPUT and KM_READ_CHAR in the
                CPC firmware jump table to mirror BASIC's I/O over TCP;
                returns to BASIC which then drives the remote session
  httpd/        Static HTTP server — reads HTTPD.MAN manifest at startup,
                loads web files into iRAM1024 expansion RAM (banks 1-14,
                up to 224 KB / 288 KB total), then serves HTTP/1.0 GET
                requests on port 80 over all four W5100S sockets concurrently
                (round-robin poll)
```

## Building

Each example has its own `build.sh` that produces **two sets of binaries**
in a single run:

| Output directory | Target hardware          | BASIC loader   |
|------------------|--------------------------|----------------|
| `bin/`           | ULIfAC / real floppy     | `NAME.BAS`     |
| `bin/albireo/`   | Albireo (Unidos)      | `NAMEA.BAS`    |

```bash
cd examples/tcptest && ./build.sh
# bin/TCPTEST.BIN + bin/TCPTEST.BAS
# bin/albireo/TCPTEST.BIN + bin/albireo/TCPTESTA.BAS

cd examples/dnstest && ./build.sh
# bin/DNSTEST.BIN + bin/DNSTEST.BAS
# bin/albireo/DNSTEST.BIN + bin/albireo/DNSTESTA.BAS

cd examples/wget && ./build.sh
# bin/WGET.BIN + bin/WGET.BAS
# bin/albireo/WGET.BIN + bin/albireo/WGETA.BAS

cd examples/ntp && ./build.sh
# bin/NTP.BIN + bin/NTP.BAS
# bin/albireo/NTP.BIN + bin/albireo/NTPA.BAS

cd examples/telnet && ./build.sh
# bin/TELNET.BIN + bin/CHARSET.BIN + bin/TELNET.BAS
# bin/albireo/TELNET.BIN + bin/albireo/CHARSET.BIN + bin/albireo/TELNETA.BAS

cd examples/telnetd && ./build.sh
# bin/TELNETD.BIN + bin/TELNETD.BAS
# bin/albireo/TELNETD.BIN + bin/albireo/TELNETDA.BAS

cd examples/httpd && ./build.sh
# bin/HTTPD.BIN + bin/HTTPD.BAS + bin/INDEX.HTM + bin/LOGO.PNG + bin/HTTPD.MAN
# bin/albireo/HTTPD.BIN + bin/albireo/HTTDPA.BAS + (same web files)

cd examples/hello && ./build.sh
# bin/HELLO.BIN + bin/HELLO.BAS
```

Copy all files from the relevant output directory to a CPC disk and
`RUN` the `.BAS` loader.

### Ready-to-use disk image

A prebuilt standard CPC DSK image with all ULIfAC binaries is provided:

```
images/n4c_tools.dsk
```

Load it in any CPC emulator (WinAPE, JavaCPC, etc.) or write it to a Gotek
USB drive.  It contains `N4CCFG.BAS` — run it first to create your `N4C.CFG`:

```
RUN "N4CCFG.BAS"
```

It prompts for IP address, subnet mask, gateway, and DNS server, then writes
`N4C.CFG` with the correct CR+LF line endings.  Rebuild the DSK with:

```bash
./make_dsk.sh          # requires iDSK from github.com/reidrac/cpc-mastering
                       # set IDSK=/path/to/idsk if not at ../cpc-mastering/idsk
```

**telnet** requires three files: `TELNET.BIN`, `CHARSET.BIN`, and `N4C.CFG`.
The BASIC loader loads `TELNET.BIN` first (its padding zeros 0x6800–0x6FFF),
then loads `CHARSET.BIN` at `&6800` to restore the Code Page 437 bitmaps.

**httpd** requires `HTTPD.BIN`, `N4C.CFG`, `HTTPD.MAN`, and all web files listed
in the manifest — all on the same CPC disk.  The binary reads everything itself
at startup (no BASIC config-reading helper needed).

## Network configuration — N4C.CFG

Network settings are read from a plain-text file `N4C.CFG` on the CPC disk:

```
IP=192.168.1.100
MASK=255.255.255.0
GW=192.168.1.1
DNS=8.8.8.8
```

The file must use **CR+LF line endings** (standard for AMSDOS).  MAC address
is hardcoded in the binary as `DE:AD:BE:EF:00:FF`.

### How each target reads the config

**ULIfAC / real floppy** — the BASIC loader reads `N4C.CFG` with `OPENIN`
and POKEs the parsed addresses into RAM at `&3F10`–`&3F1F` before calling
the binary.  The binary itself reads those 16 bytes directly; no file I/O
in machine code.

**Albireo (Unidos)** — the binary opens `N4C.CFG` itself via
`CAS_IN_DIRECT`.  The BASIC loader just sets the screen mode, loads the
binary, and calls it.  The USB/FAT AMSDOS shifts CAS IN routine addresses
by +3 from the standard ROM; this is handled at compile time with
`-DAMSDOS_USB` (added automatically by the Albireo build pass).

**httpd (both targets)** — the binary always reads `N4C.CFG` itself, so
both builds use a minimal one-line BASIC loader.  The ULIfAC build is
compiled with `-DAMSDOS_STD` (standard CAS IN addresses); the Albireo
build with `-DAMSDOS_USB` as usual.

## API reference

### Network init (`src/netinit.h`)

```c
/* Read N4C.CFG and initialise the chip. Returns 0, -1 (file not found,
 * USB build only), or -2 (chip not present). */
int net_init_from_file(void);

/* Initialise directly from a config struct. Returns 0 or -1 (no chip). */
int net_init(const net_config_t *cfg);
```

### TCP (`src/net.h`)

```c
int          net_socket_open(void);
int          net_connect(const unsigned char *ip, unsigned int port);
int          net_send(const unsigned char *buf, unsigned int len);
unsigned int net_recv(unsigned char *buf, unsigned int maxlen);
void         net_close(void);
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
 * Returns 0, -1 (socket error), -2 (send error),
 *         -3 (timeout), -4 (no A record / DNS error). */
int dns_resolve(const unsigned char *dns_server_ip, const char *hostname,
                unsigned char *result_ip);
```

### CPC firmware (`src/cpcbios.h`)

```c
void cpc_print_char(char c);       /* TXT_OUTPUT */
void cpc_print(const char *s);     /* print null-terminated string */
void cpc_cls(void);                /* clear text window */
void cpc_set_mode(char mode);      /* 0=160x200/16col, 1=320x200/4col, 2=640x200/2col */
int  cpc_read_key(void);           /* KM_READ_CHAR — non-blocking; returns char or -1 */
char cpc_wait_key(void);           /* KM_WAIT_CHAR — blocks until keypress */
unsigned char cpc_test_key(unsigned char key_num); /* KM_TEST_KEY — raw matrix state */
unsigned int cpc_time_ms(void);    /* 50 Hz frame counter × 20 — elapsed ms, wraps ~65 s */
```

### AMSDOS file output (`src/amsdos.h`)

```c
/* Open new output file. fname = null-terminated name, flen = length.
 * Returns 1 on success, 0 on failure. */
unsigned char cas_out_open(const char *fname, int flen);

/* Write one byte. Returns 1 on success, 0 on failure (disk full?). */
unsigned char cas_out_char(char c);

/* Close file (renames .$$$ to final name). Returns 1 on success. */
unsigned char cas_out_close(void);

/* Discard file without renaming — use on error to clean up. */
void cas_out_abandon(void);
```

CAS_OUT addresses are standard on both ULIfAC and Albireo (no shift).

### AMSDOS file input (`src/amsdos_in.h`)

```c
/* Open file for reading. flen declared unsigned int so sdcccall(1) passes
 * it in DE (avoids pushed-char stack cleanup in __naked asm).
 * Returns 1 on success, 0 if not found. */
unsigned char cas_in_open(const char *fname, unsigned int flen);

/* Read next byte. Returns 0-255 on success, -1 on EOF/error. */
int cas_in_readbyte(void);

/* Close the open input file. */
void cas_in_close(void);
```

Compile with `-DAMSDOS_USB` for Albireo (CAS IN shifted +3) or without
for standard AMSDOS / ULIfAC.

**Binary files on FAT (ULIfAC / USB drives):** `CAS_IN_OPEN` with `A=0xFF`
(auto-detect) on a file without an AMSDOS header defaults to text mode and
`CAS_IN_DIRECT` stops at the first `0x1A` byte — a problem for PNG files
whose magic header contains `0x1A` at byte 6.  The fix is to wrap binary
files with a real 128-byte AMSDOS type-2 header using `amsdos_wrap.py`
(use load address `0000` for data files).  `CAS_IN_OPEN` reads the header,
identifies the file as binary, and `CAS_IN_DIRECT` then delivers all bytes
without stopping at `0x1A`.  The header is consumed by `CAS_IN_OPEN` and is
not returned to the caller.  Text files (HTML, manifest) need no wrapping as
they contain no `0x1A` bytes.

### Multi-socket TCP (`src/net_multi.h`)

For use when all four W5100S sockets are needed (e.g. the httpd).

```c
/* Macros — compute W5100S addresses from socket number s (0-3) */
#define SREG(s, off)  /* socket register address */
#define STX_BASE(s)   /* TX ring buffer base */
#define SRX_BASE(s)   /* RX ring buffer base */

int          tcp_listen_sock(unsigned char s, unsigned int port);
unsigned char tcp_get_status_sock(unsigned char s);
int          tcp_send_sock(unsigned char s, const unsigned char *buf,
                           unsigned int len);
unsigned int tcp_recv_sock(unsigned char s, unsigned char *buf,
                           unsigned int maxlen);
unsigned int tcp_rx_available_sock(unsigned char s);
unsigned int tcp_tx_free_sock(unsigned char s);
void         tcp_close_sock(unsigned char s);
```

### iRAM1024 banking (`src/bank.h`)

For the [iRAM1024](https://github.com/etomuc/CPC464-iRAM1024) DK'Tronics/Yarek
compatible 1 MB expansion RAM.  Banks 1–14 × 16 KB = 224 KB usable (288 KB
total with base 64 KB).  Banks 1–7 are the standard DK'Tronics block (port
0x7F7F); banks 8–14 are the Yarek extended block (port 0x7F7E).

```c
#define BANK_CFG_C000  1u   /* map last 16 KB of bank to &C000-&FFFF */
#define BANK_MAX      14u   /* highest usable bank (7 block-0 + 7 block-1) */

/* Select bank (1-BANK_MAX) mapped to &C000.
 * cfg declared unsigned int so sdcccall(1) passes it in DE. */
void bank_select(unsigned char bank, unsigned int cfg);

/* Restore normal CPC RAM at &C000. */
void bank_restore(void);
```

**Port form:** both functions use `OUT (C), A` with B=0x7F (A15=0), not
`OUT (n), A`.  This is required because the iRAM1024 PAL needs A15=0 to
respond.  Using `OUT (n), A` with a banking value (0xC0–0xFF) sets A15=1
and the hardware ignores the command entirely (symptom: 64 KB reported).

**Costdown CPC 464 ASIC:** the ASIC gate array also responds to port 0x7F7F.
This is harmless as long as `bank_select` and `bank_restore` both use the
same `OUT (C), A` form — the ASIC always receives the restore command and
never gets stuck.  Mixing forms (select via `OUT (C), A`, restore via
`OUT (n), A`) leaves the ASIC with expansion mapped and causes the firmware
ISR to corrupt `&C000` after EI (symptom: system hang).

When a bank is selected, `&C000–&FFFF` contains expansion RAM.  Code at
`&4000+` and I/O port accesses (W5100S at `&FD20–&FD23`) are unaffected,
so `tcp_send_sock` can safely be called with a pointer into `&C000` to
stream file data directly from expansion RAM to the network chip.

### httpd manifest — HTTPD.MAN

Plain-text file (CR+LF), one entry per line:

```
/=INDEX.HTM
/index.htm=INDEX.HTM
/about.htm=ABOUT.HTM
/style.css=STYLE.CSS
/logo.gif=LOGO.GIF
```

URL path (left of `=`) up to 13 characters; CPC filename (right) up to 12
characters (8.3 format).  MIME type is inferred from the URL extension
(`htm`/`html` → `text/html`, `css`, `gif`, `jpg`, `png`, `txt`, else
`application/octet-stream`).  Up to 24 files.  Files with an AMSDOS binary
header (first byte `0xFF`) have the full 128-byte header stripped
automatically on load.

**Implementation note:** the manifest is read entirely into RAM and the file
closed before any web file is opened.  This is necessary because AMSDOS only
supports one CAS input file open at a time — opening a web file while the
manifest is still open would silently close the manifest, causing only the
first entry to be processed.

## W5100S hardware (Net4CPC)

| Port   | Purpose                        |
|--------|--------------------------------|
| 0xFD20 | Mode register (reads 0x03)     |
| 0xFD21 | High address byte              |
| 0xFD22 | Low address byte               |
| 0xFD23 | Data (auto-increments address) |

Socket 0 is used for TCP.  Socket 1 is used for UDP/DNS.  The httpd uses
all four sockets for concurrent HTTP connections.

| Socket      | TX base | RX base | Size |
|-------------|---------|---------|------|
| 0 (TCP)     | 0x4000  | 0x6000  | 2 KB |
| 1 (UDP/DNS) | 0x4800  | 0x6800  | 2 KB |
| 2 (httpd)   | 0x5000  | 0x7000  | 2 KB |
| 3 (httpd)   | 0x5800  | 0x7800  | 2 KB |

`N_TMSR` (0x001B) and `N_RMSR` (0x001C) are both written to `0x55` by
`net_init()` to explicitly allocate 2 KB per socket on all four sockets.

## Calling convention notes

SDCC 4.x with `-mz80` uses **sdcccall(1)** by default:

| Argument position | Type     | Register |
|-------------------|----------|----------|
| 1st               | int/ptr  | HL       |
| 1st               | char     | A        |
| 2nd               | int/ptr  | DE       |
| 2nd               | char     | pushed   |
| 3rd+              | any      | pushed   |
| Return char       |          | A        |
| Return int        |          | HL       |

`w5100_write_reg` declares `val` as `unsigned int` (not `unsigned char`) so
SDCC passes it in DE — the low byte E is then used directly in `OUT (C), E`.
Declaring it as `unsigned char` would cause SDCC to push it on the stack instead.

`cas_out_open` declares `flen` as `int` for the same reason — it lands in DE/E,
avoiding the `(ptr, char)` path where the char is pushed and requires manual
stack cleanup in `__naked` asm.

`__naked` functions used for firmware calls must not use `4(ix)` to access
parameters — SDCC omits the IX frame for functions whose body is pure inline
asm.  Use the register the calling convention already places the argument in.

## Memory map

| Range           | Use                                                        |
|-----------------|------------------------------------------------------------|
| 0x0000–0x3FFF   | Lower ROM (BASIC/firmware)                                 |
| 0x3E00–0x3E7F   | wget: hostname (filled by BASIC loader)                    |
| 0x3E80–0x3EFF   | wget: path (filled by BASIC loader)                        |
| 0x3F00–0x3F0D   | wget: filename, length, port (filled by BASIC loader)      |
| 0x3F10–0x3F1F   | N4C.CFG config block (ULIfAC build, filled by BASIC loader)|
| 0x4000–0x6FFF   | Program code (`--code-loc 0x4000`)                         |
| 0x6800–0x6FFF   | telnet: Code Page 437 charset (loaded by BASIC at runtime) |
| 0x7000–0xBFEF   | Static data and BSS (`--data-loc 0x7000`)                  |
| 0xBFF0–0xBFFF   | Stack (grows down from 0xBFF0)                             |
| 0xC000–0xFFFF   | Screen RAM + upper ROM                                     |

## Acknowledgements

- **[llopis/amstrad-diagnostics](https://github.com/llopis/amstrad-diagnostics)** — CPC model detection signatures and RAM test patterns used as reference for `src/cpcdetect.c`.
