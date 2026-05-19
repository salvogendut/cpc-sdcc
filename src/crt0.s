;--------------------------------------------------------------------------
; crt0.s -- Minimal CPC startup stub for SDCC -mz80
;
; Linked first so it lands at --code-loc (0x4000).
; Sets up the stack, runs SDCC static initialisers, calls main(), then
; returns to BASIC.
; Load on CPC with: LOAD "PROG.BIN",0x4000 : CALL 0x4000
;--------------------------------------------------------------------------
        .module crt0
        .globl  _main
        .globl  s__DATA
        .globl  l__DATA

        .area   _CODE

        ; BASIC CALL &4000 pushed a 2-byte return address on the Z80 stack.
        ; Save SP in a local register — we cannot write to _bas_sp yet because
        ; the DATA section zero loop below will overwrite it.
        ld      hl, #0
        add     hl, sp              ; HL = BASIC's SP
        ld      (sp_save), hl       ; stash in _CODE (not in _DATA)

        ld      sp, #0xBFF0         ; top of free RAM, below screen at 0xC000

        ; Zero the entire DATA section.  makebin -p pads gaps in the ihex
        ; with 0xFF, so uninitialised (.ds) variables start as 0xFF instead
        ; of 0x00.  Clearing first, then running gsinit, gives the standard
        ; C guarantee: static variables without an explicit initialiser are 0.
        ; Note: arithmetic on external symbols (l__DATA - 1) is evaluated at
        ; assembly time as 0-1=0xFFFF, so do NOT use expressions here.
        ld      hl, #s__DATA
        ld      bc, #l__DATA
00001$:
        ld      (hl), #0
        inc     hl
        dec     bc
        ld      a, b
        or      c
        jr      nz, 00001$

        ; Now the DATA section is clear — safe to write _bas_sp.
        ld      hl, (sp_save)
        ld      (#_bas_sp), hl

        ; Run SDCC static-variable initialisers (_GSINIT → _GSFINAL).
        ; Without this, `static` variables with non-zero initialisers
        ; (e.g. the MAC address in net_init_from_file) are never written.
        call    gsinit

        call    _main
_exit::
        ld      hl, (#_bas_sp)      ; restore BASIC's stack pointer
        ld      sp, hl              ; SP now points at the BASIC return address
        ret                         ; pop it → back to BASIC

        ; Scratch word in CODE (survives the DATA zero loop).
sp_save:
        .dw     0

        .area   _DATA
_bas_sp:
        .dw     0

; gsinit marks the start of the _GSINIT section (immediately follows _HOME).
; The empty _GSINIT entry here establishes the link order HOME→GSINIT→GSFINAL
; so that calling gsinit executes all SDCC-generated static initialisers and
; then falls through into gsfinal which returns.
        .area   _HOME
gsinit::

        .area   _GSINIT

        .area   _GSFINAL
gsfinal::
        ret
