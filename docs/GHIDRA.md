# Ghidra project

Pinned version: Ghidra 12.1.3, installed through the
`org.ghidra_sre.Ghidra` Flatpak.

Two separate raw programs are imported with language
`MIPS:LE:64:64-32addr` and compiler specification `n32`:

| Program | Virtual base | Contents |
|---|---:|---|
| `bootrom.bin` | `0xbfc00000` | 512 KiB U98 boot ROM |
| `mainram-10s.bin` | `0x88000000` | 8 MiB after ten seconds |

`ghidra_scripts/SeedKi15d.java` adds only entry points supported by captured
evidence:

- boot vector and boot entry;
- loaded-image vector and entry;
- jump `0x8800034c → 0x8802aa24`;
- provisional main controller at `0x8802aa24`;
- recurring loop head at `0x8802ae14`; and
- VBlank wait routine at `0x88012614`.

The generated project lives in `work/ghidra/` and is not published. Confirmed
labels are copied to provenance records so Ghidra is never the only store of
project knowledge.

Run the complete reproducible import with:

```sh
tools/import-ki15d-ghidra.sh
```

The RAM snapshot includes later data loaded from disk. For functions between
`0x88000000` and `0x88032caf`, bytes are also checked against `rom-0.bin`, the
U98-extracted segment before runtime modification.

The R4600 executes 64-bit instructions but KI uses 32-bit virtual addresses.
This combination is declared directly by Ghidra 12.1.3 and prevents addresses
from being treated as 64-bit pointers. Critical functions must always be
checked against raw assembly: decompiler output is evidence, not ground truth.
