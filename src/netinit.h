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

#endif /* NETINIT_H */
