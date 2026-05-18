#include "../../src/cpcbios.h"

void main(void) {
    cpc_set_mode(1);            /* mode 1: 320x200, 4 colours */
    cpc_cls();
    cpc_print("Hello, CPC!\r\n");
    cpc_print("Built with SDCC.\r\n");

}
