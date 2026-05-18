---
name: feedback-sdcc-calling-convention
description: SDCC 4.x z80 sdcccall(1) register passing convention — empirically verified
metadata:
  type: feedback
---

SDCC 4.x `-mz80` uses **sdcccall(1)** by default. Register assignments verified by disassembly:

| Signature              | Arg 1 | Arg 2         | Arg 3   |
|------------------------|-------|---------------|---------|
| `(int)`                | HL    | —             | —       |
| `(char)`               | A     | —             | —       |
| `(int, int)`           | HL    | DE            | —       |
| `(char, int)`          | A     | DE            | —       |
| `(int, char)`          | HL    | pushed (1 byte, PUSH AF + INC SP trick) | — |
| `(int, int, int)`      | HL    | DE            | pushed  |
| char return            | A     | —             | —       |
| int/ptr return         | HL    | —             | —       |

**Why:** Confirmed by compiling a probe and disassembling the call sites.

**How to apply:**
- For `__naked` asm wrappers with a `(int, char)` signature, the char is at [SP+2] on function entry (no IX frame). Either access it via SP arithmetic or change the signature to `(int, int)` so the value lands in DE instead.
- Do NOT write `__naked` asm that uses `4(ix)` unless there is a non-asm C statement in the function body that forces SDCC to emit the `push ix; ld ix,#0; add ix,sp` prologue.
- For firmware calls that take a byte in A (TXT_OUTPUT, SCR_SET_MODE): `__naked` with a single `char` arg works perfectly — SDCC puts the arg in A and the firmware reads from A.
- `w5100_write_reg(addr, unsigned int val)`: declaring val as `unsigned int` (not char) puts it in DE (E = byte). This is intentional — `(int, char)` would push the char onto the stack instead.

See [[project-cpc-sdcc]] for where this was discovered.
