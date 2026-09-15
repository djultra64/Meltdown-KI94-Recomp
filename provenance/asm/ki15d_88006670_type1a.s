# Killer Instinct v1.5d, R4600 little-endian
# Common routine extent 0x88006670..0x880067fb; 396 bytes
# Extent SHA-256: 2b7c33a8be8ac4f6ba88827611ad2cc07a67b0c22ea006dac19b01a3aaf38edf
#
# This excerpt retains the verified already-installed-script path. The omitted
# 0x8800669c..0x88006784 block initializes a null script from an object
# descriptor and remains outside the current native reconstruction.

88006670: 3c018808  lui       $at,0x8808
88006674: ac3f727c  sw        $ra,0x727c($at)
88006678: 8fc50020  lw        $a1,0x20($fp)
8800667c: 10a0001e  beqz      $a1,0x880066f8
88006680: 00000000  nop
88006684: 8fc40018  lw        $a0,0x18($fp)
88006688: 28810100  slti      $at,$a0,0x100
8800668c: 10200054  beqz      $at,0x880067e0
88006690: 00000000  nop
88006694: 0a0019ea  j         0x880067a8
88006698: 00000000  nop

# Command dispatch and normal token-duration path.
88006788: 3c188803  lui       $t8,0x8803
8800678c: 00043080  sll       $a2,$a0,2
88006790: 27183900  addiu     $t8,$t8,0x3900
88006794: 0306c021  addu      $t8,$t8,$a2
88006798: 8f180000  lw        $t8,0($t8)
8800679c: 03000008  jr        $t8
880067a0: 00000000  nop
880067a4: 24a50004  addiu     $a1,$a1,4
880067a8: 90a60000  lbu       $a2,0($a1)
880067ac: 90a40001  lbu       $a0,1($a1)
880067b0: 10c0fff5  beqz      $a2,0x88006788
880067b4: 00000000  nop
880067b8: 87d80042  lh        $t8,0x42($fp)
880067bc: 00042400  sll       $a0,$a0,16
880067c0: 24a50002  addiu     $a1,$a1,2
880067c4: 0098001a  div       $a0,$t8
880067c8: 93c40018  lbu       $a0,0x18($fp)
880067cc: a3c60014  sb        $a2,0x14($fp)
880067d0: afc50020  sw        $a1,0x20($fp)
880067d4: 0000c012  mflo      $t8
880067d8: 00982021  addu      $a0,$a0,$t8
880067dc: 00000000  nop
880067e0: 2484ff00  addiu     $a0,$a0,-0x100
880067e4: 0480fff0  bltz      $a0,0x880067a8
880067e8: afc40018  sw        $a0,0x18($fp)
880067ec: 3c1f8808  lui       $ra,0x8808
880067f0: 8fff727c  lw        $ra,0x727c($ra)
880067f4: 03e00008  jr        $ra
880067f8: 00000000  nop

# Opcode 0x10 handler at command-table index 0x10; 60 bytes.
# SHA-256: 50108c0f95e6c9aa83f209fd3cd3f5e30169a0337f27032898b66ffd54a9b850
88006c80: 90b80002  lbu       $t8,2($a1)
88006c84: 1300fec7  beqz      $t8,0x880067a4
88006c88: 00000000  nop
88006c8c: 2b810002  slti      $at,$gp,2
88006c90: 10200006  beqz      $at,0x88006cac
88006c94: 03805825  move      $t3,$gp
88006c98: 2b01000a  slti      $at,$t8,0xa
88006c9c: 14200003  bnez      $at,0x88006cac
88006ca0: 00000000  nop
88006ca4: 3b9c0001  xori      $gp,$gp,0x0001
88006ca8: 24180001  li        $t8,1
88006cac: 0e0018c5  jal       0x88006314
88006cb0: 00000000  nop
88006cb4: 0a0019e9  j         0x880067a4
88006cb8: 0160e025  move      $gp,$t3

# Opcode 0x14 terminator; 8 bytes.
# SHA-256: 7c4a9c2c9f96d2021aa0567e37f254e10fbf799ae32f2595333d0792b01db1a3
88006d88: 0a0019fb  j         0x880067ec
88006d8c: afc00020  sw        $0,0x20($fp)

# Ordering helper called by opcode 0x10; 120 bytes.
# SHA-256: 1f4683ffa6002dc9a67c0aee0d6bf7ee9eee23b9b6f9b4f03f436fcdba6a7599
88006314: 3c088808  lui       $t0,0x8808
88006318: 250872a0  addiu     $t0,$t0,0x72a0
8800631c: 250c0020  addiu     $t4,$t0,0x20
88006320: 0198c021  addu      $t8,$t4,$t8
88006324: 930affdf  lbu       $t2,-0x21($t8)
88006328: 25890020  addiu     $t1,$t4,0x20
8800632c: 115c0015  beq       $t2,$gp,0x88006384
88006330: 00000000  nop
88006334: 2718ffff  addiu     $t8,$t8,-1
88006338: 910a0000  lbu       $t2,0($t0)
8800633c: 25080001  addiu     $t0,$t0,1
88006340: 115cfffd  beq       $t2,$gp,0x88006338
88006344: 00000000  nop
88006348: 170c0003  bne       $t8,$t4,0x88006358
8800634c: 00000000  nop
88006350: 2508ffff  addiu     $t0,$t0,-1
88006354: 03805025  move      $t2,$gp
88006358: 258c0001  addiu     $t4,$t4,1
8800635c: 152cfff6  bne       $t1,$t4,0x88006338
88006360: a18affff  sb        $t2,-1($t4)
88006364: 3c0a8808  lui       $t2,0x8808
88006368: 3c0c8808  lui       $t4,0x8808
8800636c: 254a72a0  addiu     $t2,$t2,0x72a0
88006370: 258c72c0  addiu     $t4,$t4,0x72c0
88006374: 8d480020  lw        $t0,0x20($t2)
88006378: 254a0004  addiu     $t2,$t2,4
8800637c: 154cfffd  bne       $t2,$t4,0x88006374
88006380: ad48fffc  sw        $t0,-4($t2)
88006384: 03e00008  jr        $ra
88006388: 00000000  nop
