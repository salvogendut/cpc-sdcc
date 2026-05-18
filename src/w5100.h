#ifndef W5100_H
#define W5100_H

/* --- Net4CPC I/O ports --------------------------------------------------- */
#define W51MR   0xFD20  /* mode register — reads 0x03 when chip is present    */
#define W51HAD  0xFD21  /* high address byte                                   */
#define W51LAD  0xFD22  /* low address byte                                    */
#define W51DAT  0xFD23  /* data (auto-increments address after each access)    */

/* --- Common registers ----------------------------------------------------- */
#define N_MR     0x0000
#define N_GAR0   0x0001  /* gateway IP (4 bytes, big-endian)   */
#define N_SUBR0  0x0005  /* subnet mask (4 bytes)               */
#define N_SHAR0  0x0009  /* source MAC address (6 bytes)        */
#define N_SIPR0  0x000F  /* source IP (4 bytes)                 */
#define N_RTR0   0x0017  /* ARP retry time (2 bytes)            */
#define N_RCR    0x0019  /* retry count                         */
#define N_DNS0   0x0032  /* DNS IP stored in PPPoE dest field   */

/* --- Socket 0 registers (used for TCP) ----------------------------------- */
#define S0_MR      0x0400
#define S0_CR      0x0401
#define S0_IR      0x0402
#define S0_SR      0x0403
#define S0_PORT0   0x0404  /* source port (2 bytes, big-endian)  */
#define S0_DIPR0   0x040C  /* destination IP (4 bytes)           */
#define S0_DPORT0  0x0410  /* destination port (2 bytes)         */
#define S0_TX_FSR0 0x0420  /* TX free size (2 bytes)             */
#define S0_TX_WR0  0x0424  /* TX write pointer (free-running)    */
#define S0_RX_RSR0 0x0426  /* RX received size (2 bytes)         */
#define S0_RX_RD0  0x0428  /* RX read pointer (free-running)     */

/* --- Socket 0 TX/RX ring buffers ----------------------------------------- */
#define S0_TX_BASE 0x4000u
#define S0_TX_MASK 0x07FFu  /* 2 KB */
#define S0_RX_BASE 0x6000u
#define S0_RX_MASK 0x07FFu  /* 2 KB */

/* --- Socket commands ------------------------------------------------------ */
#define SCMD_OPEN    0x01
#define SCMD_LISTEN  0x02
#define SCMD_CONNECT 0x04
#define SCMD_DISCON  0x08
#define SCMD_CLOSE   0x10
#define SCMD_SEND    0x20
#define SCMD_RECV    0x40

/* --- Socket status -------------------------------------------------------- */
#define SSTAT_CLOSED      0x00
#define SSTAT_INIT        0x13
#define SSTAT_LISTEN      0x14
#define SSTAT_ESTABLISHED 0x17
#define SSTAT_CLOSE_WAIT  0x1C

/* --- Socket 1 registers (used for UDP/DNS) ------------------------------- */
#define S1_MR      0x0500
#define S1_CR      0x0501
#define S1_IR      0x0502
#define S1_SR      0x0503
#define S1_PORT0   0x0504  /* source port (2 bytes, big-endian)  */
#define S1_DIPR0   0x050C  /* destination IP (4 bytes)           */
#define S1_DPORT0  0x0510  /* destination port (2 bytes)         */
#define S1_TX_FSR0 0x0520  /* TX free size (2 bytes)             */
#define S1_TX_WR0  0x0524  /* TX write pointer (free-running)    */
#define S1_RX_RSR0 0x0526  /* RX received size (2 bytes)         */
#define S1_RX_RD0  0x0528  /* RX read pointer (free-running)     */

/* --- Socket 1 TX/RX ring buffers ----------------------------------------- */
#define S1_TX_BASE 0x4800u
#define S1_TX_MASK 0x07FFu  /* 2 KB */
#define S1_RX_BASE 0x6800u
#define S1_RX_MASK 0x07FFu  /* 2 KB */

/* --- Socket modes --------------------------------------------------------- */
#define SMODE_TCP 0x01
#define SMODE_UDP 0x02

/* --- Socket status (additional) ------------------------------------------ */
#define SSTAT_UDP 0x22

/* -------------------------------------------------------------------------
 * Low-level register access
 *
 * SDCC 4.x z80 sdcccall(1):  first int arg → HL,  second int arg → DE.
 * We declare val as unsigned int so it arrives in DE (E = data byte),
 * avoiding the messy one-byte stack-push that (int, char) would generate.
 * -------------------------------------------------------------------------*/

/* Read one byte from W5100S register at addr. */
unsigned char w5100_read_reg(unsigned int addr);

/* Write one byte (val & 0xFF) to W5100S register at addr. */
void w5100_write_reg(unsigned int addr, unsigned int val);

/* Copy len bytes from CPC RAM at buf to W5100S starting at w5100addr. */
void w5100_write_buf(unsigned int w5100addr, const unsigned char *buf,
                     unsigned int len);

/* Copy len bytes from W5100S starting at w5100addr into CPC RAM at buf. */
void w5100_read_buf(unsigned int w5100addr, unsigned char *buf,
                    unsigned int len);

#endif /* W5100_H */
