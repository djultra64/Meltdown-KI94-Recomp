# KI v1.5d R4600LE; segment 0 offset 0xa2e4, 152 bytes.
# SHA-256: 8d10efb5394ef73b8ad2e8e909097d8162086196e2ac520a9e25638c321a1555
# Internal block, stops before unrelated continuation 0x8800a37c.
8800a2e4: 91ae001d  lbu       $t6,0x1d($t5)
8800a2e8: 11c00024  beqz      $t6,0x8800a37c
8800a2ec: 00000000  nop
8800a2f0: 31ce000f  andi      $t6,$t6,0x000f
8800a2f4: 3c0f8803  lui       $t7,0x8803
8800a2f8: 000e70c0  sll       $t6,$t6,3
8800a2fc: 25ef4610  addiu     $t7,$t7,0x4610
8800a300: 01cf7821  addu      $t7,$t6,$t7
8800a304: 91ee0000  lbu       $t6,0($t7)
8800a308: a3ce00c4  sb        $t6,0xc4($fp)
8800a30c: 91ee0001  lbu       $t6,1($t7)
8800a310: a3ce00c5  sb        $t6,0xc5($fp)
8800a314: 91ae0013  lbu       $t6,0x13($t5)
8800a318: 91a6001a  lbu       $a2,0x1a($t5)
8800a31c: 31ce0040  andi      $t6,$t6,0x0040
8800a320: 15c00008  bnez      $t6,0x8800a344
8800a324: 00000000  nop
8800a328: 8d86000c  lw        $a2,0xc($t4)
8800a32c: 8fc8000c  lw        $t0,0xc($fp)
8800a330: 00c83023  subu      $a2,$a2,$t0
8800a334: 04c10002  bgez      $a2,0x8800a340
8800a338: 00000000  nop
8800a33c: 00003025  move      $a2,$0
8800a340: 00063202  srl       $a2,$a2,8
8800a344: a3c600c6  sb        $a2,0xc6($fp)
8800a348: 91ae001d  lbu       $t6,0x1d($t5)
8800a34c: 9186008e  lbu       $a2,0x8e($t4)
8800a350: 000e7102  srl       $t6,$t6,4
8800a354: 11c00005  beqz      $t6,0x8800a36c
8800a358: 00000000  nop
8800a35c: 3c068803  lui       $a2,0x8803
8800a360: 24c64647  addiu     $a2,$a2,0x4647
8800a364: 00ce3021  addu      $a2,$a2,$t6
8800a368: 90c60000  lbu       $a2,0($a2)
8800a36c: 91ee0002  lbu       $t6,2($t7)
8800a370: 000e7140  sll       $t6,$t6,5
8800a374: 00ce3021  addu      $a2,$a2,$t6
8800a378: a3c600c7  sb        $a2,0xc7($fp)
