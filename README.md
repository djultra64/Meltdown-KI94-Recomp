# Meltdown-KI94-Recomp

Meltdown is a research and learning project exploring native recompilation of
the 1994 **Killer Instinct** arcade game. The project targets native PC
execution on Linux and Windows.

This is not a matching decompilation. The project does not aim to recover the
original source verbatim, reproduce the original compiler, or generate a
byte-identical ROM. Its purpose is to recover as much authentic MIPS R4600 game
logic as practical and run it natively through verified decompilation, static
recompilation, or a combination of both. New code is limited to tooling, tests,
and explicit replacements for arcade hardware and host-platform services.

## Current status

- Working revision confirmed as **KI v1.5d**. The main boot ROM and all eight
  audio ROMs match the MAME manifest, and `chdman` fully verified the CHD.
- MAME 0.289 and Ghidra 12.1.3 are installed and scripted on Linux.
- Boot ROM, RAM snapshots, and an initial 100 ms execution trace were captured.
- The U98 compressed payload was extracted into three segments and compared
  with MAME RAM. The segment containing the main loop matches byte-for-byte.
- The provisional main controller at `0x8802aa24` and recurring loop head at
  `0x8802ae14` have provenance records and seeded Ghidra labels.
- A minimal R4600 memory model runs in the native PC test harness.
- The original routine at `0x8802d5b0` now executes natively on PC and matches
  seven controlled 64-bit input/output cases produced by its R4600 code in MAME.
- The original record-copy routine at `0x8800700c` now runs through that memory
  model. Three controlled MAME cases match, including a byte-for-byte comparison
  of the full 1 MiB native test RAM after each call.
- Its three known callers now establish that it initializes secondary objects
  from fighter records. A live match reached the type-`0x1f` caller organically.
- The preceding allocator at `0x880053f4` also runs natively. Four controlled
  MAME cases match, including its unusual all-occupied fallthrough behavior.
- A controlled Jago-versus-idle-Fulgore match now traces the Endokuken from its
  type-`0x13` initializer to active type `0x12`, then separates the type
  `0x14`/`0x15` central contact object from the timed type-`0x1a` particle burst.
  The project owner's visual review confirms that the complete green effect is
  the Endokuken impact.
- The general object-release routine at `0x880054d0` now runs natively and
  matches three controlled MAME cases with full-RAM comparisons.
- The animation initializer at `0x880063ac` also runs natively. Its verified
  index-12 path links the type-`0x1a` particle to original command stream
  `0x8805e6f2`.
- Two original per-frame motion routines now run natively. `0x88004180`
  updates the particle's planar position and decaying movement amount;
  `0x8800842c` integrates its signed acceleration, velocity, and vertical
  position. Eight controlled MAME cases match complete native RAM images,
  including R4600 overflow and truncation behavior.
- The complete type-`0x1a` constructor at `0x8800b1fc` now runs natively by
  composing the verified allocator and animation initializer. Four full-RAM
  MAME cases cover the exact first live Endokuken particle, both orientation
  branches, pool selection, scale boundaries, masking, and position wraparound.
- The installed-stream path through common interpreter `0x88006670` now
  advances type-`0x1a` animation natively from token `0x04` through `0x15` and
  handles its original `0x10` setup and `0x14` termination commands. Seven
  controlled full-RAM cases match; unrelated generic interpreter paths remain
  explicitly unsupported rather than guessed.
- Handler `0x88004e54` now composes those recovered routines in original order.
  Starting from the real first-particle constructor state, the native lifetime
  matches the MAME checkpoint after 45 active ticks and clears all 256 record
  bytes on release tick 46.
- Pixel-level traces now prove that live type-`0x1a` records write both sampled
  core and halo pixels of the confirmed Endokuken impact through the secondary
  render pass and common software renderer.
- A native packed-frame decoder now expands the original five-bit run data. It
  decodes the real frame at `0x88097ea4` to its exact 25x22 index image and has
  bounds/error-path tests independent of copyrighted game data.
- The type-`0x1a` 32-entry green blend table is recovered, and the original
  branch-free BGR555 saturating add matches 12 MAME register cases. Eighteen
  authentic packed frames are mapped from token `0x04` through token `0x15`.
- A new native packed renderer reproduces the R4600 renderer's 12-bit
  horizontal and vertical accumulators, orientation, clipping, placement, and
  BGR555 scene composition. For token `0x0b`, all 597 framebuffer stores match
  the controlled MAME oracle in address and final 16-bit value. The vertical
  path also implements the source-row rejection used for particles scaled
  below their original size.
- The complete controlled impact contains seven type-`0x1a` particle
  lifetimes: 315 renderer calls across 96 displayed frames, with six particles
  overlapping at the peak. The native renderer preserves their original pass
  order. A same-run peak oracle covers 5,321 R4600 writes to 4,101 unique
  destination pixels and produces zero native blend-surface differences.
- The verified display-class-`0x60`, flags-`0` path through original routine
  `0x88001b90` now derives a complete packed-render descriptor directly from
  guest object state. All 315 controlled calls match, and developer-only
  oracle/state rendering gives the same complete framebuffer hash for the
  first call, all six peak calls in order, and the last call. Unsupported
  display/flag/palette branches fail explicitly rather than being guessed.
- The PC window now animates that complete captured impact over either a
  supplied 320x240 scene or a generated diagnostic background. SDL2 is a
  replaceable runtime-loaded host layer; it does not determine game rendering
  behavior.
- Next milestone: recover the original particle-emission timing and ordering so
  the full seven-particle effect no longer needs a captured schedule.

## Game data

No ROMs, CHDs, extracted graphics, audio, memory dumps, or other copyrighted
game data are included. Users must provide their own lawfully obtained dumps.
All local inputs and substantial derived artifacts remain ignored by Git.

## Quick start

From the repository root:

```sh
make check
python3 tools/ki_project.py doctor
python3 tools/ki_project.py inventory /path/to/your/KI-files \
  --output work/input-inventory.json
```

`inventory` reads files, calculates fingerprints, and compares ZIP/CHD members
with the public MAME manifest. It never modifies the supplied dumps.

### Native PC rendering milestone

After producing the local RAM capture described in `mame/README.md`, build and
open the native window with:

```sh
make pc-demo
./native/build/endokuken_demo
```

The default run uses a generated diagnostic background, animates all 96 frames
of the captured impact, and expects the ignored RAM capture at
`work/mame/dumps/endokuken-mainram-27.1s.bin`. To compose over a lawfully
captured 320x240 scene, convert it to an 8-bit binary PPM and pass it explicitly:

```sh
magick your-clean-scene.png -depth 8 ppm:work/clean-scene.ppm
./native/build/endokuken_demo --background work/clean-scene.ppm
```

The capture-backed diagnostic window keeps the 320x240 (4:3) source surface
unchanged while fitting it to the actual high-DPI drawable. Integer fit is the
default; `--fit aspect` permits fractional downscaling, `--scale` selects the
initial window size, `--fullscreen` starts in desktop fullscreen, F11 toggles
fullscreen, and `--decor RRGGBB` colors only the area outside gameplay.
`--output`, `--raw-output`, `--background-raw`, and `--tick` provide
deterministic headless output for image review and exact framebuffer
comparisons.

To create a provenance record for an identified function:

```sh
python3 tools/ki_project.py new-function \
  --id ki15d_80001234 \
  --address 0x80001234 \
  --name provisional_name
```

## Documentation

1. [Initial hardware map](docs/HARDWARE_BASELINE.md)
2. [Verification method](docs/VERIFICATION.md)
3. [First object-model findings](docs/OBJECT_MODEL.md)
4. [Ghidra project](docs/GHIDRA.md)
5. [External references](docs/EXTERNAL_REFERENCES.md)

## Repository layout

```text
config/                 Public fingerprints and revision configuration
docs/                   Technical notes and external references
ghidra_scripts/         Reproducible labels and analysis setup
mame/                   Debugger capture and tracing scripts
native/                 Native PC runtime and tests
provenance/             One JSON record per recovered original function
tools/                  Inventory, comparison, and validation utilities
work/                   Local captures and analysis; always ignored by Git
```

## Readability and auditability

Native reconstructions favor clear, reviewable code over clever shortcuts.
Each recovered routine lives in an address-named file, non-obvious MIPS
semantics are explained next to the implementation, and tests point to the
same fixtures recorded in provenance. Comments should explain intent,
assumptions, hardware behavior, and fidelity-sensitive details rather than
repeat what an individual statement already says.

## Disclaimer

Meltdown is an unofficial, non-commercial research project. It is not affiliated
with or endorsed by Rare, Microsoft, Nintendo, Midway, or MAME. Killer Instinct
and all related game assets remain the property of their respective owners.
