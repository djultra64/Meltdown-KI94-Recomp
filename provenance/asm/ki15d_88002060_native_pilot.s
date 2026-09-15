# KI v1.5d R4600LE; finite native-pilot dispatcher enrollment.
# Segment 0 offset 0x2060, 40 bytes; SHA-256:
# 9489d8075e042a680e3c3efa76216e1fdb056697440f0bb5088b9ed332fbc569
88002060: 93c60000  lbu       $a2,0($fp)
88002064: 10c00058  beqz      $a2,0x880021c8
88002068: 00000000  nop
8800206c: 3c078803  lui       $a3,0x8803
88002070: 00063080  sll       $a2,$a2,2
88002074: 24e7416c  addiu     $a3,$a3,0x416c
88002078: 00c73021  addu      $a2,$a2,$a3
8800207c: 8cc60000  lw        $a2,0($a2)
88002080: 00c00008  jr        $a2
88002084: 00000000  nop
