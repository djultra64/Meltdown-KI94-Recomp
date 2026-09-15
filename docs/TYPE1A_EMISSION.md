# Receiver-side Endokuken particle emission

## Recovered boundary

`native/src/original/ki15d_88003d30.c` reconstructs KI arcade v1.5d block
`0x88003d30..0x88003d70`. It runs inside the receiver's updater, calls the
already verified particle constructor `0x8800b1fc` when due, and exits at the
original continuation boundary `0x88002088`. It is **not** a recovered main
loop, contact initializer, or displayed-frame scheduler.

Its only direct state is receiver bytes `c4` and `c5`:

1. If `c4` is zero, return without reading `c5` or writing memory.
2. Read `c5`, decrement `c4`, then store `c5 + 0x10` as a byte.
3. Compare the high part of the **untruncated register sum** with its low
   nibble. If the high part is smaller, leave without constructing a particle.
4. Otherwise reset `c5` to the low nibble in the original call's delay slot,
   then run the constructor with the receiver and original `$gp` orientation.

For example, `c5=0xff` stores `0x0f`, but the comparison is `16 >= 15`, so it
emits. A byte-only calculation would get this branch wrong. A nonzero `c4`
that decrements to zero can still emit on that invocation.

The constructor sees the already decremented countdown, which controls motion
and scale. Its saved return-address write is exactly `0x88003d6c`. Allocation
retains the original behavior: skip occupied records among 29 searched slots;
if all are occupied, clear/use `0x8808db00` without checking whether that fallback
is itself occupied. The port must not silently replace that behavior with an
invented “pool full” result. Host memory mapping errors are separate and are
reported without rolling back earlier guest writes.

## Organic contact evidence

The controlled Jago-versus-idle-Fulgore run reaches the initializer around
`0x8800a2e4`. Executing registers at its stores show receiver `c4/c5/c6/c7`
becoming `31/78/55/00` (hex). The descriptor/table memory observations identify
candidate initializer inputs but are not a reconstruction of all hit types.
The initializer remains outside the emitter boundary above, but its separate
bounded reconstruction and verification are documented under “Contact
initializer” in `docs/VERIFICATION.md`.

There are 49 eligible receiver-emitter invocations. The observed allocation
sequence is:

| Particle | Emitter invocation (decimal) | Visible group (decimal) | Constructor countdown (hex) | Record |
|---|---:|---:|---|---|
| 1 | 0 | 0 | 30 | 8808c000 |
| 2 | 8 | 11 | 28 | 8808be00 |
| 3 | 16 | 19 | 20 | 8808bf00 |
| 4 | 24 | 27 | 18 | 8808c100 |
| 5 | 32 | 35 | 10 | 8808c200 |
| 6 | 40 | 43 | 08 | 8808c300 |
| 7 | 48 | 51 | 00 | 8808c000 (reused) |

Particles continue updating in visible groups 1, 2 and 3 without receiver
emitter invocations. Thus the first spacing is 11 visible groups, then eight.
This is consistent with the visible hit pause, but the branch that causes the
three omitted invocations has **not** been recovered. Calling the emitter once
per display frame would be incorrect; hardcoding three skipped frames in
production would also be an unsupported replacement for original game logic.

The complete observed impact has 322 particle updates, 315 render calls across
96 visible groups, a six-particle peak, and seven releases at groups
45, 56, 64, 72, 80, 88 and 96. Each particle is updated 46 times: 45 visible
updates followed by the terminating update. Update traversal is ascending
record address; draw traversal is different and matches the existing POC-110
oracle's ordered `(group, record, animation token)` sequence exactly.

Visible groups are indexed by changes in the executing framebuffer-selector
register, not by reading a potentially stale cached global. The first bank's
absolute parity is not used to compare runs. These are observational indices,
not an implementation of vertical interrupts. The event digest still includes
the observed bank values and all logged event fields for repeatability.

## Verification and reproduction

Run from the project root, with the existing lawful local inputs and extracted
segment available. Output logs and regenerated files stay under ignored `work/`.

```sh
flatpak run org.mamedev.MAME kinst -rompath assets/ki1 -debug -nodrc \
  -debugscript mame/oracle_ki15d_88003d30.cmd \
  -video none -sound none -nothrottle -skip_gameinfo -seconds_to_run 5 \
  -cfg_directory work/mame/cfg -diff_directory work/mame/diff -oslog \
  > work/mame/traces/oracle-ki15d-88003d30.log 2>&1
python3 tools/analyze_type1a_emission.py oracle \
  work/mame/traces/oracle-ki15d-88003d30.log \
  --output work/emitter-regenerated.csv
cmp tests/fixtures/ki15d_88003d30.csv work/emitter-regenerated.csv

flatpak run org.mamedev.MAME kinst -rompath assets/ki1 -debug \
  -debugscript mame/trace_ki15d_type1a_emission.cmd \
  -autoboot_script mame/autoplay_jago_vs_idle_fulgore.lua \
  -video none -sound none -nothrottle -skip_gameinfo -seconds_to_run 35 \
  -cfg_directory work/mame/cfg -diff_directory work/mame/diff -oslog \
  > work/mame/traces/type1a-emission.log 2>&1
python3 tools/analyze_type1a_emission.py organic \
  work/mame/traces/type1a-emission.log
make check
```

The decision oracle executes all 65,536 possible byte pairs in MAME 0.289 with
the original 68-byte block. Only the constructor body is intercepted, **after**
the original `jal` delay slot executes. Test-only nop landing sites outside the
block let the debugger cycle inputs; no original block instruction is patched.
The script records real case counts and a completion marker. The analyzer
rejects truncated, reordered, duplicated, malformed or incorrectly counted
oracle output rather than producing a partial fixture.

The compact tracked CSV has one row per initial countdown. Within each row,
cadences run from 0 through 255. Start a uint32 digest at `0x811c9dc5`; for each
result compute `word = c4_after<<16 | c5_after<<8 | constructor_called`, then
`digest = (digest XOR word) * 0x01000193` modulo 2^32. These are word-wise test
digests, not a claim of byte-wise FNV encoding or cryptographic proof.

The native decision test links a **test-only constructor observer**, checks
the entire mapped 1 KiB against expected emitter-only writes, and verifies
the receiver, orientation, fixed return address and already-reset cadence at
the call boundary. It compares all 256 per-countdown digests with the original
CPU fixture. Production code has no observer or captured scheduling table.

The separate real-constructor integration test covers the observed 49-step
byte-state sequence with zero, one and maximum uint32 `$gp`; occupied records;
the occupied pool fallback; and memory failures. It compares the entire 1 MiB
image with explicit post-emitter state composed with the previously verified
constructor. This is a dependency-composition check, not a new full-constructor
MAME oracle. The original constructor's own differential tests remain in the
suite. Native tests print the first differing input or RAM offset on failure.

Repeated organic runs produce normalized event SHA-256
`3832158bb3cf81ba6ead126d9746cd98dcbb529dbabfe3e1b784485975de87bf`.
Exploratory logs mislabeled allocator `$a2` as `countdown`; that value was the
allocated address and is excluded by the analyzer when comparing those old
local logs. The checked-in script no longer emits the mislabeled field.

No PC demo behavior changes in this step. The bounded frame-selection slice is
now independently executable, but contact initialization, production object
ordering, projection, and wider frame-table production still need recovery and
integration before removing the demo's capture scaffolding.
