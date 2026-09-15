# Known Killer Instinct v1.5d callers of 0x8800700c.
# These are focused excerpts, not complete function boundaries.

# A fighter-local byte at offset 0xf9 counts down. Every even value allocates
# type 0x1f from the current fighter record in $fp.
88002b88: 93c600f9  lbu       $a2,0xf9($fp)
88002b8c: 10c00018  beqz      $a2,0x88002bf0
88002b90: 00000000  nop
88002b94: 24c6ffff  addiu     $a2,$a2,-1
88002b98: a3c600f9  sb        $a2,0xf9($fp)
88002b9c: 30c60001  andi      $a2,$a2,0x0001
88002ba0: 14c00013  bnez      $a2,0x88002bf0
88002ba4: 00000000  nop
88002ba8: 0e0014fd  jal       0x880053f4
88002bac: 00000000  nop
88002bb0: 03c0a825  move      $s5,$fp
88002bb4: 0e001c03  jal       0x8800700c
88002bb8: 2407001f  li        $a3,0x1f
88002bbc: 92a70000  lbu       $a3,0($s5)
88002bc0: a0c70055  sb        $a3,0x55($a2)
88002bc4: 8ea7008c  lw        $a3,0x8c($s5)
88002bc8: 34e70080  ori       $a3,$a3,0x0080
88002bcc: acc7008c  sw        $a3,0x8c($a2)
88002bd0: 8ea70034  lw        $a3,0x34($s5)
88002bd4: acc70034  sw        $a3,0x34($a2)
88002bd8: 8ea70014  lw        $a3,0x14($s5)
88002bdc: acc70014  sw        $a3,0x14($a2)
88002be0: 8ea70024  lw        $a3,0x24($s5)
88002be4: acc70024  sw        $a3,0x24($a2)
88002be8: 24070014  li        $a3,0x14
88002bec: a0c70038  sb        $a3,0x38($a2)

# Command-interpreter opcode 0x48 selects a fighter record from command data
# and creates type 0x1e before applying command-specific fields.
88007cdc: 0e0014fd  jal       0x880053f4
88007ce0: 00000000  nop
88007ce4: 90a70003  lbu       $a3,3($a1)
88007ce8: 3c088809  lui       $t0,0x8809
88007cec: 2508bc00  addiu     $t0,$t0,-0x4400
88007cf0: 00fcb026  xor       $s6,$a3,$gp
88007cf4: 00164a00  sll       $t1,$s6,8
88007cf8: 0128a821  addu      $s5,$t1,$t0
88007cfc: 0e001c03  jal       0x8800700c
88007d00: 2407001e  li        $a3,0x1e

# Command-interpreter opcode 0x51 derives the opposing fighter record from
# $gp and creates type 0x20.
88007ff8: 0e0014fd  jal       0x880053f4
88007ffc: 00000000  nop
88008000: 3c158809  lui       $s5,0x8809
88008004: 001ca200  sll       $s4,$gp,8
88008008: 26b5bd00  addiu     $s5,$s5,-0x4300
8800800c: 02b4a823  subu      $s5,$s5,$s4
88008010: 0e001c03  jal       0x8800700c
88008014: 24070020  li        $a3,0x20
