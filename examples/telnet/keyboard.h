#ifndef KEYBOARD_H
#define KEYBOARD_H

/*
 * CPC keyboard remapping for the telnet session.
 *
 * setup_key_translations() reprograms cursor and ESC keys via KM_SET_TRANSLATE
 * so that KM_READ_CHAR returns unambiguous control codes during the session.
 * restore_key_translations() restores the original firmware translations on exit.
 *
 * After setup_key_translations(), KM_READ_CHAR returns:
 */
#define KEY_ESC         0x1B    /* ESC key (was 0x7C '|') */
#define KEY_CURSOR_UP   0x0B    /* cursor up   (was ~0x72 'r') */
#define KEY_CURSOR_DOWN 0x0A    /* cursor down (was ~0x73 's') */
#define KEY_CURSOR_LEFT 0x08    /* cursor left (was ~0x70 'p') */
#define KEY_CURSOR_RIGHT 0x0E   /* cursor right (was ~0x71 'q') — distinct from tab 0x09 */

void keyboard_init(void);
void keyboard_restore(void);

#endif
