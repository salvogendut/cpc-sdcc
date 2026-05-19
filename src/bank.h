#ifndef BANK_H
#define BANK_H

/*
 * iRAM1024 DK'Tronics/Yarek-extended banking driver.
 *
 * Standard DK'Tronics (banks 1-7, port 0x7F):
 *   OUT value = 0xC0 | (bank<<3) | cfg
 *   bits 7:6 = 11  RAM config command
 *   bits 5:3 = bank (64KB page, 1-7)
 *   bits 2:0 = cfg  1 = map bank-3 quadrant (last 16KB) to &C000-&FFFF
 *
 * Yarek extended (banks 8-14, port 0x7E = second 512KB block):
 *   Same data encoding, bank offset = bank - 8 in bits 5:3.
 *
 * Each bank_select maps exactly 16KB to &C000-&FFFF (the bank-3 quadrant
 * of the selected 64KB physical page).  Total accessible: 14 × 16KB = 224KB.
 */

#define BANK_CFG_C000  1u   /* maps 16KB of selected bank to &C000-&FFFF */
#define BANK_MAX       14u  /* highest usable bank number (7 block-0 + 7 block-1) */

/* Select expansion RAM: bank (1-BANK_MAX) mapped to &C000.
 * cfg is declared unsigned int so sdcccall(1) passes it in DE (low byte E). */
void bank_select(unsigned char bank, unsigned int cfg);

/* Deselect expansion RAM: &C000 reverts to normal CPC screen RAM / upper ROM. */
void bank_restore(void);

#endif /* BANK_H */
