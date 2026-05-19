#include "cpcdetect.h"
#include "bank.h"

/*
 * CPC model detection by reading known signature bytes in the Lower ROM
 * (0x0000–0x3FFF), which is always mapped at startup.
 *
 * Byte signatures (from llopis/amstrad-diagnostics Model.asm):
 *   6128 : *(0x0006)==0x91 && *(0x0007)==0x05
 *   664  : *(0x0684)=='2'  (0x32)
 *   464  : *(0x0683)=='1'  (0x31)
 *
 * CPC Plus / GX4000 detection requires a 17-byte ASIC unlock sequence via
 * port 0xBC followed by ASIC register probing. This is not yet implemented;
 * a Plus running in compatibility mode will be reported as the base model
 * it emulates (usually 6128).
 *
 * Reference: https://github.com/llopis/amstrad-diagnostics
 */

unsigned char cpc_detect_model(void)
{
    /* 6128: two-byte signature at ROM offset 0x0006/0x0007 */
    if (*((volatile unsigned char *)0x0006) == 0x91 &&
        *((volatile unsigned char *)0x0007) == 0x05) {
        return CPC_MODEL_6128;
    }

    /* 664: single byte at ROM offset 0x0684 */
    if (*((volatile unsigned char *)0x0684) == '2') {
        return CPC_MODEL_664;
    }

    /* 464: single byte at ROM offset 0x0683 */
    if (*((volatile unsigned char *)0x0683) == '1') {
        return CPC_MODEL_464;
    }

    return CPC_MODEL_UNKNOWN;
}

unsigned int cpc_detect_ram_kb(void)
{
    volatile unsigned char *test_addr = (volatile unsigned char *)0xC100;
    unsigned char model;
    unsigned int total_kb;
    unsigned char bank;
    unsigned char saved;

    model = cpc_detect_model();

    /* 6128 has 128 KB built-in; banks 1-7 of the gate array address its own
     * extra pages, not DK'Tronics expansion — skip the expansion probe. */
    if (model == CPC_MODEL_6128 || model == CPC_MODEL_6128PLUS)
        return 128;

    total_kb = 64;

    for (bank = 1; bank <= BANK_MAX; bank++) {
        saved = *test_addr;

        /* Write sentinel to screen RAM, then swap in expansion bank at &C000.
         * If the bank exists the subsequent write goes there; screen RAM keeps
         * 0xAA.  DI prevents firmware ISR touching &C000 during the window. */
        *test_addr = 0xAA;
        __asm
            di
        __endasm;
        bank_select(bank, BANK_CFG_C000);
        *test_addr = 0x55;
        bank_restore();
        __asm
            ei
        __endasm;

        if (*test_addr == 0xAA)
            total_kb += 16;

        *test_addr = saved;
    }

    return total_kb;
}
