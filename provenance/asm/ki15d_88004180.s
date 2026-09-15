# Killer Instinct v1.5d, R4600 little-endian
# Segment 0 offset 0x00004180; 140 bytes including the return delay slot
# SHA-256: 497d6c35d14bd4f7161bd382b90f4e944a4796fa01a1f450e0df8ea47beeac33

88004180: 0a001063  j         0x8800418c
88004184: 87d6007e  lh        $s6,0x7e($fp)
88004188: 97d6007e  lhu       $s6,0x7e($fp)  # alternate unsigned entry
8800418c: 16c00004  bnez      $s6,0x880041a0
88004190: 00000000  nop
88004194: 97d60084  lhu       $s6,0x84($fp)
88004198: 12c0001a  beqz      $s6,0x88004204
8800419c: 00000000  nop
880041a0: 87d7007c  lh        $s7,0x7c($fp)
880041a4: a7c0007e  sh        $0,0x7e($fp)
880041a8: 3c018808  lui       $at,0x8808
880041ac: 02d70018  mult      $s6,$s7
880041b0: 8fd70004  lw        $s7,4($fp)
880041b4: 0000c812  mflo      $t9
880041b8: 0019ca03  sra       $t9,$t9,8
880041bc: ac397b20  sw        $t9,0x7b20($at)
880041c0: 0337b821  addu      $s7,$t9,$s7
880041c4: afd70004  sw        $s7,4($fp)
880041c8: 87d70080  lh        $s7,0x80($fp)
880041cc: 02d70018  mult      $s6,$s7
880041d0: 8fd70008  lw        $s7,8($fp)
880041d4: 0000c812  mflo      $t9
880041d8: 0019ca03  sra       $t9,$t9,8
880041dc: ac397b24  sw        $t9,0x7b24($at)
880041e0: 0337b821  addu      $s7,$t9,$s7
880041e4: afd70008  sw        $s7,8($fp)
880041e8: 97d70086  lhu       $s7,0x86($fp)
880041ec: 97d60084  lhu       $s6,0x84($fp)
880041f0: 02d7b023  subu      $s6,$s6,$s7
880041f4: 1ec00002  bgtz      $s6,0x88004200
880041f8: 00000000  nop
880041fc: 0000b025  move      $s6,$0
88004200: a7d60084  sh        $s6,0x84($fp)
88004204: 03e00008  jr        $ra
88004208: 00000000  nop
