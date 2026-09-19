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

## Current status — September 19, 2026

**This is an early native-execution research build, not a playable game.**
The Linux diagnostic demo renders the green Endokuken impact, but still uses
captured scheduling and transforms. It is not proof of a fully integrated
native effect, a complete startup, or a working fight.

### Verified capabilities

- The working game revision is **KI v1.5d**. Source identities and bounded
  reconstruction evidence are recorded in the repository's provenance files.
- A build-time MIPS translator and native runtime execute selected connected
  original regions, including calls, branches, delay slots and a verified
  manual-code boundary. Unsupported paths stop explicitly.
- Bounded object allocation, release, particle construction, animation,
  movement and emission routines have original/native comparisons.
- Selected contact, projection, frame-selection and rendering transactions
  have bounded state comparisons. The connected `0x8800099c` to `0x880012b8`
  diagnostic phase executes natively; its initialization remains diagnostic.
- Scoped snapshots support restoration and deterministic replay for admitted
  pilot transactions. This is not whole-game rollback or online play.
- The registered startup-clear region `[0x880001c4, 0x880001ec)` executes
  natively and matches a retained original endpoint comparison. A separate
  nonzero-memory test checks its write effects. Its exit is an explicit stop;
  full startup and startup-region snapshots remain unsupported.
- The host presenter preserves the canonical 320x240, 4:3 game surface, with
  integer/aspect fit, resizing, fullscreen and external solid-color decoration.
  Automated viewport checks cover 1080p and 4K geometry.

### Remaining boundaries

- The effect still needs authentic initiating state and connected lifecycle
  execution without captured spawn or transform schedules. Existing diagnostic
  fighter streams and seeded state are not production initialization.
- Full CP0/TLB/FCSR behavior, production timing and reached device contracts
  remain incomplete. Unknown operations cannot be replaced by silent no-ops.
- Complete scenes, playable combat, executable integrated DCS audio and the
  full arcade flow are not implemented.
- Linux is the current development and test host. Windows is a target, but
  actual Windows execution has not yet been verified.
- Image-based decorative artwork is not integrated into the executable.

The next integration objective is a repeatable, state-driven native effect
with scoped restore/replay. Progress is assessed by verified capabilities,
not function counts or an implied completion date.

See [technical boundaries](docs/KNOWLEDGE.md),
[object-model findings](docs/OBJECT_MODEL.md), and the
[verification method](docs/VERIFICATION.md) for the evidence scope.

## Game data

No ROMs, CHDs, extracted graphics, audio, memory dumps, or other copyrighted
game data are included. Users must provide their own lawfully obtained dumps.
All local inputs and substantial derived artifacts remain ignored by Git.

## Quick start

From the repository root:

```sh
python3 tools/ki_project.py doctor
python3 tools/ki_project.py inventory /path/to/your/KI-files \
  --output work/input-inventory.json
```

`inventory` reads files, calculates fingerprints, and compares ZIP/CHD members
with the public MAME manifest. It never modifies the supplied dumps.

`make pc-demo` builds the diagnostic executable. Running it requires the local
capture described below. `make check` is the development verification suite:
several checks require locally extracted source segments and retained private
oracle captures that are not distributed with this repository. A clean public
checkout alone cannot reproduce the complete suite; missing evidence is not a
passing result. See [verification prerequisites](docs/VERIFICATION.md#running-checks).

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
