# External references

## `nekuz0r/ki-rom`

- Repository: [github.com/nekuz0r/ki-rom](https://github.com/nekuz0r/ki-rom)
- Examined revision: `8e4344cfd2b039e2ca97c9f115bb8f1acaff804b`
- Declared license: AGPL-3.0-only.
- Local copy: `work/references/ki-rom/`, ignored by Git.

This is an alternative boot ROM for KI1/KI2 boards, not a decompilation of the
game. Meltdown does not adopt it as a replacement for the original ROM; it is a
separate technical reference.

### Contribution to the analysis

Its `kipack` tool implements the MIPS instruction compression format used
inside U98. The examined revision passed all 14 local synthetic tests. Applied
to the verified KI v1.5d ROM, it produced:

| Segment | Physical load address | Size | SHA-256 |
|---|---:|---:|---|
| `rom-0` | `0x08000000` | 208,048 | `5dd923067b1a3398d4d26b6558e46cf396ab69b66afd5f8b183885b820e79b79` |
| `rom-1` | `0x08033900` | 332,032 | `d3f0ffa007cb625fa95674bfe6be950b5b1ec04da24f70f948edf74abf365f15` |
| `rom-2` | `0x08033880` | 128 | `e66076c5a40964ca3f08a9393c9f681bdaeedd2f11e743637a7eb41a839de1d4` |

An independent comparison against `mainram-10s.bin` confirmed `rom-0` and
`rom-2` byte-for-byte. `rom-1` retained 331,098 of 332,032 bytes; 934 bytes had
already changed during ten seconds of execution. This establishes that the
initial main-RAM program comes from U98's compressed stream and gives every
included function a reproducible segment offset.

The repository also provides candidate patch addresses for v1.5d. Four were
checked directly against our bytes:

| Address | Verified original bytes | Interpretation |
|---|---|---|
| `0x880003ca` | byte `0x22` within `sb $v0,...` | Patch changes the source register to zero |
| `0x880105c0` | `mfc0 $v0,SR` | Start of the block bypassing disk validation |
| `0x880266d0` | `sd $a1,-0x20($sp)` | Detour point after interrupt-stack setup |
| `0x8802c034` | `addiu $a1,$a1,-1` | Decrement implementing volume fade |

These labels describe the external project's intent. Within Meltdown they
remain hints until our own MAME traces confirm the corresponding behavior.

### License and evidence boundary

No AGPL code is copied into the native core or Meltdown's tools. The external
tool runs as a separate reference, and only factual results are retained:
addresses, hashes, and comparisons of user-supplied dumps. Incorporating its
implementation later would require an explicit project-license decision.

External addresses never become verified automatically. They are accepted only
after confirming the revision and original bytes and, when a label implies
behavior, obtaining an independent MAME trace.
