# First object-model findings

This note separates facts demonstrated by code or traces from semantic names
that still require gameplay analysis. Addresses refer to Killer Instinct v1.5d.

## Connected native execution pilot

The bounded connected runner executes the original type-`0x1a` record path and
the whole-dispatch transaction through its real large/small pools, the selected
resumed fighter updates, pause epilogue, and isolated packed-frame selector
from a build-time generated set of 3,177 original instruction words (12,708
bytes). It owns the full
guest GPR/HI/LO/PC context and models branch delay slots; only the verified
`0x880054d0` record clear remains a manual full-register adapter. Unknown table
targets, null initial scripts, changed enrolled code, bad mappings and budget
exhaustion stop explicitly. This is a computational harness, not a general
R4600 interpreter or a whole-machine scheduler.

Checkpoints contain the complete 1 MiB pilot RAM, the optional authoritative
512 KiB low-RAM bank, and full guest context in a
fixed little-endian format. Restore validates size, format, build/load identity,
boundary and payload integrity before changing live state, and host memory may
be rebound at a different address. The bounded contact tape and pending byte
MMIO journal are resumable computational state; external device delivery,
interrupts, production input/clock ownership, disk and host display state are
not claimed resumable.
The identity bytes are generated as the SHA-256 of the exact tracked
`provenance/native_pilot_manifest.json` bytes; that manifest records both load
segments, the 3,177 generated words and the separately verified 28-byte manual
adapter. The allocation animation selector, its eight-byte installer-table
entry and the executed command-10/duration stream prefix are also
identity-pinned source data. A caller starts each post-restore invocation explicitly with its next
`fp` and `gp`; the pilot does not infer scheduler state.
The dispatcher harness admits only its declared stack top `0x88087300` so its
two nested save frames cannot alias enrolled code or other unqualified state.
The resumed enrollment includes the selected real allocation, constructor and
animation-install path, including occupied-slot fallback and the new object's
update later in the same dispatch. Collision resolution and unselected
motion/input branches remain explicit frontiers. This does not close B2,
initial contact selection, or recover the upstream scalar producers.

The frame transaction authenticates the exact 136,192-byte extracted boot
graphics file before copying it to `0x88090100`. Those bytes become ordinary
mutable guest RAM: snapshots preserve later pointer-table changes and restore
does not reinstall the file. The enrolled 52-word selector stops at the real
caller continuation `0x88001898`, after its delay-slot store. Seven original
executions cover normalization, final/clamped tokens, two real rows, a later
frame, and a synthetic sentinel fallback. This slice does not recover
projection, wider table producers, storage hardware, or complete POC-120C.

The selected contact transaction adds the retained original `0x88008cdc` to
`0x88008fbc` path and its reached helpers. Three Count reads and four byte MMIO
reads consume a caller-supplied, PC/address-tagged tape; twelve original byte
stores append to a bounded rollback-owned journal. Count is not derived from
host time, and the journal is not delivered to audio hardware. Snapshots v2
include the tape/cursor, pending journal, raw 32 FPR lanes, and observed Status
register. The admitted floating-point operations are restricted to exact
signed-integer conversion, exact positive-normal binary32 square root, and
exact integral conversion. FCSR, DCS/audio execution, interrupts, production
clock ownership, and other contact paths remain unsupported.

The bounded renderer transaction executes the retained 316-word original
wrapper, palette copy, geometry setup, and forward packed-pixel family through
its real `0x88001ae4` continuation. Snapshot v3 owns both the 1 MiB main RAM
and 512 KiB low RAM framebuffer; presentation only reads committed guest
pixels. The qualified route is the observed mode `0x70`, nonnegative-facing,
forward, unclipped family. Reverse/clipped/unit-scale/alternate-table routes,
frame traversal and full render-loop scheduling remain explicit frontiers.

The projection transaction enrolls the 382 original words reached from
`0x88001038` through the selected-owner boundary at `0x88001228`. It executes
both original transform calls and their polynomial helpers rather than
substituting captured coordinates. Each single-precision operation is rounded
separately to binary32, nearest-even; the host floating-point environment is
held and restored around the operation. The admitted numerical domain contains
finite normal values and both signed zeros. Subnormals, infinities, NaNs,
FCSR-dependent behavior, exception-flag fidelity, other owners, and the full
projection loop remain explicit frontiers. Raw integer payloads in FPR slots
are classified only when an instruction consumes them as floating point.

The connected diagnostic transaction runs from `0x8800099c` through the real
contact-wrapper return at `0x880012b8` without resetting CPU, FPR, RAM, or
service state between original calls. A retained MAME 0.289 DRC run compares
all endpoint registers and both RAM banks. Its mode-6 camera, fighter, and
animation inputs are declared test inputs rather than a production startup
claim. At `0x8800914c`, only the single observed zero-divisor tuple is admitted:
the DRC preserves HI and writes LO to zero. The interpreter behaved differently,
so other operands, other sites, hardware behavior, positive contact, effect
view/order, and scheduler ownership remain explicit frontiers.

## Record layout and allocation

Two fixed 0x100-byte fighter records begin at `0x8808bc00` and `0x8808bd00`.
The routine at `0x880053f4` then searches secondary records beginning at
`0x8808be00`:

1. byte `0x00` is tested as an occupied/type marker;
2. occupied records are skipped in 0x100-byte steps;
3. at most 29 records (`0x8808be00` through `0x8808da00`) are tested;
4. the selected record is cleared, one 32-bit word at a time from offset
   `0xfc` down to `0x00`; and
5. its address is returned in `$a2`.

There is no failure branch. If all 29 tested records are occupied, the original
code advances to `0x8808db00`, clears it, and returns it. A controlled MAME
oracle confirms this behavior. Until surrounding memory is mapped, this note
calls `0x8808db00` the *fallthrough record* rather than assuming it is either a
bug or an intentionally reserved slot.

## Common object initialization

All three currently known callers of `0x8800700c` first call the allocator. The
leaf routine then writes the requested type to byte `0x00` and copies the three
32-bit words at offsets `0x04`, `0x08`, and `0x0c` from a source record. These
words behave like placement/origin state, but that field name remains a
hypothesis until their consumers are traced.

| Call | Assigned type | Source | Confirmed context |
|---|---:|---|---|
| `0x88002bb4` | `0x1f` | current fighter (`$fp`) | byte `0xf9` counts down; a record is created on every even value |
| `0x88007cfc` | `0x1e` | fighter selected by command data | command-interpreter opcode `0x48` |
| `0x88008010` | `0x20` | opposing fighter | command-interpreter opcode `0x51` |

The command dispatch table begins at `0x88033900`. Its entries at indexes
`0x48` and `0x51` point to handlers `0x88007cdc` and `0x88007ff8`, respectively.
This establishes those two contexts without assigning names to the commands'
visible effects yet.

## Live type-0x1f observation

The tracked live script reaches the `0x88002bb4` caller during an automated
Jago-versus-Riptor match. It changes only byte `0xf9` in each fighter record to
make the caller deterministic; execution otherwise reaches the routine through
the original game loop.

The first captured call had:

- source/current fighter: `0x8808bc00` (Jago, type/owner byte `0x01`);
- destination: first free secondary record `0x8808be00`;
- assigned secondary type: `0x1f`; and
- copied words: `0x00004000`, `0x00010c00`, `0x00000000`.

After the caller completed, the object also received owner byte `0x01`, copied
fighter state at offsets `0x14`, `0x24`, `0x34`, and `0x8c`, and a value of
`0x14` at offset `0x38`. Across eight later samples, the object kept the copied
three words and the Jago owner byte.

The common object update routine at `0x88006670` indexes a data table at
`0x88033a5c` using byte `0x00` of the record. Entry `0x1f` contains
`0x8807a646`. That address points into packed data rather than aligned MIPS
code, so it is currently treated as a type descriptor or command stream, not a
function entry point. Decoding that format is still pending.

This makes a temporary fighter copy, echo, or afterimage the leading semantic
hypothesis for type `0x1f`. It does **not** yet justify a final name. A manual
visual review by the project owner identified a green contact effect over
Riptor after an Endokuken hit in the same captures. Because the forced type
`0x1f` object remained tied to Jago's record and origin, that contact effect is
recorded as distinct from type `0x1f`.

## Controlled Endokuken lifecycle

A second automation starts a two-human-player Jago-versus-Fulgore match and
leaves Fulgore idle. This eliminates AI attacks that made the earlier
Jago-versus-Riptor run ambiguous. Repeated Endokuken inputs then produce the
following object sequence:

| Stage | Original code | Record type | Observed behavior |
|---|---|---:|---|
| Script command | opcode table `0x88033900`, index `0x15` -> `0x88006d90` | `0x13` | allocates and initializes the projectile record |
| First update | `0x880045bc` | `0x12` | changes the same record to its active state |
| Contact initializer | `0x8800a52c` | `0x14` | allocates a second record and copies the projectile contact position |
| Contact activation | `0x88004408` | `0x15` | changes that second record in place on its next update; its direct visible contribution remains unisolated |
| Receiver hit state | `0x8800a2e4`, then emitter block `0x88003d30` | `0x1a` | emits a timed series of independently moving secondary records |
| Projectile release | `0x880054d0` | `0x00` | clears all 0x100 bytes of the former type-`0x12` record |

The receiver-side damage path uses a descriptor byte to initialize fighter
fields `0xc4` through `0xc7`. Field `0xc4` is an emission countdown and field
`0xc5` controls its cadence. The controlled Fulgore hit stored an initial
countdown of `0x31`. The emitter decrements it before calling the constructor,
so the first constructor observed `0x30`. Calls to `0x8800b1fc` progressively
created type-`0x1a` records with different positions, velocities, and script
pointers; each traced record had 45 visible updates followed by terminating
update 46.

The project owner confirmed that the green visual in the controlled Fulgore
capture is the Endokuken impact, matching the effect previously identified over
Riptor. The type-`0x14`/`0x15` record is synchronized with contact, but timing
alone does not prove which of its data becomes visible. Pixel watchpoints do
prove that the type-`0x1a` series draws both the sampled bright core and green
halo. It is therefore part of the visible impact rather than only invisible
collision or timing state.

The type descriptor table at `0x88033a5c` provides another route into the
rendering data. Its current entries are `0x88065290` for type `0x12`,
`0x8806b230` for type `0x13`, `0x880764e0` for type `0x14`, `0x88070a00` for
type `0x15`, and `0x8805693c` for type `0x1a`. Their packed format is not yet
decoded.

## Animation-script setup

The type descriptors feed the common interpreter at `0x88006670`. When an
object has no current script pointer, that routine selects its descriptor from
the table at `0x88033a5c`. Once a stream is selected, it reads two-byte items:

- a nonzero first byte becomes the current frame/token at object offset `0x14`;
  the second byte supplies its duration, scaled through the object's value at
  offset `0x42`; or
- a zero first byte makes the second byte an opcode dispatched through the
  command table at `0x88033900`.

The smaller routine at `0x880063ac` provides a second, explicit way to install
an animation stream. It indexes an 8-byte table beginning at `0x8805e310` and
copies the entry's script pointer and parameters into object offsets `0x20`,
`0x1c`, `0x34`, `0x2c`, and `0x8a`, while clearing the counters at `0x14` and
`0x18`. The first traced type-`0x1a` particle used table index 12, which installs
script pointer `0x8805e6f2`.

Four controlled MAME cases now match a native reconstruction of `0x880063ac`
over the complete 1 MiB test RAM image. Three use original table indexes 0, 3,
and 12; a fourth synthetic row proves that entry bytes 6 and 7 remain
independent. This is the first recovered bridge between an Endokuken effect
object and its original packed animation-command stream.

Nominal-frame samples of the first particle show the stream advancing through
tokens `0x04` to `0x12` with the two- or three-tick durations encoded beside
them. The frame-selection routine at `0x880016c4` then uses the token and the
object's state byte at offset `0x34` to follow a linked set of frame tables. A
controlled live trace reached state row `0x88097614`; for token `0x0b`, that row
selected frame-data pointer `0x88097ea4` and limit pointer `0x88097fe7`.

The earlier claim of a cache-coherency mismatch at pointer slot `0x88090164`
was an instrumentation error. The retained script calculated that label from
the record byte, but original `0x880016f0` assigns class index 11 in its delay
slot and the executing load uses `0x88090140`. CPU and debugger backing values
agree at that actual address. The old traces remain useful historical evidence,
but they do not prove a same-address cache discrepancy.

The already-installed-script path through `0x88006670` is now reconstructed
for type `0x1a`. Each update subtracts `0x0100` from a nonexpired timer. When a
new pair is needed, the duration byte is shifted left 16 and divided by the
signed halfword at record offset `0x42`; only the old timer's low byte is added
before the routine subtracts the current tick. Seven complete-RAM oracle cases
also cover the stream's initial opcode `0x10`, its ordering-table helper, and
terminating opcode `0x14`, which clears the script pointer.

This is deliberately not labeled a complete reconstruction of the common
interpreter. Its null-script descriptor initialization and command opcodes used
by other object types remain unsupported until independently traced.

## Type-0x1a constructor

The receiver-side emission block calls `0x8800b1fc`. The constructor saves its
return address, calls the verified secondary-record allocator, assigns type
`0x1a`, and derives the new record from fighter state:

- position words `0x04` and `0x08` are copied directly from the fighter;
- fighter byte `0xc6`, shifted left eight, is added to position `0x0c`;
- direction words `0x74` and `0x78` are retained or negated according to the
  zero/nonzero `$gp` orientation branch, then stored as halfwords `0x7c` and
  `0x80`;
- emission countdown byte `0xc4` produces persistent movement `c4 << 3`,
  one-shot movement `(c4 << 6) - 0x0c00`, and an initial scale of
  `(c4 << 6) + 0x0800` while `c4 < 0x1a`, otherwise `0x1000`; and
- fighter byte `0xc7 & 0x1f` becomes record byte `0x8e`.

The constructor also installs velocity `0x00a0`, signed acceleration `-10`,
state byte `0x02`, display byte `0x60`, and shared flag word `0x01000100`.
Original data byte `0x8800b2d8` selects animation index `0x0c`; the shared tail
then calls verified initializer `0x880063ac`, which installs stream
`0x8805e6f2`.

Four complete controlled calls now match the native constructor across the
entire 1 MiB test RAM. They include the exact first live Fulgore-side particle,
both orientation branches, a skipped occupied pool record, low-countdown scale
calculation, the `0x1a` fixed-scale boundary, byte masking, and position
wraparound.

## Type-0x1a per-frame motion

The handler beginning at `0x88004e54` gives the particle update a concrete
order. It calls planar motion routine `0x88004180`, scaled vertical-motion
routine `0x8800842c`, and the common animation interpreter at `0x88006670`.
It then adds `0x0080` to the vertical scale halfword at offset `0x5a`. A zero
script pointer at offset `0x20` sends the object toward its release path.

The planar routine chooses a signed one-shot movement amount at offset `0x7e`,
or an unsigned persistent amount at `0x84` when the first value is zero. It
multiplies that amount by signed direction halfwords at `0x7c` and `0x80`,
shifts each low 32-bit product right by eight, and adds the resulting deltas to
position words `0x04` and `0x08`. The one-shot amount is cleared, while the
persistent amount is reduced by offset `0x86` and clamped at zero. It also
publishes the two latest deltas at `0x88087b20` and `0x88087b24`.

The vertical routine treats offsets `0x3c` and `0x40` as signed acceleration
and 8-bit fixed-point time scale. It updates velocity `0x10`, then position
`0x0c`. Its 32-bit left shifts deliberately discard high bits before the next
arithmetic shift; this is observable with extreme inputs and is preserved in
the native reconstruction rather than replaced with ordinary host arithmetic.

Four controlled MAME cases per routine now match complete 1 MiB native RAM
images. For the first live particle, one handler tick changes planar position
`0xffff9a00` to `0xffff9880`, vertical velocity `0x000000a0` to `0x000000aa`,
vertical position `0x00005500` to `0x000055aa`, animation token `0x00` to
`0x04`, and vertical scale `0x1000` to `0x1080`. All five changes are now
generated by recovered native game logic in the integration harness; only the
subsequent conversion from record state to renderer transforms still comes
from the captured timeline in the PC demo.

Handler `0x88004e54` now composes these routines in their original order and
routes a zero post-animation stream through release routine `0x880054d0`. From
the exact live constructor inputs, the native particle remains active for 45
updates. Its checkpoint then matches MAME at position
`(0xffff5680, 0x00000000, 0x0000998e)`, velocity `0x00000262`, token `0x15`,
script `0x8805e71a`, and scales `0x1000/0x2680`. Update 46 executes opcode
`0x14`, grows vertical scale once more, and clears the complete record exactly
as the original release branch does.

## Static dispatcher boundary and receiver gate candidate

Static inspection of the update dispatcher at `0x88002038..0x880021d8` finds a
table at `0x8803416c`, indexed by `type * 4`. Its type-`0x1a` entry targets
handler `0x88004e54`, and the loop advances records by `0x100`. This explains a
candidate path for ascending secondary-record updates, but the complete
dispatcher, fighter follow-up, caller scheduling, and other type handlers are
not reconstructed or dynamically verified.

Within the common fighter updater, global byte `0x88087aec` is read at
`0x88002a70`; the branch at `0x88002a74` can skip the later fighter path.
This is a **receiver gate candidate**, not proof that it causes the three
omitted emitter invocations. Its value, writer, and branch behavior must be
captured together with receiver dispatch and framebuffer-defined grouping.

## Unverified contact lookup

Static inspection of the draft contact block `0x8800a2e4..0x8800a378` finds an
eight-byte row at `0x88034610 + (descriptor[0x1d] & 0x0f) * 8` and a variant
mapping based at `0x88034647`; the addressed ranges can overlap. No controlled
original oracle has executed for this block. Its branches, wrap behavior,
aliasing, read order, and memory effects remain unverified, and the five local
POC-120B1 files are not accepted implementation evidence.

## Endokuken rendering path

The ordinary object pass selects frames for both active type `0x15` and type
`0x1a`, then maps their types through the byte table at `0x880341f0`. Both map
to class `0x11`. The branch at `0x880018b8` routes that class around the
ordinary renderer call; this is routing, not proof that the objects are
invisible.

A later pass iterates the active records again and calls `0x88001b90` for these
objects. During the controlled Endokuken impact, this path supplied successive
type-`0x1a` packed frames to the common software renderer at `0x880107a8`.
Tokens `0x0d` through `0x12` selected frame pointers `0x880981a0`,
`0x88098394`, `0x880985b8`, `0x88098808`, `0x88098ab4`, and `0x88098da0`.

Two pixels were watched in both alternating framebuffer banks: a bright-core
sample at `(260, 100)` and a green-halo sample at `(255, 90)`. At the final
16-bit store at `0x8801195c`, a debugger temporary retained the record pointer
that had entered `0x88001b90`. Every sampled impact write belonged to a live
type-`0x1a` record in state `0x0a`; examples include records `0x8808c000` and
`0x8808c300`. The observed path is therefore:

`type-0x1a record -> 0x88001b90 -> 0x880107a8 -> 0x8801195c -> BGR555 framebuffer`

The data beginning at `0x88097ea4` is not a raw bitmap. Its first eight bytes
are four little-endian 16-bit values: signed origin `(12, 8)` and dimensions
`25 x 22`. Each following row starts with its total byte size. Size one means
a completely transparent row; every nonempty row then has an initial
transparent skip. In a nonzero packet byte, bits 7..3 are a five-bit color
index and bits 2..0 are the run length minus one. A zero packet introduces
another transparent skip.

The native decoder in `native/src/packed_frame.c` expands this representation
to an unscaled row-major index image. For frame `0x88097ea4`, it consumes 0x148
bytes across exactly 22 rows and finishes at `0x88097fec`, the independently
observed pointer of token `0x0c`. Every decoded run remains inside the declared
25-pixel width. The same decoder now accepts all nine traced frames from token
`0x0b` through `0x13`.

For this type-`0x1a` path, the live renderer held palette base `0x8803480a` in
`$t6`. Register traces independently matched the table's entries for indexes 2
through 27. The renderer combines each selected value with the existing BGR555
destination using the branch-free saturating add at
`0x8801193c`-`0x88011958`. Its native reconstruction matches 12 captured
input/blend/output triples, including component-overflow cases.

The exporter can therefore render the original frames with their original
green palette over black.

The renderer transform is now recovered for one complete token-`0x0b` call.
Its first destination row begins at screen coordinate `(266, 123)` before the
packed row's initial skip. The horizontal scale is `0x104d` with 12 fractional
bits and remainder `0x0d44`; rows proceed upward with scale `0x17f1` and initial
accumulator `0x25e8`. The original loop applies the same horizontal accumulator
to transparent gaps and colored runs, and it repeats a source row while the
vertical accumulator remains above `0x0fff`. Generic host bitmap scaling does
not preserve those packet-boundary decisions.

The native implementation in `native/src/packed_renderer.c` follows those
rules directly. Starting from the 597 destination words captured immediately
before the R4600 stores, it produces the same 597 addresses and final 16-bit
words. A complete follow-up captures tokens `0x04` through `0x15`, including
the additional source-row rejection performed when vertical scale is below
`0x1000`.

The controlled green impact spans 96 displayed frames and seven particle
lifetimes. New type-`0x1a` records are emitted at staggered intervals; expired
slots can be reused before the last particle finishes. The secondary renderer
makes 315 calls over the interval, with a maximum of six particles overlapping
in their original pass order. One peak frame contains 5,321 final stores to
4,101 unique destination pixels. Replaying the same original packed bytes,
transforms, order, palette operands, and R4600 saturating-add semantics produces
zero differences in the native blend-only output surface.

The current PC demo uses those captured transforms to display the complete
impact. This is verification scaffolding rather than a claim that the complete
particle state is already generated natively. Planar and vertical motion have
now crossed that boundary, as has the constructor at `0x8800b1fc`. The
type-`0x1a` animation path has crossed it as well. POC-100 maps the remaining
state-to-render bridge, and POC-110 now reconstructs its verified path. The PC
demo retains the captured table until POC-120 recovers particle scheduling and
POC-130 removes the last capture scaffolding from the demo.

### Type-`0x1a` state-to-render bridge

The observed `0x88001b90` path is mapped from the live object record to the
first accepted packed row at `0x88011874`. Native source
`native/src/original/ki15d_88001b90_type1a.c` implements the trace-proven
display-class-`0x60`, flags-`0` route as a host-neutral render descriptor.

The function obtains the frame pointer from record offset `0x30`, the signed
pixel step from `0x24`, base scale from `0x70`, optional x/y multipliers from
`0x58/0x5a`, and 24.8 position words from `0x68/0x6c`. Offset `0x8c` chooses a
0x100-byte source palette bank. The observed path also copies bytes `0x8e` and
`0x97` into palette-control globals; both are zero in all 315 calls and the
native bridge explicitly rejects nonzero values until their alternate palette
semantics are independently verified. Offsets `0xc8/0xca` supply the horizontal
minimum and width, with zero width replaced by 320. Display class `0x60` takes
the direct secondary-renderer branch used by every captured type-`0x1a`
particle.

Using `lo32(a * b)` for the R4600 multiply result, the observed equations are:

```text
x_scale = scale58 ? (lo32(word70 * scale58) >> 12) : word70
y_scale = scale5a ? (lo32(word70 * scale5a) >> 12) : word70
x_position = s32(word68) >> 8
y_position = s32(word6c) >> 8

y_fixed = lo32((s16(frame.origin_y) - top) * y_scale)
          + lo32((y_position << 4) + 0x800)
raw_y = s32(y_fixed) >> 12
if raw_y < vertical_min: no draw
if raw_y - scaled_height >= viewport: no draw
destination_y = min(raw_y, viewport - 1)

x_base = lo32((x_position << 4) + 0x800)
x_fixed = x_base - lo32(s16(frame.origin_x) * x_scale)  # step > 0
x_remainder = x_fixed & 0x0fff
destination_x = s32(x_fixed) >> 12

scaled_width  = lo32(s16(frame.width)  * x_scale) >> 12
scaled_height = lo32(s16(frame.height) * y_scale) >> 12
output_rows = min(scaled_height, raw_y - vertical_min + 1)
# While raw_y >= viewport, reject destination rows and advance packed source
# rows with the original vertical accumulator before selecting the first row.
```

The destination address is the selected framebuffer base plus
`destination_y * 640 + destination_x * 2`. Global byte `0x88086200 & 1`
selects base `0x80030000` or `0x80058000`. Rows proceed upward because the
positive `0x280` stride is subtracted after each row. All calls in this oracle
use pixel step `+2`; the statically visible negative-step branch adds the frame
x origin, complements the 12-bit x remainder, and adds mode selector `0x04`.
The native path implements that branch with synthetic boundary coverage, but
it remains dynamically unverified against an organic negative-step
type-`0x1a` call.

Horizontal span checks at `0x88010a50`-`0x88010a84` set mode bit `0x08` when
an otherwise visible sprite crosses either bound. The controlled trace has 261
unclipped calls and 54 calls crossing the 320-pixel right edge. Their executing
mode/callback pairs are `0x70/0x88011918` for x-upscaled unclipped rows,
`0x78/0x880118c0` for right-clipped rows, and `0x80/0x88011918` for downscaled
unclipped rows. Static table entries additionally establish
`0x64/0x88011a24` for reverse unclipped rows and `0x6c/0x880119c4` for reverse
clipped rows. The callback clips each scaled run against the active boundary;
it is unrelated to transparency or palette selection.

The vertical fraction is complemented before the packed loop. If its first
addition of `y_scale` remains at or below `0x0fff`, `0x88011840`-`0x8801185c`
rejects complete source rows until a row becomes visible. The trace records the
rejection count and cumulative byte advance from executing registers. The
first accepted cursor is therefore `frame + 8 + skipped_bytes + 1`, and its
live accumulator is
`complemented_fraction + (skipped_rows + 1) * y_scale`.

When the unclamped anchor is at or beyond the 240-line high edge,
`0x88010b44` first rejects wholly invisible spans. For a partially visible
span, `0x88010b78`-`0x88010bd8` walks the same packed row-size chain and
decrements the remaining output count once per rejected destination row. The
native descriptor preserves the clamped framebuffer destination, selected
source-row offset, adjusted accumulator, and remaining row count from this
path. Synthetic tests cover the exact edge, multiple rejected rows, combined
high clipping and downscaling, malformed row sizes, and an unmapped row walk.

Palette helper `0x88001ffc` forms source
`0x88090100 + (selected_bank << 8)`, saves it at `0x880359c8`, and copies 256
bytes to the working palette at `0x88090000`. The trace captures the helper's
source in `$a0`, the selected frame in renderer-entry `$v1`, and the live blend
table in `$t6`. The first actual signed-halfword palette load is captured at
`0x880118bc` for every call. These values do not depend on debugger reads of
potentially dirty R4600 cache lines.

Two independent controlled executions produced identical normalized trace
lines. The analyzer verified all 315 calls over 96 displayed ticks, including
one six-particle frame, the first horizontal-downscale state, source-row
rejection, 54 right-edge-clipped calls, and the last visible call of each of
seven lifetimes. Left-edge clipping, negative orientation, alternate display
classes, and nonzero render flags remain explicit limits of this POC-100
evidence.

POC-110 joins each trace's scalar record/header inputs to its independently
derived descriptor in `tests/fixtures/ki15d_88001b90_type1a_bridge.csv`. The
fixture contains no packed pixels, palettes, ROM bytes, or screenshots. The C
test seeds those values at their original guest addresses and matches all 315
frame, palette, framebuffer, clip-mode, callback, position, scale,
accumulator, selected-row, and output-row results.

For first, six-particle peak, and final calls, the developer-only comparison
build renders synthetic non-expressive packed rows through both the fixture
oracle and the state-derived descriptor. It compares the complete 320x240
framebuffer after every ordered peak call; both sources produce deterministic
hash `1a8693d815eebeed`. Additional tests cover zero and nonzero record scales,
low-32-bit R4600 multiplication, signed positions, reverse direction, every
horizontal clip classification, invisible spans, high vertical clamping,
below-top rejection, high-edge rejection/source walking, packed-row walking,
malformed dimensions/rows, unmapped guest memory, and explicit rejection of
unrecovered branches.

The reconstruction intentionally does not emulate palette-copy side effects,
negative-palette special handling, alternate display classes, or flag-driven
secondary calls. Nonzero record fields `0x8e` and `0x97` are also rejected
because their mode-`0x60` palette selection has not yet been reconstructed. It
returns an unsupported result for those states. POC-120
will supply the original emission and scheduling state; until then, only the
developer comparison harness consumes this bridge from captured scalar state.

## General object release

The leaf routine at `0x880054d0` receives an object record through `$fp` and
clears its 0x100 bytes in ascending 32-bit word order. It is separate from the
allocator's descending clear loop and is used when live objects expire. In the
Endokuken trace, the active type-`0x12` update calls it after the record's script
pointer becomes zero.

Three controlled MAME calls prove that the routine preserves guard words on
both sides of the record and leaves `$a2` and `$a3` at `$fp + 0x100`. The native
reconstruction produces the same complete 1 MiB RAM image for all three cases.

## Evidence levels

- **Trace-matched:** allocator scan, selected address, allocator clear, general
  release clear, common copy routine, animation-script setup, type-`0x1a`
  constructor, planar and vertical particle motion, live type-`0x1f` call, the
  complete type `0x13` -> `0x12` Endokuken record lifecycle, and the type-`0x1a`
  path from packed frame selection to sampled framebuffer writes.
- **Statically and dynamically confirmed:** the separate contact type `0x14` ->
  `0x15` transition and the receiver-side series of type-`0x1a` records.
- **Provisional:** semantic field names; the exact visual purpose of types
  `0x1e`, `0x1f`, and `0x20`; and whether type `0x15` contributes any separately
  visible pixels before the confirmed type-`0x1a` series.
- **Expert visual confirmation:** the green effect shown in both the earlier
  Riptor capture and the controlled Fulgore capture is the Endokuken impact.

The full write trace for one type-`0x1a` lifetime now assigns original code
addresses to its planar position, vertical position, velocity, animation,
scale growth, and release. The constructor is now isolated and trace-matched.
The installed-stream animation path is also isolated and matched, while the
rest of the general interpreter remains partial. One natively advanced lifetime
now matches the live pre-release checkpoint and exact release tick. Work moves
to replacing the captured transform timeline one remaining renderer field at a
time.
