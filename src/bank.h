#ifndef BANK_H
#define BANK_H

/*
 * iRAM1024 DK'Tronics-compatible banking driver.
 *
 * OUT (&7F), value   where value = 0xC0 | (bank<<3) | cfg
 *   bits 7:6 = 11   enable expansion RAM
 *   bits 5:3 = bank  bank number 0-7 (bank 0 = built-in CPC RAM; use 1-7)
 *   bits 2:0 = cfg   0 = map to &4000, 1 = map to &C000
 *
 * OUT (&7F), 0       disable expansion RAM, &C000 reverts to CPC screen/ROM.
 */

#define BANK_CFG_C000  1u   /* maps 16 KB of selected bank to &C000-&FFFF */

/* Select expansion RAM: bank (1-7) mapped to the window specified by cfg.
 * cfg is declared unsigned int so sdcccall(1) passes it in DE (low byte E). */
void bank_select(unsigned char bank, unsigned int cfg);

/* Deselect expansion RAM: &C000 reverts to normal CPC screen RAM / upper ROM. */
void bank_restore(void);

#endif /* BANK_H */
