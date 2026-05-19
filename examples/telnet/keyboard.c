#include "keyboard.h"

/*
 * KM_GET_TRANSLATE (0xBB2A): A = key number → A = current char
 * KM_SET_TRANSLATE (0xBB27): A = key number, B = new char
 *
 * Keys remapped (key numbers are identical on CPC 464, 664, and 6128):
 *   66  ESC          → KEY_ESC          (0x1B)
 *    0  cursor up    → KEY_CURSOR_UP    (0x0B)
 *    2  cursor down  → KEY_CURSOR_DOWN  (0x0A)
 *    8  cursor left  → KEY_CURSOR_LEFT  (0x08)
 *    1  cursor right → KEY_CURSOR_RIGHT (0x0E)
 */
static unsigned char saved_keys[5];   /* ESC, up, down, left, right */

void keyboard_init(void) __naked {
    __asm
        ld a,#66
        call #0xBB2A
        ld (_saved_keys+0),a
        ld a,#66
        ld b,#27
        call #0xBB27

        ld a,#0
        call #0xBB2A
        ld (_saved_keys+1),a
        ld a,#0
        ld b,#11
        call #0xBB27

        ld a,#2
        call #0xBB2A
        ld (_saved_keys+2),a
        ld a,#2
        ld b,#10
        call #0xBB27

        ld a,#8
        call #0xBB2A
        ld (_saved_keys+3),a
        ld a,#8
        ld b,#8
        call #0xBB27

        ld a,#1
        call #0xBB2A
        ld (_saved_keys+4),a
        ld a,#1
        ld b,#14
        call #0xBB27

        ret
    __endasm;
}

void keyboard_restore(void) __naked {
    __asm
        ld a,(_saved_keys+0)
        ld b,a
        ld a,#66
        call #0xBB27

        ld a,(_saved_keys+1)
        ld b,a
        ld a,#0
        call #0xBB27

        ld a,(_saved_keys+2)
        ld b,a
        ld a,#2
        call #0xBB27

        ld a,(_saved_keys+3)
        ld b,a
        ld a,#8
        call #0xBB27

        ld a,(_saved_keys+4)
        ld b,a
        ld a,#1
        call #0xBB27

        ret
    __endasm;
}
