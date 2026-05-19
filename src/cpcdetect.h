#ifndef CPCDETECT_H
#define CPCDETECT_H

#define CPC_MODEL_UNKNOWN  0
#define CPC_MODEL_464      1
#define CPC_MODEL_664      2
#define CPC_MODEL_6128     3
#define CPC_MODEL_464PLUS  4
#define CPC_MODEL_6128PLUS 5
#define CPC_MODEL_GX4000   6

/* Returns one of the CPC_MODEL_* constants above. */
unsigned char cpc_detect_model(void);

/*
 * Returns base RAM in KB (64 or 128).
 * On 464/664 this is always 64. On 6128/Plus it is 128.
 * Does not test for expansion RAM.
 */
unsigned int cpc_detect_ram_kb(void);

#endif
