# cpc-sdcc

C development for the Amstrad CPC using [SDCC](https://sdcc.sourceforge.net/), targeting the Z80.  
Includes a W5100S Ethernet driver for the [Net4CPC](https://www.cpcwiki.eu/index.php/Net4CPC) hardware,
with TCP, UDP/DNS support, an HTTP file downloader (wget), and an NTP time client.

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
  w5100.h       W5100S register map and low-level I/O prototypes
  w5100.c       w5100_read_reg / w5100_write_reg (__naked asm), buffer ops
  netinit.h/c   Network init: net_init() and net_init_from_file()
  net.h/c       TCP socket API (socket 0): open/connect/send/recv/close
  udp.h/c       UDP socket API (socket 1): open/sendto/recv/close
  dns.h/c       DNS A-record resolver: dns_resolve()
  amsdos_wrap.py  Adds 128-byte AMSDOS type-2 binary header to a raw binary

examples/
  hello/        Prints "Hello, CPC!" and returns to BASIC
  tcptest/      Opens a TCP connection and performs an HTTP GET
  dnstest/      Resolves a hostname via DNS and prints the IP
  wget/         HTTP file downloader — prompts for URL, saves file to disk
  ntp/          NTP/SNTP time client — resolves time.cloudflare.com,
                displays current UTC date and time
```

## Building

Each example has its own `build.sh` that produces **two sets of binaries**
in a single run:

| Output directory | Target hardware          | BASIC loader   |
|------------------|--------------------------|----------------|
| `bin/`           | ULIfAC / real floppy     | `NAME.BAS`     |
| `bin/albireo/`   | Albireo / GoTek (Unidos) | `NAMEA.BAS`    |

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

cd examples/hello && ./build.sh
# bin/HELLO.BIN + bin/HELLO.BAS
```

Copy all files from the relevant output directory to a CPC disk and
`RUN` the `.BAS` loader.

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

**Albireo / GoTek (Unidos ROM)** — the binary opens `N4C.CFG` itself via
`CAS_IN_DIRECT`.  The BASIC loader just sets the screen mode, loads the
binary, and calls it.  The USB/FAT AMSDOS shifts CAS IN routine addresses
by +3 from the standard ROM; this is handled at compile time with
`-DAMSDOS_USB` (added automatically by the Albireo build pass).

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
char cpc_wait_key(void);           /* KM_WAIT_CHAR — blocks until keypress */
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

## W5100S hardware (Net4CPC)

| Port   | Purpose                        |
|--------|--------------------------------|
| 0xFD20 | Mode register (reads 0x03)     |
| 0xFD21 | High address byte              |
| 0xFD22 | Low address byte               |
| 0xFD23 | Data (auto-increments address) |

Socket 0 is used for TCP.  Socket 1 is used for UDP/DNS.

| Socket | TX base | RX base | Size |
|--------|---------|---------|------|
| 0 (TCP)| 0x4000  | 0x6000  | 2 KB |
| 1 (UDP)| 0x4800  | 0x6800  | 2 KB |

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
| 0x7000–0xBFEF   | Static data and BSS (`--data-loc 0x7000`)                  |
| 0xBFF0–0xBFFF   | Stack (grows down from 0xBFF0)                             |
| 0xC000–0xFFFF   | Screen RAM + upper ROM                                     |
