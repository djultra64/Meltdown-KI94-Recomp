# Curated technical knowledge

`provenance/knowledge.json` is the machine-checked index of durable facts and
open uncertainties for Killer Instinct v1.5d. It currently consolidates the 16
seed facts (`K-001` through `K-016`) and 14 questions (`U-001` through `U-014`)
from local planning. New records may be added with the same stable ID patterns;
the current counts are a migration baseline, not a schema limit.

Each record carries confidence, a narrow scope, limits, an invalidation
condition, source path and locator, and links to existing function provenance
where applicable. Evidence classes keep these claims distinct:

- `observation`: values or order captured from declared original execution;
- `static-analysis`: findings from bytes, assembly, tables, or driver source;
- `differential-verification`: an original/native bounded comparison;
- `implementation`: what repository code currently supplies;
- `review`: an independent inspection result for a declared candidate; and
- `runtime-integration`: evidence that a native runtime actually reaches it.

Confidence is one of `observed`, `hypothesis`, `statically-derived`,
`differentially-verified`, or `refuted`. These labels do not replace the
`reconstruction.status` or `verification.status` fields in
`provenance/functions/*.json`. The function records remain authoritative for
their own boundaries. A function may match a bounded oracle while remaining
partial, absent from the demo, or dependent on captured upstream state.

## Current boundary

Status reviewed September 19, 2026. The current native pilot includes connected
execution of the bounded `0x8800099c` to `0x880012b8` phase, selected contact,
projection, frame-selection and renderer transactions, and scoped snapshot
replay. Their source and state contracts are recorded in
[`provenance/native_pilot_manifest.json`](../provenance/native_pilot_manifest.json).
Diagnostic initialization and captured upstream state still limit these proofs;
the executable demo continues to use a captured effect timeline.

The separately registered startup-clear region covers only
`[0x880001c4, 0x880001ec)`. Retained original endpoint comparison covers GPRs,
HI/LO, Status, raw FPRs and 1 MiB main RAM. The original clear targets were
already zero, so a separate nonzero-memory test provides native write-effect
coverage. This is not a full ordered-store differential, FCSR/TLB validation,
or whole-boot proof. The runtime deliberately rejects snapshots of this region.

The historical scoped observations below remain valid within their stated
limits. This summary does not promote unfinished routines or imply that every
recent local investigation has been incorporated into the knowledge catalog.

The strongest connected evidence remains the controlled type-`0x1a` Endokuken
impact: 49 eligible emitter invocations, seven allocations, 322 updates, 315
draws across 96 visible groups, and seven releases. The initial contact seed is
`31/78/55/00` hexadecimal; the emitter decrements `0x31` before the first
constructor sees `0x30`. Each particle has 45 visible updates followed by
terminating update 46. These observations do not establish a once-per-frame
emitter call, VBlank scheduling, or full native effect integration.

The bridge at `0x88001b90` has 315 bounded descriptor comparisons but still
depends on upstream projected fields and a valid frame pointer. The general
animation routine's supported type-`0x1a` installed-stream subset and the bridge
therefore retain their existing `in-progress`/`partial` provenance statuses.
The driver hardware map is a working baseline, not an executed device contract.
The bounded contact initializer at `0x8800a2e4` now matches two fresh 18-case
original-CPU runs. The matrix covers disabled/enabled routes, signed wrap and
clamp behavior, byte truncation, all lookup limits, overlapping lookup ranges,
attacker/receiver aliasing, and a descriptor reread alias. Executing load/store
values agreed with ordered 16 KiB window and lookup-table snapshots in this
controlled `-nodrc` harness. That agreement is not a universal cache-coherence
claim, and the initializer still does not recover hit selection, collision,
damage, caller scheduling, or temporary-register ABI state.

## Checking and local recovery

Run `make knowledge-check` (also part of `make check`) to validate IDs,
confidence, evidence classes, repository-safe paths, locators, cross-links, and
function-record references. A missing optional ignored source is reported as
`UNAVAILABLE`, not as success evidence.

The complementary shared verification entry point is
`provenance/verification.json`, checked by `make verify-check` and replayed with
`python3 tools/verify_evidence.py replay --anchor all`. It pins the public and
private identities for only the exhaustive emitter-decision and bounded
state-to-render bridge anchors. Public metadata `PASS`, retained-capture replay
`PASS`, freshly rebuilt native-test `PASS`, and fresh original acquisition are
separate claims. The current retained logs are reviewed legacy evidence, not
new MAME executions or self-attesting capture records.
The two anchors' meaning-bearing metadata is an exact versioned contract, so a
wording change that would strengthen or alter an ABI, cache, acquisition,
completion, admission, or limits claim requires review and fails the public
validator until its trusted contract is deliberately updated.

After inspecting the fixed allowlist in `tools/knowledge.py`, create and test a
local metadata bundle with unique fresh paths:

```sh
python3 tools/knowledge.py export \
  --bundle work/knowledge-backups/know-090-YYYYMMDD
python3 tools/knowledge.py restore-check \
  --bundle work/knowledge-backups/know-090-YYYYMMDD \
  --destination work/knowledge-backups/know-090-YYYYMMDD-restored \
  --manifest-sha256 <hash printed by export>
```

Both destinations must not already exist. The bundle is an exact, UTF-8
text/metadata-only, same-volume copy. It excludes game inputs, assets, raw
captures, dumps, binaries, generated assembly, and the unverified POC-120B1
implementation drafts. Those exclusions make it unsuitable for disk-loss
recovery; raw evidence is regenerated from lawful inputs using the commands in
the technical verification documents.
