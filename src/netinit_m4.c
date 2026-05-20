#include "netinit.h"
#include "m4io.h"

int net_init_from_file(void) {
    m4_rom_init();  /* find M4 ROM slot so m4_select_rom() works before any 0xFF02/0xFF06 access */
    return 0;
}

int net_init(const net_config_t *cfg) {
    (void)cfg;
    return 0;
}
