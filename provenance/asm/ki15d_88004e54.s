# Killer Instinct v1.5d, R4600 little-endian
# Segment 0 offset 0x00004e54; 56 bytes through both dispatch exits
# SHA-256: c4498c07620efaf50cfbee1bcab095ea540d173506b7eaf96fa13f2cdd327605

88004e54: 0e001060  jal       0x88004180
88004e58: 00000000  nop
88004e5c: 0e00210b  jal       0x8800842c
88004e60: 00000000  nop
88004e64: 0e00199c  jal       0x88006670
88004e68: 00000000  nop
88004e6c: 97c4005a  lhu       $a0,0x5a($fp)
88004e70: 24840080  addiu     $a0,$a0,0x80
88004e74: a7c4005a  sh        $a0,0x5a($fp)
88004e78: 8fc40020  lw        $a0,0x20($fp)
88004e7c: 1080fdcb  beqz      $a0,0x880045ac
88004e80: 00000000  nop
88004e84: 0a000822  j         0x88002088
88004e88: 00000000  nop

# Shared release branch reached when the animation pointer becomes zero.
880045ac: 0e001534  jal       0x880054d0
880045b0: 00000000  nop
880045b4: 0a000822  j         0x88002088
880045b8: 00000000  nop
