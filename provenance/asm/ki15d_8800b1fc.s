# Killer Instinct v1.5d, R4600 little-endian
# Constructor body: segment 0 offset 0x0000b1fc; 220 bytes
# Constructor SHA-256: e189cfaff19e99200bbe66fe01e16faf805c50c23d9831ccc5cc68e8b6950e12

8800b1fc: 3c018808  lui       $at,0x8808
8800b200: ac3f7274  sw        $ra,0x7274($at)
8800b204: 0e0014fd  jal       0x880053f4
8800b208: 00000000  nop
8800b20c: 00c05025  move      $t2,$a2
8800b210: 2414001a  li        $s4,0x1a
8800b214: a1540000  sb        $s4,0($t2)
8800b218: 93d400c7  lbu       $s4,0xc7($fp)
8800b21c: 3294001f  andi      $s4,$s4,0x001f
8800b220: a154008e  sb        $s4,0x8e($t2)
8800b224: 8fd40008  lw        $s4,8($fp)
8800b228: ad540008  sw        $s4,8($t2)
8800b22c: 8fd40004  lw        $s4,4($fp)
8800b230: ad540004  sw        $s4,4($t2)
8800b234: 8fd40074  lw        $s4,0x74($fp)
8800b238: 17800003  bnez      $gp,0x8800b248
8800b23c: 8fd50078  lw        $s5,0x78($fp)
8800b240: 0014a023  negu      $s4,$s4
8800b244: 0015a823  negu      $s5,$s5
8800b248: a554007c  sh        $s4,0x7c($t2)
8800b24c: a5550080  sh        $s5,0x80($t2)
8800b250: 93c600c4  lbu       $a2,0xc4($fp)
8800b254: 000630c0  sll       $a2,$a2,3
8800b258: a5460084  sh        $a2,0x84($t2)
8800b25c: 000630c0  sll       $a2,$a2,3
8800b260: 24c6f400  addiu     $a2,$a2,-0xc00
8800b264: a546007e  sh        $a2,0x7e($t2)
8800b268: 93c700c6  lbu       $a3,0xc6($fp)
8800b26c: 8fd4000c  lw        $s4,0xc($fp)
8800b270: 00073a00  sll       $a3,$a3,8
8800b274: 0287a021  addu      $s4,$s4,$a3
8800b278: 24070060  li        $a3,0x60
8800b27c: ad54000c  sw        $s4,0xc($t2)
8800b280: a1470094  sb        $a3,0x94($t2)
8800b284: 93c600c4  lbu       $a2,0xc4($fp)
8800b288: 24071000  li        $a3,0x1000
8800b28c: 28c1001a  slti      $at,$a2,0x1a
8800b290: 10200003  beqz      $at,0x8800b2a0
8800b294: 00000000  nop
8800b298: 00063980  sll       $a3,$a2,6
8800b29c: 24e70800  addiu     $a3,$a3,0x800
8800b2a0: a5470058  sh        $a3,0x58($t2)
8800b2a4: a547005a  sh        $a3,0x5a($t2)
8800b2a8: 24070002  li        $a3,2
8800b2ac: a1470024  sb        $a3,0x24($t2)
8800b2b0: 2407fff6  li        $a3,-0xa
8800b2b4: ad47003c  sw        $a3,0x3c($t2)
8800b2b8: 240700a0  li        $a3,0xa0
8800b2bc: ad470010  sw        $a3,0x10($t2)
8800b2c0: 93c700c7  lbu       $a3,0xc7($fp)
8800b2c4: 3c108801  lui       $s0,0x8801
8800b2c8: 2610b2d8  addiu     $s0,$s0,-0x4d28
8800b2cc: 92100000  lbu       $s0,0($s0)
8800b2d0: 0a002c76  j         0x8800b1d8
8800b2d4: 00073942  srl       $a3,$a3,5

# Shared setup tail reached by the jump above; segment offset 0x0000b1d8.
# 36-byte SHA-256: eb4ec0eff8eaf5608636fdf2f2bd8046936ef3b513f051e1927c2ec23760b9cb
8800b1d8: 3c040100  lui       $a0,0x0100
8800b1dc: 34840100  ori       $a0,$a0,0x0100
8800b1e0: ad440040  sw        $a0,0x40($t2)
8800b1e4: 0e0018eb  jal       0x880063ac
8800b1e8: ad44004c  sw        $a0,0x4c($t2)
8800b1ec: 3c1f8808  lui       $ra,0x8808
8800b1f0: 8fff7274  lw        $ra,0x7274($ra)
8800b1f4: 03e00008  jr        $ra
8800b1f8: 00000000  nop
