# cpc-sdcc

C development for the Amstrad CPC using [SDCC](https://sdcc.sourceforge.net/), targeting the Z80.  
Includes a W5100S Ethernet driver for the [Net4CPC](https://www.cpcwiki.eu/index.php/Net4CPC) hardware.

## Prerequisites

- **SDCC 4.x** with Z80 target (`sdcc`, `sdasz80`, `makebin`)  
  Set `SDCC_BIN` in each build script to your SDCC `bin/` directory.

## Project structure

```
src/
  crt0.s        Startup stub: sets SP=0xBFF0, calls main(), halts
  cpcbios.h     CPC firmware wrappers (TXT_OUTPUT, SCR_SET_MODE, CLS)
  w5100.h       W5100S register map and low-level I/O prototypes
  w5100.c       w5100_read_reg / w5100_write_reg (__naked asm), buffer ops
  netinit.h/c   Write IP/mask/gateway/MAC/DNS into W5100S registers
  net.h/c       TCP socket API (socket 0): open/connect/send/recv/close

examples/
  hello/        Prints "Hello, CPC!" using firmware text output
  tcptest/      Opens a TCP connection and performs an HTTP GET
```

## Building

Each example has its own `build.sh`.  Output binaries go to `bin/`.

```bash
cd examples/hello
./build.sh          # produces ../../bin/HELLO.BIN

cd examples/tcptest
./build.sh          # produces ../../bin/TCPTEST.BIN
```

Load and run on the CPC:

```
LOAD "HELLO.BIN",0x4000 : CALL 0x4000
```

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

## W5100S hardware (Net4CPC)

| Port   | Purpose                          |
|--------|----------------------------------|
| 0xFD20 | Mode register (reads 0x03)       |
| 0xFD21 | High address byte                |
| 0xFD22 | Low address byte                 |
| 0xFD23 | Data (auto-increments address)   |

Socket 0 is used for TCP.  Socket 1 is available for UDP/DNS (not yet ported).

## TCP API

```c
#include "src/netinit.h"
#include "src/net.h"

static const net_config_t cfg = {
    { 192, 168,   1, 100 },   /* ip      */
    { 255, 255, 255,   0 },   /* netmask */
    { 192, 168,   1,   1 },   /* gateway */
    {   8,   8,   8,   8 },   /* dns     */
    { 0x00, 0x08, 0xDC, 0x01, 0x02, 0x03 }  /* mac */
};

net_init(&cfg);
net_socket_open();
net_connect(ip4, port);
net_send(buf, len);
len = net_recv(buf, sizeof(buf));
net_close();
```

## Memory map

| Range           | Use                              |
|-----------------|----------------------------------|
| 0x0000–0x3FFF   | Lower ROM (BASIC/firmware)       |
| 0x4000–0x6FFF   | Program code + data (`--code-loc 0x4000 --data-loc 0x7000`) |
| 0x7000–0xBFEF   | Heap / static data               |
| 0xBFF0–0xBFFF   | Stack (grows down from 0xBFF0)   |
| 0xC000–0xFFFF   | Screen RAM + upper ROM           |
