#ifndef NETINIT_H
#define NETINIT_H

typedef struct {
    unsigned char ip[4];
    unsigned char netmask[4];
    unsigned char gateway[4];
    unsigned char dns[4];
    unsigned char mac[6];
} net_config_t;

/*
 * Write ip/mask/gateway/mac/dns into W5100S registers and set retry timers.
 * Returns 0 on success, -1 if the chip is not responding.
 */
int net_init(const net_config_t *cfg);

/*
 * Read N4C.CFG from disk (key=value format), parse IP/MASK/GW/DNS,
 * and call net_init(). MAC is hardcoded as DE:AD:BE:EF:00:FF.
 *
 * N4C.CFG format (CR+LF line endings):
 *   IP=192.168.1.100
 *   MASK=255.255.255.0
 *   GW=192.168.1.1
 *   DNS=8.8.8.8
 *
 * Uses USB-drive-shifted CAS IN firmware addresses (CAS_IN_OPEN=0xBC77,
 * CAS_IN_CHAR=0xBC80, CAS_IN_CLOSE=0xBC7A).
 *
 * Returns 0 on success, -1 if file not found, -2 if chip not present.
 */
int net_init_from_file(void);

#endif /* NETINIT_H */
