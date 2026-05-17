;--------------------------------------------------------------------------
; crt0.s -- Minimal CPC startup stub for SDCC -mz80
;
; Linked first so it lands at --code-loc (0x4000).
; Sets up the stack, calls main(), then halts.
; Load on CPC with: LOAD "PROG.BIN",0x4000 : CALL 0x4000
;--------------------------------------------------------------------------
        .module crt0
        .globl  _main

        .area   _CODE

        ld      sp, #0xBFF0     ; top of free RAM, below screen at 0xC000
        call    _main
_exit::
        halt                    ; stop CPU; reset CPC to get back to BASIC
