#ifndef ANSI_H
#define ANSI_H

void ansi_feed(unsigned char c);
unsigned char ansi_active(void);  /* non-zero when mid-sequence */

#endif
