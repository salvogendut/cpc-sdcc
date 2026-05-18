#ifndef SCREEN_H
#define SCREEN_H

#define SCREEN_COLS 80
#define SCREEN_ROWS 25

extern unsigned char cursor_col;
extern unsigned char cursor_row;
extern unsigned int  screen_offset;

void screen_init(void);
void screen_write(unsigned char c);
void screen_cls(void);
void screen_blank_at(unsigned int addr, unsigned int count);
unsigned int screen_find_cursor(void);
unsigned int screen_find_cursor_at(unsigned char col, unsigned char row);
void screen_set_fg(unsigned char ink);
void screen_set_bg(unsigned char ink);

/* Called by screen_write; implemented in ansi.c */
void ansi_feed(unsigned char c);

#endif
