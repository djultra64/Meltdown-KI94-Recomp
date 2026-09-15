# Killer Instinct v1.5d, R4600 little-endian
# Segment 0 offset 0x000053f4; 60 bytes including the return delay slot
# SHA-256: d8e8143226a28b209d201d5413a3d770b57f5210c8bc1b4a633a7b408f009eb1

880053f4: 3c068809  lui       $a2,0x8809
880053f8: 24c6be00  addiu     $a2,$a2,-0x4200
880053fc: 2407001d  li        $a3,0x1d
88005400: 90c80000  lbu       $t0,0($a2)
88005404: 11000004  beqz      $t0,0x88005418
88005408: 00000000  nop
8800540c: 24e7ffff  addiu     $a3,$a3,-1
88005410: 14e0fffb  bnez      $a3,0x88005400
88005414: 24c60100  addiu     $a2,$a2,0x100
88005418: 24c70100  addiu     $a3,$a2,0x100
8800541c: 24e7fffc  addiu     $a3,$a3,-4
88005420: 14c7fffe  bne       $a2,$a3,0x8800541c
88005424: ace00000  sw        $0,0($a3)
88005428: 03e00008  jr        $ra
8800542c: 00000000  nop
