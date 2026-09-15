# Killer Instinct v1.5d, R4600 little-endian
# Segment 0 offset 0x0000700c; 32 bytes including the return delay slot
# SHA-256: 7cd0d60b499a853c4037bb3cce0b93fc78feaef51f042d7d8b8524e9dc753833

8800700c: a0c70000  sb        $a3,0($a2)
88007010: 8ea70004  lw        $a3,4($s5)
88007014: acc70004  sw        $a3,4($a2)
88007018: 8ea70008  lw        $a3,8($s5)
8800701c: acc70008  sw        $a3,8($a2)
88007020: 8ea7000c  lw        $a3,0xc($s5)
88007024: 03e00008  jr        $ra
88007028: acc7000c  sw        $a3,0xc($a2)
