# Killer Instinct v1.5d, R4600 little-endian
# Segment 0 offset 0x000063ac; 64 bytes including the return delay slot
# SHA-256: 8f0c43bc01e7ac68a1c7727610ab891957b41b11165a01ac4961a7b15d63d463

880063ac: 3c0f8806  lui       $t7,0x8806
880063b0: 001080c0  sll       $s0,$s0,3
880063b4: 25efe310  addiu     $t7,$t7,-0x1cf0
880063b8: 01f07821  addu      $t7,$t7,$s0
880063bc: 91f00007  lbu       $s0,7($t7)
880063c0: a550008a  sh        $s0,0x8a($t2)
880063c4: 8df00000  lw        $s0,0($t7)
880063c8: ad500020  sw        $s0,0x20($t2)
880063cc: 91f00004  lbu       $s0,4($t7)
880063d0: a150001c  sb        $s0,0x1c($t2)
880063d4: ad500034  sw        $s0,0x34($t2)
880063d8: 91f00006  lbu       $s0,6($t7)
880063dc: ad400018  sw        $0,0x18($t2)
880063e0: ad400014  sw        $0,0x14($t2)
880063e4: 03e00008  jr        $ra
880063e8: a550002c  sh        $s0,0x2c($t2)
