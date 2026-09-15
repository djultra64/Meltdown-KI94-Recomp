# Differential verification

Progress is a verified executable capability or resolved blocking uncertainty,
not an estimated percentage of the game or a function count. Per-function
records still use the following verification states; those states describe
their declared boundaries and do not imply caller scheduling, runtime
integration, subsystem closure, or whole-game coverage:

| State | Meaning |
|---|---|
| `unverified` | Hypothesis or translation without comparison |
| `partial` | Some effects match; cases or dependencies remain |
| `trace-matched` | Matches recorded fixtures and traces |
| `accepted` | Repeatable match, reviewed with no relevant unknowns |

The curated fact confidence labels in `provenance/knowledge.json` are a separate
vocabulary. They index observations, static findings, differential verification,
implementation, review, and runtime integration without changing any function
record's reconstruction or verification status. In particular, a source
fingerprint is not semantic coverage, and a bounded native match is not proof
that the function is reached by the PC demo or a native game loop.

## Minimum evidence

- Exact game identity and input fingerprints.
- Original virtual address and, when possible, file or segment offset.
- Bytes or assembly saved at a stable path.
- Inferred calling convention: input, output, and preserved registers.
- Memory ranges read and written.
- Hardware dependencies and called functions.
- Input fixtures and expected states.
- Test command and result.

## Proposed comparison

For a specific call, MAME captures:

1. registers before entry;
2. memory regions the function may read;
3. registers at return; and
4. modified regions and relevant access sequences.

The same fixture is passed to the native harness. The test stops at the first
difference and retains a compact diff. A similar final image is not sufficient:
two implementations can look identical while differing in RNG, timing,
overflow, or internal state.

When a routine writes RAM, the native test constructs the full expected memory
image from the MAME fixture and compares every byte in the mapped test region.
This catches accidental writes outside the known destination as well as wrong
values inside it.

The retained projection-A transaction starts from a complete CPU, Status, raw
FPR, and 1 MiB RAM snapshot. Native execution reaches the qualified selected
owner only after 5,272 original instructions and two transform returns. The
comparison checks every RAM byte and every GPR, HI/LO, Status, and raw FPR slot
against the retained final snapshot. It also restores a completed snapshot and
resimulates from the same input, while source mutation, upper FPR lanes,
non-finite/subnormal operands, and a source-overlapping stack are negative
gates. The invalid control-register probe is not used as FCSR evidence.

## Rule for new code

A hardware replacement is verified against MAME's observable behavior but is
marked `hardware-shim`, not recovered game code. This prevents an accidental
reimplementation from being presented as original logic.

Format decoders use structural boundaries as an additional oracle. For the
first Endokuken frame, the recovered row grammar consumes all 22 declared rows
without exceeding the 25-pixel width and stops exactly at the independently
traced pointer of the next frame. Synthetic native tests then cover transparent
gaps, adjacent runs, maximum run/index values, truncation, and row overruns.
The type-`0x1a` blend table was then captured through the live `$t9` register,
avoiding stale data-cache views. The native BGR555 saturating add matches 12
MAME triples containing the destination, blend operand, and stored result.

The next renderer oracle retains every store made by token `0x0b` of record
`0x8808c000`. It captures destination address, prior BGR555 word, palette
operand, and final word. The native packed renderer consumes the same original
frame, starts from the captured inputs, and matches all 597 stores with zero
address or 16-bit-value differences. This verifies the following details as one
combined path rather than as isolated formulas:

- 12-bit fixed-point scaling of both visible runs and transparent gaps;
- the shared horizontal remainder (`0x0d44` for this sample);
- upward row order and vertical row repetition;
- clipping against the 320x240 surface; and
- saturating composition over existing scene pixels.

The complete follow-up trace covers 96 displayed frames, 315 renderer calls,
and seven independently allocated type-`0x1a` particle lifetimes. At its peak,
six particles overlap. The tracked table preserves their original object-pass
order and extends the packed-frame range to tokens `0x04` through `0x15`.

This experiment also exposed the renderer's missing downscale branch. At
`0x88011840`-`0x8801185c`, the R4600 can advance over complete packed rows while
the vertical accumulator remains at or below `0x0fff`. The native renderer now
implements both that rejection path and the previously verified row-repeat
path. A synthetic test selects alternating source rows at half scale.

Debugger-side framebuffer saves are not a reliable final oracle because dirty
R4600 cache lines can differ from the backing memory visible to the debugger.
The composite oracle therefore records live destination addresses and blend
operands at the final store. For one six-particle peak frame, all 5,321 native
writes compose the same blend-only surface across 4,101 unique pixels, with
zero 16-bit differences. This verifies the complete type-`0x1a` composition
without mistaking stale framebuffer backing data for CPU-visible inputs.

The PC demo deliberately labels its 96-frame transform table as capture
scaffolding. Packed decoding, scaling, clipping, blending, and composition now
run natively. A subsequent full-lifetime state-write trace identified the
original planar update at `0x88004180` and vertical update at `0x8800842c`.
Four controlled calls to each routine cover the live particle values, early or
zero-scale cases, signed inputs, wrapping, and truncation. Both native
reconstructions match complete 1 MiB expected RAM images. The object
constructor at `0x8800b1fc` now passes the same full-RAM standard in four more
cases, including the exact first live particle and both orientation branches.
It composes the independently verified allocator and animation initializer.

The type-`0x1a` installed-stream path through common interpreter `0x88006670`
passes seven more full-RAM cases. They cover timer decrement, next-token
selection, signed duration scaling, the low-byte timer carry, setup opcode
`0x10` including a nontrivial ordering-table move, and termination opcode
`0x14`. Provenance remains `partial` because null-script initialization and
commands belonging to other object types have not been reconstructed. A
complete particle-lifetime oracle and scale orchestration follow below; the
state-to-render bridge is mapped by the POC-100 experiment below.

### POC-100 state-to-render verification

`trace_ki15d_type1a_state_to_render.cmd` observes the complete controlled
impact at the original `0x88001b90` bridge and selected checkpoints inside
`0x880107a8`. It records state inputs, frame header, palette-source register,
fixed-point intermediates, clipping state, downscale source-row skips, and the
mode-indexed row callback, first accepted packed row, and first actual palette
load. Large raw logs remain ignored.

The two final executions produced identical `KI_S2R` lines with SHA-256
`da57e25e012b328286cd0254bf0abe6e453c320d16e7b8ac6fa357d2457a4f5f`.
Each contains 315 complete calls across 96 framebuffer-defined ticks. Every
call has exactly one input, renderer, vertical, surface, clip, dispatch,
first-row, palette-lookup, and done phase. The callback evidence separates 261
unclipped calls from 54 calls whose scaled span crosses the right edge; all 315
subsequently reach a real palette load at `0x880118bc`.

The verifier deliberately fails on a missing, duplicated, reordered,
malformed, or arithmetically changed phase. It models R4600 low-32-bit
multiplication, signed shifts, 12-bit scale and remainder rules, framebuffer
bank selection, clipping bounds and mode bit, unclipped/right-clipped callback
selection, packed-source rejection during downscale, real palette addressing,
and destination normalization. The tracked CSV contains only compact numeric
observations and labels the first visible call, first right clip, all six calls
of a peak frame, first horizontal downscale, and all seven last-visible
lifetime calls.

Reproduce the check after creating the ignored trace with the command in
`mame/README.md`:

```sh
python3 tools/analyze_type1a_state_to_render.py \
  work/mame/traces/ki15d-type1a-state-to-render-a.log \
  --fixture tests/fixtures/ki15d_type1a_state_to_render.csv
```

This verifies the observed positive-step type-`0x1a` route both with and
without right-edge clipping. It does not claim dynamic coverage of negative
step, left-edge clipping, other display classes, or nonzero flag combinations.

### POC-110 native state-to-render verification

The POC-100 analyzer can also export the compact native bridge fixture:

```sh
python3 tools/analyze_type1a_state_to_render.py \
  work/mame/traces/ki15d-type1a-state-to-render-a.log \
  --write-bridge-fixture tests/fixtures/ki15d_88001b90_type1a_bridge.csv
python3 tools/analyze_type1a_state_to_render.py \
  work/mame/traces/ki15d-type1a-state-to-render-b.log \
  --bridge-fixture tests/fixtures/ki15d_88001b90_type1a_bridge.csv
```

This second fixture joins the original scalar object/header inputs to the
already verified descriptor. It contains no packed-frame or palette bytes.
`native/tests/test_ki15d_88001b90_type1a.c` places each case in `KiMemory` at
its original address and compares all 315 results returned by the native
`0x88001b90` display-class-`0x60`, flags-`0` path.

The ordinary test runs the two transform sources as a developer comparison:

```sh
make poc110-compare
./native/build/test_ki15d_88001b90_type1a --render-source oracle
./native/build/test_ki15d_88001b90_type1a --render-source state
```

Each selector produces hash `1a8693d815eebeed` from full synthetic 320x240
surfaces after the first, each of six ordered peak calls, and the final call.
The selector and fixture oracle exist only in this test build; neither is
exposed by the PC demo or recovered game path. Boundary cases separately test
signed wrapping arithmetic, scale multiplication, direction, clip sides,
visibility, high-edge vertical rejection and packed-source walking, packed-row
selection, malformed/truncated rows, unmapped reads, exact forward/reverse
callback-table entries, and unsupported original branches. Bytes `0x8e` and
`0x97` are present in every bridge fixture row; all observed values are zero,
and focused cases prove the native bridge rejects either field when nonzero.

The integration harness now composes constructor `0x8800b1fc`, handler
`0x88004e54`, its three per-tick dependencies, and release `0x880054d0` against
the live lifetime trace. It matches the complete recorded state after 45 active
updates and clears every byte of the 0x100-byte record on update 46. This closes
constructor, physics, animation, scale growth, and release as one native
subsystem. The render transform can now be derived from one record natively.
The isolated emitter block `0x88003d30` is also differentially verified at its
declared eligible-invocation boundary. Caller gating, displayed-event timing,
receiver state evolution, and multi-object scheduling remain capture-backed;
the verified block must not be treated as a once-per-frame scheduler.

## Metadata recovery boundary

`tools/knowledge.py` validates the curated fact and uncertainty index and the
tracked evidence links that support it. Missing optional local evidence is
reported as `UNAVAILABLE`; it is never converted into a verification pass.

The local recovery command copies an exact allowlist of UTF-8 documentation,
function metadata, compact observations, selected planning/register text, the
POC-120A review, and the blocked POC-120B/B1 discovery context. It writes a
content hash and byte length for every file into a canonical manifest. The
restore check accepts only that complete allowlist, rejects unsafe paths,
symlinks, unmanifested files, corruption, and overwrites, then compares every
restored byte and hash in a fresh directory.

This is a same-volume metadata exercise under `work/knowledge-backups`, not a
disk-failure backup or correctness oracle. It deliberately excludes ROMs,
CHDs, images, audio, raw traces, memory dumps, binaries, generated assembly,
and the five unverified POC-120B1 implementation drafts. Source-derived raw
evidence must still be regenerated with the commands in this document,
`docs/TYPE1A_EMISSION.md`, and `mame/README.md`; lawful game inputs and the
required tools remain external prerequisites.

## Shared evidence identity and replay

`provenance/verification.json` defines two fixed reviewed anchors rather than a
generic oracle platform: the 65,536 emitter byte-state decisions and the 315
type-`0x1a` bridge calls across 96 visible groups. Run the public check with:

```sh
make verify-check
```

The public check validates the v1.5d/R4600LE identity, segment/load metadata,
function provenance, safe paths, fixed adapter names, and hashes of 17 tracked
analyzer, fixture, capture-script, assembly, native-source, and native-test
dependencies. Because these are two fixed reviewed legacy anchors, it also
pins their complete descriptive semantics under the versioned
`verify-095-v1` contract: base commit, source exit, ABI and exclusions,
initialization, interception, CPU mode, cache-observation boundary, admission,
completion rules, and limits. Type-correct wording changes and unexpected
schema fields fail instead of silently revising the evidence claim. It works
without ignored `work/` evidence. Its `PASS` means only
that public metadata and dependencies are consistent; private captures are
reported `AVAILABLE` or `UNAVAILABLE`, and no original replay pass is inferred.

With the reviewed retained captures and extracted segment available locally,
replay both anchors without launching MAME:

```sh
python3 tools/verify_evidence.py replay --anchor all
```

Replay verifies the source segment and each capture's declared size/SHA-256,
checks both original function byte ranges against function provenance, then
dispatches only the two compiled-in adapters. It never executes acquisition
text or commands from JSON and never updates expected hashes or fixtures. A
missing private input returns `NOT_RUN` with nonzero status; identity drift,
malformed/truncated completion, or a first output difference returns `FAIL`.

Both retained logs are admitted as **reviewed legacy evidence**. They predate
the manifest and do not self-attest their revision, harness/source hashes, or
every acquisition field, so replay is not fresh original execution. The
emitter log uses MAME 0.289 with `-nodrc` and intercepts the constructor only
after the original call delay slot. Its digest comparison excludes general
temporary-register and full-memory equivalence. The separately rebuilt native
tests retain their declared bounded-memory and constructor-composition checks;
they also make no general temporary-register-equivalence claim.

The bridge acquisition uses MAME 0.289's default DRC setting (`-debug`, without
`-nodrc`). Its observation method is mixed: the frame pointer, palette source
and table, framebuffer-selector register, callbacks, and arithmetic
intermediates come from executing registers, while some record/header fields
are debugger reads. This is not a blanket cache-coherence claim. The bridge
still starts with supplied projected fields and a valid frame pointer, remains
`in-progress`/`partial` in function provenance, and excludes caller scheduling,
palette-copy side effects, alternate display classes, nonzero flag routes, and
general ABI or gameplay closure.

Evidence replay also does not prove that native binaries are current. The
manifest pins the selected native entry source and test files as an index, not
their full transitive build closure; `make -B check` resolves dependencies and
must rebuild and execute the native suites as a separate gate. Each CLI result
prints the actual manifest and runner sizes/SHA-256 identities used.

## Contact initializer

The KI v1.5d block `0x8800a2e4..0x8800a37b` is represented by
`ki15d_8800a2e4`. Two separately generated MAME 0.289 `-nodrc` runs execute 18
controlled cases and stop only at `0x8800a37c`. Their scalar CSVs are identical
(SHA-256
`091205165a3da3e001f2223083ca4d7e579e573bbe6abc115feab80a5c3d9e30`).
The harness records the CPU operands for the initial effect load, selected row
bytes, all four stores and the descriptor reread, and compares them with exact
before/after snapshots. It rejects unknown log phases, wrong terminal PC,
stale identities, table changes and writes outside receiver bytes c4..c7.

`tests/fixtures/ki15d_8800a2e4.csv` contains only the reviewed scalar reduction,
not raw game data. The native test compares all mapped RAM for every fixture
case, preserves partial writes across late mapping errors, exercises lookup and
record aliasing, and composes the initialized countdown/cadence with the emitter
to obtain seven constructor-boundary calls over 49 eligible invocations.

This remains a bounded internal memory ABI. The original block continues into
unrecovered hit processing; temporary registers, descriptor ownership,
collision/damage selection, pause/update scheduling, projection, and gameplay
are outside this verification. `-nodrc` is not itself cache proof: only the
explicit CPU/debugger/snapshot agreements in this controlled harness are
claimed.
