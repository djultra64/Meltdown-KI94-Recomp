# Initial hardware map

Primary source: the current Killer Instinct driver in
[MAME](https://github.com/mamedev/mame/blob/master/src/mame/rare/kinst.cpp).
This is a working baseline, not a definitive description of the game software.

## CPU and video

- Main CPU: 100 MHz little-endian MIPS R4600.
- Instruction cache: 16 KiB.
- Data cache: 16 KiB.
- Visible display: 320 × 240 at approximately 58.96 Hz.
- Pixels: BGR555, two pixels packed into each 32-bit word.
- Visible buffer in low RAM: offset `0x30000` or `0x58000`, selected by a video
  control bit.
- VBlank reaches interrupt line 0; ATA reaches interrupt line 1.

## Known KI1 physical memory map

| Range | Size | Initial use |
|---|---:|---|
| `0x00000000–0x0007ffff` | 512 KiB | Low RAM, including framebuffer |
| `0x08000000–0x087fffff` | 8 MiB | Main RAM with loaded code and data |
| `0x10000080–0x100000b3` | — | Board inputs and controls |
| `0x10000100–0x1000013f` | — | ATA CS0 registers |
| `0x10000170–0x10000173` | — | ATA CS1 register |
| `0x1fc00000–0x1fc7ffff` | 512 KiB | U98 boot ROM |

KSEG0/KSEG1 virtual addresses must be normalized to physical addresses where
appropriate. For example, virtual `0x887xxxxx` maps to physical `0x087xxxxx`.
The native runtime already models this basic translation. TLB behavior and
other segments will be added only when traces demonstrate that they are needed.

## KI1 control registers

| Address | Read | Write |
|---|---|---|
| `0x10000080` | Player 1 | Framebuffer selection |
| `0x10000088` | Player 2 | Audio reset |
| `0x10000090` | Volume | Audio control |
| `0x10000098` | No known use | Audio data |
| `0x100000a0` | DIP switches | No effect |
| `0x100000b0` | — | Coin control |

KI2 rearranges several of these registers, so revisions are never mixed in one
analysis.

## Porting implications

U98 contains the boot code and a compressed stream that reconstructs at least
three initial segments in main RAM, including the main-loop bytes. The
extraction was verified against a real MAME capture, allowing loaded functions
to be linked to a reproducible source segment in U98.

U98 is still not the whole game. During execution, the IDE disk supplies
additional data and assets into later portions of the 8 MiB region. The analysis
must preserve both paths—U98 decompression and ATA loads—rather than attributing
all main-RAM content to the disk.

The historical type-`0x1a` frame-lookup claim of a CPU/debugger mismatch at
`0x88090164` is refuted. Its script computed that address from a record byte;
the original delay slot instead selects class 11 and the CPU loads from
`0x88090140`, where the CPU result and backing value agree. This correction
does not prove general cache coherence or erase other cache risks; it only
removes that mislabelled same-address example.
