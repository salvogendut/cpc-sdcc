---
name: project-cpc-sdcc
description: "cpc-sdcc project — C for Amstrad CPC using SDCC, W5100S Ethernet TCP/DNS driver, Net4CPC hardware"
metadata:
  type: project
---

C development framework for the Amstrad CPC using SDCC 4.x (at /var/home/salvogendut/Dev/sdcc).  
Git remote: git@github.com:salvogendut/cpc-sdcc.git

**Why:** Explore writing CPC software in C rather than assembly, with real TCP/IP networking via the Net4CPC W5100S chip.

**How to apply:** Remember the sdcccall(1) calling convention (see [[feedback-sdcc-calling-convention]]), GSINIT must run before main, and all examples must have AMSDOS headers and clean exits.

## Source structure

- `src/crt0.s` — startup stub: saves BASIC SP, sets SP=0xBFF0, calls gsinit, calls main, restores BASIC SP and rets
- `src/cpcbios.h` — CPC firmware wrappers (__naked, A = char arg)
- `src/w5100.h/c` — W5100S low-level I/O; `write_reg(addr, unsigned int val)` so val lands in DE/E
- `src/netinit.h/c` — reads N4C.CFG (IP/MASK/GW/DNS), writes to W5100S registers; MAC in a static initialiser
- `src/net.h/c` — TCP socket 0 API: open/connect/send/recv/close/is_connected/rx_available
- `src/udp.h/c` — UDP socket 1 API: open/sendto/recv/close
- `src/dns.h/c` — DNS A-record resolver using UDP socket 1; 3000ms timeout via CPC frame counter
- `src/amsdos_wrap.py` — adds 128-byte AMSDOS type-2 binary header to a raw binary

## Build

Each example has its own `build.sh`. Raw binary → amsdos_wrap.py → BIN with header. Output to `bin/`. SDCC hardcoded at `/var/home/salvogendut/Dev/sdcc/bin`.

dnstest/tcptest have two variants: ULIfAC/floppy (default) and Albireo/USB (`-DAMSDOS_USB`).

## Hardware environment

- LAN-only: no external internet connectivity from the CPC (ISP transparent proxy). Gateway test is the valid TCP test.
- DNS server and gateway are read from W5100S registers (N_DNS0, N_GAR0) after net_init_from_file() — never hardcoded.
- W5100S I/O: 0xFD20–0xFD23. Socket 0 = TCP. Socket 1 = UDP/DNS.

## Critical bugs fixed

1. **GSINIT never called**: crt0.s originally used `halt` and never called gsinit. Static variable initializers (including `cfg.mac`) were never written. W5100S had all-zeros MAC → sent no Ethernet frames at all. Fixed by adding `call gsinit` and HOME/GSINIT/GSFINAL areas in crt0.s. **The empty `.area _GSINIT` declaration between _HOME and _GSFINAL is essential** — without it the linker places _GSFINAL before _GSINIT and gsinit immediately hits ret.

2. **`unsigned int` timeout in net_connect()**: max ~65535 iterations ≈ 2.9s. W5100S RTR×RCR retry cycle takes ~10s. Changed to `unsigned long timeout = 300000UL` (~25s). SR=0x15 (SYNSENT) was the symptom.

3. **AMSDOS header missing**: `LOAD "HELLO.BIN"` (without explicit address) causes CPC to treat the file as BASIC → syntax error. All BIN files must go through amsdos_wrap.py.

4. **`while(1){}` on exit**: hello/dnstest/tcptest originally froze. crt0.s now restores BASIC SP and rets cleanly.

5. **Hardcoded external IPs**: dnstest had hardcoded 8.8.8.8, tcptest had hardcoded example.com IP. Both now read from W5100S registers after init.

6. **GSFINAL ordering**: first fix attempt put `.area _GSFINAL` before `.area _GSINIT` in crt0.s — linker placed gsfinal's `ret` at the very start of gsinit, so calling gsinit returned immediately. Fixed by inserting an empty `.area _GSINIT` first.

## Working examples (all tested on real hardware)

- `examples/hello` — prints two lines, returns to BASIC
- `examples/tcptest` — reads GW from N_GAR0, connects to gateway:80, GET /, prints response and byte count
- `examples/dnstest` — reads DNS from N_DNS0, resolves example.com, prints IP or TIMEOUT
