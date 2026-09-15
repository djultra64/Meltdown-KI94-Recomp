# Killer Instinct v1.5d, R4600 little-endian
# Segment 0 offset 0x000054d0; 28 bytes including the return delay slot
# SHA-256: 5cd20496ebe2628e84587dbeff80c51806ef942a722a2d145156f479b2946fd4

880054d0: 03c03025  move      $a2,$fp
880054d4: 24c70100  addiu     $a3,$a2,0x100
880054d8: 24c60004  addiu     $a2,$a2,4
880054dc: 14c7fffe  bne       $a2,$a3,0x880054d8
880054e0: acc0fffc  sw        $0,-4($a2)
880054e4: 03e00008  jr        $ra
880054e8: 00000000  nop
