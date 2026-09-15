# Killer Instinct v1.5d, R4600 little-endian
# Segment 0 offset 0x0002d5b0; 48 bytes including the return delay slot
# SHA-256: cb229c4eec53c1b933d0ac073ddcff0db5428c1e9e84214bee935ea64860c77a

8802d5b0: 000437fc  dsll      $a2,$a0,63
8802d5b4: 00041ff8  dsll      $v1,$a0,31
8802d5b8: 000637fa  dsrl      $a2,$a2,31
8802d5bc: 0003183e  dsrl      $v1,$v1,32
8802d5c0: 0004133c  dsll      $v0,$a0,44
8802d5c4: 00c33025  or        $a2,$a2,$v1
8802d5c8: 0002103e  dsrl      $v0,$v0,32
8802d5cc: 00c23026  xor       $a2,$a2,$v0
8802d5d0: 0006153a  dsrl      $v0,$a2,20
8802d5d4: 30420fff  andi      $v0,$v0,0x0fff
8802d5d8: 03e00008  jr        $ra
8802d5dc: 00461026  xor       $v0,$v0,$a2
