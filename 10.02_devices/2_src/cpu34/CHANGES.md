# Changes to the KD11-EA core (PDP-11/34)

Change log of this directory only, newest first. **Every change to these files is recorded here,
and defect fixes are recorded here only** — the repo-wide `CHANGES.md` in the root carries an entry
only when the core gains or loses functionality at feature level (a new CPU model, the MMU, an
FPU), never one per diagnostic made to pass.

Files: `kd11ea.c`/`kd11ea.h` (the CPU core), `kt11d.c`/`kt11d.h` (the memory management unit),
`11.h` (basic types, private to this directory). The ARM-side wrapper is `../cpu34.cpp`/`.hpp`, the
contract to the QUNIBUS adapter is `../cpu_bus_adapter.h`.


## Unreleased

### Traps: three defects found by FKABD1

`FKABD1`, the 11/34 trap test, lost control after 3480 instructions and ended in a double bus error.
Three defects in `kd11ea.c`, each uncovered by fixing the one before it; the tape now runs clean
(319027 instructions per pass).

- **The trap sequence did not end at an instruction boundary.** After pushing PS and PC and loading
  the vector, `step()` returned instead of arbitrating again, so a trap or interrupt which became
  pending *during* the trap sequence was only taken after the handler's first instruction. That is
  wrong for the kernel stack limit above all, since it is the trap sequence's own pushes which
  violate it: the tape traps into a handler with an illegal `JSR`, expects `SP` two words lower on
  entry than the trap left it — the stack trap taken in between — and its handler's first
  instruction repoints vector 4 at a catcher, so one instruction of delay sent the stack trap to the
  wrong place and walked the stack down through 0 into the I/O page. `trap:` now falls through to
  `service:`, which is what the `// TODO: is this correct?` there asked about.
- **An autoincrement stood after the reference it addressed had aborted.** `(R0)+` computes the
  address from the old register and writes the new one back, but on the KD11-EA that write-back does
  not survive a bus timeout or an MMU abort of the cycle it addresses. The tape finds the first
  nonexistent address with `TSTB (R0)+`, records `R0` in its trap 4 handler, and then aborts on the
  same address with `TSTB -(R0)` from one above it and re-executes that — which only lines up if the
  increment was backed out and the decrement, which is part of forming the address, was not.
  `addrop()` now hands the pending increment to `dati()`/`dato()`, which commit it when the cycle
  completes and undo it when it does not. `MMR1` is left recording the change, frozen as the
  hardware freezes it. Not carried over to `cpu20/ka11.c`: no tape decides it there.
- **`MOV` dropped a failed destination write.** Alone among the instructions which write memory it
  ignored the result of `writedest()`, so a bus timeout or MMU abort on the destination of a `MOV`
  silently did nothing instead of trapping. The tape catches it with a `MOV` to a read-only page.

Also in this tape's way: the `printf()` for an unimplemented 0700xx instruction, which `FKABD1`
sweeps to check that the whole group traps as reserved — 200 lines of debug output per pass. It is a
`trace()` now.

### The basic instruction set: five defects found by FKAAC0

`FKAAC0`, the 11/34 basic instruction test, halted after 2206 instructions. Fixing what it reported
uncovered the next defect each time, five in a row, all in `kd11ea.c`. The tape now runs clean.

- **Operand evaluation order.** The `RD_B`/`RD_U` macros evaluated *memory* operands first and
  *register* operands afterwards, so `MOV R0,(R0)+` fetched the source after the destination had
  already autoincremented that same register: it stored 000002 where the tape expects 000000. The
  order is now source strictly before destination. What the old order was working around is that
  `addrop()` overwrites `cpu->ba`, which after the destination has been evaluated must still address
  it for `writedest()`; a register operand therefore goes through `fetchop()` alone, never
  `readop()`, and never touches `ba`. Also the order in which `MMR1` records the two register
  changes, which is what an abort handler undoes.
  This one is **not** to be copied into `cpu20/ka11.c`, where the same macros read the same way:
  whether a register source sees the destination's autoincrement/autodecrement is a documented
  family difference. *PDP-11 Architecture Handbook* (1983), appendix B "PDP-11 Family Differences":
  the 23/24, 15/20, 25/40, 60, J-11 and T-11 modify the register before using it as the source, the
  04, 05/10, 34, 44, 45, 70, LSI-11 and VAX do not. The KD11-EA belongs to the second group - which
  is what `FKAAC0` tests - and the KA11 to the first, so the 11/20 core keeps its order.
- **`JMP`/`JSR` in mode 2.** Both jumped to `cpu->b`, which for autoincrement is the register value
  *after* the increment: `MOV #15536,R0` / `JMP (R0)+` landed at 015540 instead of 015536. They now
  use the effective address `cpu->ba`. The two are the only readers of the `b` field, and `b`
  differs from `ba` only in this one mode.
- **`SXT` (0067DD) was not implemented** and trapped as a reserved instruction — the word form of
  the opcode whose byte form is `MFPS`, just as `MARK` is the word form of `MTPS`. Destination gets
  -1 if N is set and 0 if it is clear; `setnz()` then leaves N as it was and sets Z exactly when N is
  clear, which is the rule, and C is not affected.
- **`MARK` (0064NN) was not implemented** either, same trap. `SP <- PC + 2*NN; PC <- R5;
  R5 <- (SP)+`, condition codes untouched.
- **The T bit was writable.** `MTPS` and a bus write to 777776 both set PSW<4>, so the tape got a
  trace trap through vector 14 where it expects none, and read its PSW back as 000377 instead of
  000357. Only `RTI`/`RTT` can set T; an explicit write now leaves it alone.

None of the five is carried over to `cpu20/ka11.c`; all five are 11/34 behaviour that the 11/20
either differs in or does not have. The operand order is the family difference above. The 11/20 has
neither `SXT` nor `MARK`. Its `JMP`/`JSR` keep reading `cpu->b`, the incremented register — that is
what the field was introduced for ("B register before BUT JSRJMP" in `ka11.h`), i.e. deliberate KA11
behaviour. And its PSW is written straight through in `ka11.c` `dato()`, with no MTPS to protect
against. The 13 `ZKA*` tapes decide none of it: they pass on both cores, and none of them ever
executes a double operand instruction with a register source and the same register as an
autoincrement/autodecrement destination (checked by instrumenting `step()` for all 13 runs).

### MFPS with a register destination no longer aborts the emulator

`FKTHB0` killed the process outright — `Assertion '0' failed` in `addrop()`, no failure line at all,
and on a BeagleBone the same path would abort `demo`. The instruction is `MFPS R1` at 020566, the
read-back half of the diagnostic's MTPS/MFPS walk of the priority field. `addrop()` computes an
address, so it starts at mode 1 and asserts on mode 0; every other direct caller guards that
(`MFPI`/`MTPI` branch to a register-mode path, `JSR`/`JMP` do `goto ill`, the `RD_U` macro is itself
`if(dm != 0) …`) and the MFPS case was the one written without a guard. `MFPS Rn` is a legal
KD11-EA instruction, not something that should trap.

`addrop()` is now called only for `dm != 0`, and three further defects on the same lines went with
it:

- The destination was written as a word (`by = 0`). MFPS writes one byte to memory, and sign extends
  PS<7> through the whole word into a register, like MOVB.
- `addrop()` was passed byte flag 0, so `(R0)+`/`-(R0)` stepped by 2 instead of 1.
- No condition codes were set at all. MFPS sets N from PS<7> and Z from the byte, clears V and
  leaves C.

### HALT and RESET are kernel-only

Both privileged instructions executed in any processor mode: a user-mode HALT stopped the machine
and a user-mode RESET pulsed bus INIT. On real hardware neither can happen — a user program must not
be able to stop the processor or initialize the bus. `FKTDA1` tests exactly this and died on it
after 58 instructions.

HALT outside kernel mode is a reserved instruction and traps through vector 10 (`goto ri`) instead
of halting; RESET outside kernel mode is a no-op, so `kd11ea_reset()`/`unibone_bus_init()` only run
in kernel mode. Not carried over to `cpu20/ka11.c`: an 11/20 has no processor modes.

### The PSW is addressable at 777776 again

Removing it with the KT11-D work below was wrong. `dati()`/`dato()` answered
`case 0777776: case 0777777:` with a bus timeout, which is inherited KA11 behaviour; the KD11-EA has
MFPS/MTPS *as well as* the PSW address, not instead of it, and 777776 is the only way to reach the
mode and priority bits. Six of the ten XXDP diagnostics died on their first access to it, after as
few as 3 instructions.

- `dati()` returns `cpu->psw` for 777776/777777. A byte read of the odd address gets PSW<15:8>, the
  caller shifting the half down as it does for any register.
- `dato()` routes the write through `kd11ea_set_psw()`, so a changed mode switches the stack pointer
  and the KT11-D address space and a changed PSW<7:5> reaches the arbitrator. A DATOB writes only
  the addressed half, using the same `mask` as the KT11-D registers.
- New `PSW_MASK` (0170377) is applied inside `kd11ea_set_psw()` rather than at the bus, so the bits
  an 11/34 has no flipflop for — PSW<11>, there being one register set only, and the unused <10:8> —
  can never be loaded from any source: 777776, MTPS, RTI/RTT or a trap vector.

The PSW is still not published as a QUNIBUS register of `cpu34_c`, so it remains visible to the
emulated CPU only, exactly like the KT11-D registers and like `cpu20_c`.

### KT11-D memory management

The core was a fork of the 11/20 KA11 and had no memory management, which limited the emulated 11/34
to 28K words and made RSX-11M, RT-11 XM and the KT11-D diagnostics impossible to run. It now has the
complete KT11-D, including the kernel/user processor modes which come with it.

**Design decisions worth knowing**

- The MMU registers are *not* published as QUNIBUS registers. On real hardware the KT11-D sits inside
  the KD11-EA and does not answer as a bus slave, so they are decoded by `kd11ea.c` `dati()`/`dato()`
  on the translated physical address, like the console switch register already was. `cpu34_c` keeps
  `register_count = 0`. This also keeps all MMU state in cached ARM memory, which matters: the
  translation runs on every CPU memory access, and a read of shared PRU RAM there would be uncached.
- Address translation is *not* offloaded to a PRU. The virtual address is produced in the ARM
  instruction loop, so a PRU would need a mailbox round trip (~1 µs) where the ARM needs ~20 ns; PRU1
  also has no timing slack, and the KT11-D has no UNIBUS map, so device DMA is never relocated. The
  PRU already receives the translated physical address for free through the existing mailbox field.
  For scale: one CPU memory access costs ~1 µs with PMI and ~10 µs over the real bus, so the
  translation is ~1-2 % of a PMI access and ~0.1-0.2 % of a bus cycle.

**New: `kt11d.c`, `kt11d.h`**

- MMR0..MMR2, kernel and user PAR/PDR blocks, 16 → 18 bit relocation with page length and access
  checks, aborts through vector 0250, MMR0<15:13> freeze of MMR0<6:1>/MMR1/MMR2, and the MMR1
  register-change log.
- `kt11d_relocate()` is inline in the header and works from a precomputed 16-entry `kt11d_page_t`
  array. One unsigned compare `(blk - blk_lo) > blk_span` covers both expansion directions; a `deny`
  bitmask covers the non-resident and read-only cases in one test. `kt11d_abort()` recomputes the
  exact cause from the raw PDR, so the bit semantics stay off the hot path.
- Coherency of the derived array: rebuilt only by `kt11d_rebuild()` from `kt11d_write_reg()` on a
  PAR/PDR write, and by `kt11d_rebuild_all()` on an MMR0 write and from `kt11d_reset()`. The W bit is
  written into the raw PDR only, which is why the raw `par[]`/`pdr[]` are kept alongside the cache -
  it never needs an invalidation.
- `kt11d_format()` renders the registers for the CPU state dump.

**Note on 11/34 versus 11/45.** Do not copy KT11-C behaviour into this file. The KT11-D has no MMR3,
no supervisor mode, no I/D separation, no memory management *traps* (only aborts) and no PDR A bit.
Its ACF is `PDR<2:1>`, two bits, not the 3-bit field of the KT11-C.

*Unverified against the manual*, isolated so they are a one-line fix in `kt11d_rebuild()` and the
`KT11D_*_WRITABLE` constants if `EK-KD11A-TM` disagrees: the exact ACF encoding, and the masking of
the PAR to 12 bits.

**Changed: `kd11ea.c`, `kd11ea.h`**

- `psw` widened from `byte` to `word` for the mode fields, second stack pointer in `stackpointer[]`,
  `trap_vector` so the bus-error path can trap through 4 or 0250.
- `ubxt()` replaced by `kt11d_relocate()`.
- New `kd11ea_set_psw()` is the single place which assigns the PSW: it switches the stack pointer on
  a mode change, tells the MMU which address space is current, and calls
  `unibone_prioritylevelchange()`. Instructions which only touch the condition codes still assign
  `cpu->psw` directly - going through the setter on every opcode would call into the arbitrator each
  time.
- CPU internal registers are decoded on the *physical* address now. With relocation on the IO page is
  reached through a PAR, so the old virtual `(ba & 0177400) == 0177400` test was wrong.
- The trap sequence was reordered: the vector is fetched in kernel space first, the new PSW gets its
  previous-mode field, and only then are the old PSW and PC pushed - onto the new mode's stack. The
  old order pushed onto the old stack, which is correct only for a single-mode 11/20.
- MFPI/MTPI (and MFPD/MTPD, identical here since there is no I/D space) and RTT implemented; they
  used to take the reserved instruction trap. RTI/RTT may not change the mode or priority fields in
  user mode, and RTI takes a pending trace trap immediately where RTT defers it by one instruction.
  These are really implemented on the 11/34 and even the D variants are decoded; these only got their
  new meaning in the pdp 11/45.
- The stack limit red zone applies to the kernel stack only.
- The RESET opcode no longer resets the memory management: bus INIT does not reach the KT11-D on real
  hardware, and an OS executing RESET must keep its address map. `kd11ea_reset()` is now the RESET
  opcode only; console START and power-up go through the new `kd11ea_power_reset()`, which also
  clears the MMU, the PSW and both stack pointers. `cpu34_c::core_reset()` calls the latter.
- Fixed: MTPS changed PSW<7:5> without telling the arbitrator, leaving
  `mailbox->arbitrator.ifs_priority_level` stale.
- Removed the PSW at 777776. The 11/34 has no bus-addressable PSW - that is why it has MFPS/MTPS - so
  an access there is a bus timeout now. Programs which wrote the CPU PSW through 777776 must use MTPS.
- `kd11ea_printstate()`/`kd11ea_tracestate()` show the 16 bit PSW, the processor mode, both stack
  pointers and the full MMU register set, since the MMU registers cannot be read over the bus.

**Left alone deliberately.** MTPS can still write the priority field and the T bit. Protection
semantics suggest user mode should not be able to, but this predates the MMU work and was not
confident enough to change; check against the manual.

Verified: cross-compile only (`./crossco -a`, UNIBUS, clean under `-Wall -Wextra -Wshadow`). Not yet
run on real hardware; the KT11-D diagnostics are the acceptance test still to do.

### EIS and MFPS/MTPS are native instructions

The 11/20 feature switches were removed from the KA11 core, so the conditionals around them here are
gone too and the instructions are always executed: EIS (MUL, DIV, ASH, ASHC, XOR, SOB, opcode group
0070000) and MTPS/MFPS (0006400/0006700). The byte bit test in MTPS/MFPS stays - without it
006400/006700 are MARK/SXT, not MTPS/MFPS. `KD11EA_SWAB_VBIT` remains compiled in: SWAB clears the
V bit.

### Directory created

`kd11ea.c`, `kd11ea.h`, `11.h` forked from `cpu20/ka11.c`, `ka11.h`, `11.h` (Angelo Papenhoff's
KA11). All externally visible symbols renamed `kd11ea_*` / `KD11EA` and internal helpers made
`static`, so both cores link into one binary. `cpu34/11.h` must never be included together with
`cpu20/11.h` in one compilation unit.

At that point the fork still executed the 11/20 instruction set, without memory management.


## Known gaps

- Console ODT is not emulated.
