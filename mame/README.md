# MAME automation

The detected installation is MAME 0.289 through Flatpak. Scripts in this
directory run from the repository root and write exclusively under `work/`.

`capture_boot.cmd` runs for ten emulated seconds and captures:

- the memory map exposed by MAME for the main CPU;
- 512 KiB of low RAM;
- 8 MiB of main RAM; and
- the 512 KiB boot ROM.

The ROM assembly is generated afterward with `unidasm mips3le`, included in the
same MAME distribution. `trace_postboot.cmd` records ten milliseconds of the
main CPU after waiting ten emulated seconds.

`trace_postboot_100ms.cmd` covers approximately six frames. It confirms which
block recurs as the main coordinator before a name is added to its provenance
record.

`oracle_ki15d_8802d5b0.cmd` loads the verified extracted segment and executes
the original routine at `0x8802d5b0` with controlled inputs. Run it from the
repository root with MAME's dynamic recompiler disabled:

```sh
flatpak run org.mamedev.MAME kinst \
  -rompath assets/ki1 \
  -debug -nodrc \
  -debugscript mame/oracle_ki15d_8802d5b0.cmd \
  -video none -sound none -nothrottle -skip_gameinfo -seconds_to_run 3 \
  -cfg_directory work/mame/cfg -diff_directory work/mame/diff
```

The compact input/output results are tracked in
`tests/fixtures/ki15d_8802d5b0.csv`; the instruction trace remains under
`work/` and is never committed.

`oracle_ki15d_8800700c.cmd` applies the same method to a small RAM-writing leaf
routine. It records the source and destination before and after three controlled
calls. Its compact results live in `tests/fixtures/ki15d_8800700c.csv`; the
native test uses them to build and compare the complete expected RAM image.

`oracle_ki15d_880053f4.cmd` verifies the secondary-object allocator. Its four
cases cover the first free record, a skipped occupied record, the last of 29
searched records, and the original routine's all-occupied fallthrough record.
Run it with the same options as the other controlled oracles, substituting its
script name and allowing four seconds.

`oracle_ki15d_880054d0.cmd` verifies the general object-release clear. Three
cases use different record addresses and fill patterns, with distinct guard
words on both sides. The compact expected results live in
`tests/fixtures/ki15d_880054d0.csv`; the native test compares the complete
mapped RAM image after each call.

`oracle_ki15d_880063ac.cmd` verifies the animation-script setup used by contact
and particle objects. It loads both extracted segments, then applies table
indexes 0, 3, and 12 to controlled records. A fourth synthetic row gives its
last two parameter bytes different values. Index 12 is the entry reached by
the first type-`0x1a` particle in the live Endokuken trace.

`trace_ki15d_type1a_state_writes.cmd` follows every write to one particle record
from allocation through release. It identifies `0x88004180` as the planar
position/decay update and `0x8800842c` as the scaled vertical integration used
by the type-`0x1a` handler at `0x88004e54`.

`oracle_ki15d_88004180.cmd` and `oracle_ki15d_8800842c.cmd` execute those two
original routines against four controlled records each. The cases cover live
particle inputs, signed one-shot movement, early return, decay clamping,
position wrapping, fractional and zero time scales, and 32-bit multiplication
and shift truncation. Their compact fixtures live under `tests/fixtures/`; the
full instruction traces remain ignored under `work/`.

`oracle_ki15d_8800b1fc.cmd` verifies the complete type-`0x1a` constructor while
allowing its original allocator and animation initializer calls to execute.
Four cases cover the exact first live particle, both `$gp` orientation paths,
an occupied first pool slot, countdown-dependent and fixed scale, variant-byte
masking, and position overflow. The native differential test composes the two
independently verified dependencies and compares the complete RAM image.

`oracle_ki15d_88006670_type1a.cmd` isolates the already-installed stream path
through the common animation interpreter. Seven cases cover the original
stream start, ordinary timer decrement, token parsing, half-rate duration
division, low-byte timer carry, opcode `0x14` termination, and opcode `0x10`
with an observable 32-byte ordering-table move. The native implementation is
explicitly scoped to type `0x1a`; no unsupported generic command is inferred.

`native/tests/test_ki15d_type1a_lifetime.c` then composes the independently
verified constructor, handler, motion, animation, scale growth, and release.
It reproduces the live checkpoint after 45 active updates and the exact release
on update 46 without storing copyrighted stream bytes in production code; the
test-only byte sequence is the compact oracle excerpt needed for verification.

`autoplay_jago.lua` and `trace_ki15d_8800700c_live.cmd` form a separate live
experiment. Unlike a controlled oracle, it keeps MAME's dynamic recompiler
enabled, navigates into a Jago-versus-Riptor match, then arms the fighter timer
used by the real type-`0x1f` caller:

```sh
flatpak run org.mamedev.MAME kinst \
  -rompath assets/ki1 \
  -debug \
  -debugscript mame/trace_ki15d_8800700c_live.cmd \
  -autoboot_script mame/autoplay_jago.lua \
  -video none -sound none -nothrottle -skip_gameinfo -seconds_to_run 35 \
  -cfg_directory work/mame/cfg -diff_directory work/mame/diff
```

Do not add `-nodrc` to this live run: booting the full game in the interpreter is
too slow for the 30-second scripted lead-in. The compact observation extracted
from this run is tracked under `provenance/observations/`; the full instruction
trace remains ignored under `work/`.

`autoplay_jago_vs_idle_fulgore.lua` is the controlled gameplay counterpart. It
starts a two-human-player match, leaves Fulgore idle, and has Jago repeat an
Endokuken. It was used to separate the projectile (`0x13` -> `0x12`), central
contact record (`0x14` -> `0x15`), and receiver-side particle series (`0x1a`)
without CPU-player attacks contaminating the trace. The three matching
`trace_ki15d_endokuken_*.cmd` scripts isolate those stages independently and
write their full traces under `work/`.

`trace_ki15d_type1a_frame_lookup.cmd` follows one particle token through the
linked frame tables. It deliberately records the pointer-table value from the
CPU register after the load, because MAME's direct memory view can show stale
backing RAM while a newer value is still resident in the R4600 data cache.

The rendering experiments separate two display paths that initially looked
like one. `trace_ki15d_type15_render_path.cmd` and
`trace_ki15d_type1a_render_path.cmd` prove that class `0x11` is routed around
the ordinary sprite call. `trace_ki15d_impact_pixel_writes.cmd` independently
watches a core and halo pixel in both framebuffer banks. Finally,
`trace_ki15d_endokuken_special_render.cmd` carries each secondary-pass object
pointer into the common renderer and proves that live type-`0x1a` records write
the sampled Endokuken-impact pixels at `0x8801195c`.

`capture_ki15d_endokuken_render_data.cmd` saves full main and low RAM at 27.1
seconds in the same deterministic replay. The ignored snapshots provide packed
frames and both framebuffer banks for offline decoder work without repeatedly
running the game.

`trace_ki15d_type1a_blend_table.cmd` reads the five-bit packet index and live
BGR555 operand from CPU registers for one complete frame. It establishes the
palette base at `0x8803480a` without trusting a potentially stale direct memory
view. After capturing RAM, the native diagnostic exporter can reproduce a
frame over black with:

```sh
make packed-frame-tool
./native/build/packed_frame_export \
  work/mame/dumps/endokuken-mainram-27.1s.bin \
  0x97ea4 work/mame/rendered/type1a-token0b-color.ppm 8 0x3480a
```

`trace_ki15d_type1a_transform.cmd` records all 597 final pixel stores for one
token-`0x0b` call, together with the fixed-point transform state and row
origins. `trace_ki15d_type1a_animation_transforms.cmd` is the compact follow-up:
it records only the first-row transform for each call of record `0x8808c000`.
The tracked observation table contains 27 consecutive samples through token
`0x14`; the full logs remain ignored under `work/mame/traces/`.

`trace_ki15d_type1a_composite_transforms.cmd` expands that experiment to every
type-`0x1a` record participating in one controlled contact. It records the
first selected source row and output-row count as well as placement and both
fixed-point accumulators. Calls targeting the same alternating framebuffer
bank form one displayed tick. The resulting tracked table contains 315 calls
across 96 ticks and retains the original pass order.

`trace_ki15d_type1a_state_to_render.cmd` is the reproducible POC-100 successor.
It starts at the same controlled contact but captures every record/global
input and the original fixed-point checkpoints needed to explain the first
accepted row. Palette source, actual palette lookup, horizontal clip mode and
callback, and packed-source downscale advances come from executing registers,
avoiding cache-sensitive debugger memory assumptions.
Generate an ignored trace with:

```sh
flatpak run org.mamedev.MAME kinst \
  -rompath assets/ki1 \
  -debug \
  -debugscript mame/trace_ki15d_type1a_state_to_render.cmd \
  -autoboot_script mame/autoplay_jago_vs_idle_fulgore.lua \
  -video none -sound none -nothrottle -skip_gameinfo \
  -seconds_to_run 35 \
  -cfg_directory work/mame/cfg \
  -diff_directory work/mame/diff \
  -oslog \
  > work/mame/traces/ki15d-type1a-state-to-render-a.log 2>&1
```

Then validate all calls against the compact tracked fixture:

```sh
python3 tools/analyze_type1a_state_to_render.py \
  work/mame/traces/ki15d-type1a-state-to-render-a.log \
  --fixture tests/fixtures/ki15d_type1a_state_to_render.csv
```

Repeat the MAME command with a `-b.log` output name and compare only lines that
begin with `KI_S2R`; the two accepted POC-100 runs are byte-identical. The
analyzer rejects missing, reordered, malformed, or changed evidence before it
compares the fixture.

`trace_ki15d_type1a_composite_pixels.cmd` captures the six-particle peak using
live registers at `0x8801195c`. Its 5,321 writes provide a cache-safe composite
oracle: applying the recorded blend operands to an initially empty surface
touches 4,101 unique pixels and matches the native result exactly. The large
per-write log remains ignored. `capture_ki15d_type1a_composite_oracle.cmd` is
kept as a diagnostic framebuffer-dump experiment, but direct dump contents are
not treated as authoritative while dirty R4600 cache lines may be present.

The resulting native PC experiment can be built with `make pc-demo`. It reads
the ignored RAM capture directly, so original frame and palette bytes are never
copied into the repository.

Memory dumps, traces, and full generated disassemblies never enter Git. Small
per-function assembly excerpts may be tracked when they are required for
provenance and do not contain game assets.

Although MAME's map displays physical ranges, `save` and `dasm` accept R4600
virtual addresses. These scripts use KSEG0/KSEG1 for RAM and `0xbfc00000` for
the boot vector.
