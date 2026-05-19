#include "netinit.h"

/* M4 network is pre-configured by the card itself — no init needed. */

int net_init_from_file(void) {
    return 0;
}

int net_init(const net_config_t *cfg) {
    (void)cfg;
    return 0;
}
