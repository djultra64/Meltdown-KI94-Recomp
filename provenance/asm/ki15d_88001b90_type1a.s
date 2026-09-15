# KI v1.5d R4600LE disassembly excerpt.
# Function: secondary object state-to-render bridge at 0x88001b90.
# Extent: [0x88001b90, 0x88001e78), 744 bytes.
88001b90: 27bdfff8  addiu     $sp,$sp,-8
88001b94: 3c018808  lui       $at,0x8808
88001b98: afbf0000  sw        $ra,0($sp)
88001b9c: ac387b40  sw        $t8,0x7b40($at)
88001ba0: 83dc008c  lb        $gp,0x8c($fp)
88001ba4: 0380c025  move      $t8,$gp
88001ba8: 07010002  bgez      $t8,0x88001bb4
88001bac: 00000000  nop
88001bb0: 241c0043  li        $gp,0x43
88001bb4: 330300ff  andi      $v1,$t8,0x00ff
88001bb8: 240100ff  li        $at,0xff
88001bbc: 1061ffe6  beq       $v1,$at,0x88001b58
88001bc0: 00000000  nop
88001bc4: 0e0007ff  jal       0x88001ffc
88001bc8: 00000000  nop
88001bcc: 0701000a  bgez      $t8,0x88001bf8
88001bd0: 00000000  nop
88001bd4: 3318007f  andi      $t8,$t8,0x007f
88001bd8: 3c1c8809  lui       $gp,0x8809
88001bdc: 0018c200  sll       $t8,$t8,8
88001be0: 379c0100  ori       $gp,$gp,0x0100
88001be4: 031cc021  addu      $t8,$t8,$gp
88001be8: df1700f8  ld        $s7,0xf8($t8)
88001bec: fc77fff8  sd        $s7,-8($v1)
88001bf0: df1700f0  ld        $s7,0xf0($t8)
88001bf4: fc77fff0  sd        $s7,-0x10($v1)
88001bf8: 97d60058  lhu       $s6,0x58($fp)
88001bfc: 8fd70068  lw        $s7,0x68($fp)
88001c00: 8fd8006c  lw        $t8,0x6c($fp)
88001c04: 8fc90070  lw        $t1,0x70($fp)
88001c08: 8fc30030  lw        $v1,0x30($fp)
88001c0c: 83cc0024  lb        $t4,0x24($fp)
88001c10: 97da00c8  lhu       $k0,0xc8($fp)
88001c14: 0000e025  move      $gp,$0
88001c18: 0017ba03  sra       $s7,$s7,8
88001c1c: 0018c203  sra       $t8,$t8,8
88001c20: 12c00005  beqz      $s6,0x88001c38
88001c24: 01205825  move      $t3,$t1
88001c28: 01360018  mult      $t1,$s6
88001c2c: 00004812  mflo      $t1
88001c30: 00094b02  srl       $t1,$t1,12
88001c34: 00000000  nop
88001c38: 97d6005a  lhu       $s6,0x5a($fp)
88001c3c: 12c00005  beqz      $s6,0x88001c54
88001c40: 00000000  nop
88001c44: 01760018  mult      $t3,$s6
88001c48: 00005812  mflo      $t3
88001c4c: 000b5b02  srl       $t3,$t3,12
88001c50: 00000000  nop
88001c54: 93d60094  lbu       $s6,0x94($fp)
88001c58: 12c0006c  beqz      $s6,0x88001e0c
88001c5c: 00000000  nop
88001c60: 93c40097  lbu       $a0,0x97($fp)
88001c64: 3c018808  lui       $at,0x8808
88001c68: a02461fd  sb        $a0,0x61fd($at)
88001c6c: 93c4008e  lbu       $a0,0x8e($fp)
88001c70: a02461fc  sb        $a0,0x61fc($at)
88001c74: 24010060  li        $at,0x60
88001c78: 12c10064  beq       $s6,$at,0x88001e0c
88001c7c: 00000000  nop
88001c80: 93c40000  lbu       $a0,0($fp)
88001c84: 3c038809  lui       $v1,0x8809
88001c88: 2401001e  li        $at,0x1e
88001c8c: 10810004  beq       $a0,$at,0x88001ca0
88001c90: 34630140  ori       $v1,$v1,0x0140
88001c94: 24010012  li        $at,0x12
88001c98: 14810002  bne       $a0,$at,0x88001ca4
88001c9c: 00000000  nop
88001ca0: 93c40055  lbu       $a0,0x55($fp)
88001ca4: 00042080  sll       $a0,$a0,2
88001ca8: 28810040  slti      $at,$a0,0x40
88001cac: 1020000d  beqz      $at,0x88001ce4
88001cb0: 00000000  nop
88001cb4: 3c1b8809  lui       $k1,0x8809
88001cb8: 937b9f0c  lbu       $k1,-0x60f4($k1)
88001cbc: 13600009  beqz      $k1,0x88001ce4
88001cc0: 00000000  nop
88001cc4: 3c038809  lui       $v1,0x8809
88001cc8: 24638d84  addiu     $v1,$v1,-0x727c
88001ccc: 00642021  addu      $a0,$v1,$a0
88001cd0: 8c830000  lw        $v1,0($a0)
88001cd4: 0016b080  sll       $s6,$s6,2
88001cd8: 00761821  addu      $v1,$v1,$s6
88001cdc: 0a000741  j         0x88001d04
88001ce0: 8c630320  lw        $v1,0x320($v1)
88001ce4: 00641821  addu      $v1,$v1,$a0
88001ce8: 8c630000  lw        $v1,0($v1)
88001cec: 90640000  lbu       $a0,0($v1)
88001cf0: 10960004  beq       $a0,$s6,0x88001d04
88001cf4: 00000000  nop
88001cf8: 8c640004  lw        $a0,4($v1)
88001cfc: 0a00073b  j         0x88001cec
88001d00: 00641821  addu      $v1,$v1,$a0
88001d04: 93c40014  lbu       $a0,0x14($fp)
88001d08: 14800002  bnez      $a0,0x88001d14
88001d0c: 00000000  nop
88001d10: 24040001  li        $a0,1
88001d14: 90760002  lbu       $s6,2($v1)
88001d18: 02c4082a  slt       $at,$s6,$a0
88001d1c: 10200002  beqz      $at,0x88001d28
88001d20: 00000000  nop
88001d24: 02c02025  move      $a0,$s6
88001d28: 0004c880  sll       $t9,$a0,2
88001d2c: 0079b021  addu      $s6,$v1,$t9
88001d30: 8c7b0004  lw        $k1,4($v1)
88001d34: 24010001  li        $at,1
88001d38: 10810006  beq       $a0,$at,0x88001d54
88001d3c: 8ed90004  lw        $t9,4($s6)
88001d40: 173b0004  bne       $t9,$k1,0x88001d54
88001d44: 00000000  nop
88001d48: 26d6fffc  addiu     $s6,$s6,-4
88001d4c: 0a00074d  j         0x88001d34
88001d50: 2484ffff  addiu     $a0,$a0,-1
88001d54: 00791821  addu      $v1,$v1,$t9
88001d58: 0a00075e  j         0x88001d78
88001d5c: 24160060  li        $s6,0x60
88001d60: 93c40000  lbu       $a0,0($fp)
88001d64: 24010012  li        $at,0x12
88001d68: 14810028  bne       $a0,$at,0x88001e0c
88001d6c: 00000000  nop
88001d70: 0a000783  j         0x88001e0c
88001d74: 24160060  li        $s6,0x60
88001d78: 93c40095  lbu       $a0,0x95($fp)
88001d7c: 30840004  andi      $a0,$a0,0x0004
88001d80: 14800019  bnez      $a0,0x88001de8
88001d84: 00000000  nop
88001d88: 3c048808  lui       $a0,0x8808
88001d8c: 808461fd  lb        $a0,0x61fd($a0)
88001d90: 2401005f  li        $at,0x5f
88001d94: 10810014  beq       $a0,$at,0x88001de8
88001d98: 00000000  nop
88001d9c: 27bdfff0  addiu     $sp,$sp,-0x10
88001da0: afbf0000  sw        $ra,0($sp)
88001da4: afa90004  sw        $t1,4($sp)
88001da8: afab0008  sw        $t3,8($sp)
88001dac: afbe000c  sw        $fp,0xc($sp)
88001db0: 97db00ca  lhu       $k1,0xca($fp)
88001db4: 00002025  move      $a0,$0
88001db8: 17600002  bnez      $k1,0x88001dc4
88001dbc: 00000000  nop
88001dc0: 241b0140  li        $k1,0x140
88001dc4: 3c1e8808  lui       $fp,0x8808
88001dc8: 8fde7b40  lw        $fp,0x7b40($fp)
88001dcc: 0e0041ea  jal       0x880107a8
88001dd0: 24120280  li        $s2,0x280
88001dd4: 8fbf0000  lw        $ra,0($sp)
88001dd8: 8fa90004  lw        $t1,4($sp)
88001ddc: 8fab0008  lw        $t3,8($sp)
88001de0: 8fbe000c  lw        $fp,0xc($sp)
88001de4: 27bd0010  addiu     $sp,$sp,0x10
88001de8: 8fd70068  lw        $s7,0x68($fp)
88001dec: 8fd8006c  lw        $t8,0x6c($fp)
88001df0: 97da00c8  lhu       $k0,0xc8($fp)
88001df4: 0000e025  move      $gp,$0
88001df8: 8fc30030  lw        $v1,0x30($fp)
88001dfc: 83cc0024  lb        $t4,0x24($fp)
88001e00: 24160000  li        $s6,0
88001e04: 0017ba03  sra       $s7,$s7,8
88001e08: 0018c203  sra       $t8,$t8,8
88001e0c: 93c40095  lbu       $a0,0x95($fp)
88001e10: 309b0001  andi      $k1,$a0,0x0001
88001e14: 1360000b  beqz      $k1,0x88001e44
88001e18: 00000000  nop
88001e1c: 30840008  andi      $a0,$a0,0x0008
88001e20: 10800011  beqz      $a0,0x88001e68
88001e24: 00000000  nop
88001e28: 8fce00d0  lw        $t6,0xd0($fp)
88001e2c: 3c157fff  lui       $s5,0x7fff
88001e30: 3c018808  lui       $at,0x8808
88001e34: 24160001  li        $s6,1
88001e38: 0000a025  move      $s4,$0
88001e3c: 36b5ffff  ori       $s5,$s5,0xffff
88001e40: ac2e61f8  sw        $t6,0x61f8($at)
88001e44: 97db00ca  lhu       $k1,0xca($fp)
88001e48: 00002025  move      $a0,$0
88001e4c: 17600002  bnez      $k1,0x88001e58
88001e50: 00000000  nop
88001e54: 241b0140  li        $k1,0x140
88001e58: 3c1e8808  lui       $fp,0x8808
88001e5c: 8fde7b40  lw        $fp,0x7b40($fp)
88001e60: 0e0041ea  jal       0x880107a8
88001e64: 24120280  li        $s2,0x280
88001e68: 8fbf0000  lw        $ra,0($sp)
88001e6c: 27bd0008  addiu     $sp,$sp,8
88001e70: 03e00008  jr        $ra
88001e74: 00000000  nop
