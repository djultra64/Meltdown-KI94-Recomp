# KI v1.5d, R4600LE, extracted segment 0 offset 0x3d30; 68 bytes.
# SHA-256: 8a29bbefe79e91ef120cacf7ad2dcda75d48c412025215b755bd6249331c853e
# This is a block inside the fighter updater, not a standalone jr-ra routine.
88003d30: 93c600c4  lbu       $a2,0xc4($fp)
88003d34: 10c0000d  beqz      $a2,0x88003d6c
88003d38: 00000000  nop
88003d3c: 93c700c5  lbu       $a3,0xc5($fp)
88003d40: 24c6ffff  addiu     $a2,$a2,-1
88003d44: a3c600c4  sb        $a2,0xc4($fp)
88003d48: 24e70010  addiu     $a3,$a3,0x10
88003d4c: a3c700c5  sb        $a3,0xc5($fp)
88003d50: 30e8000f  andi      $t0,$a3,0x000f
88003d54: 00073902  srl       $a3,$a3,4
88003d58: 00e8082a  slt       $at,$a3,$t0
88003d5c: 14200003  bnez      $at,0x88003d6c
88003d60: 00000000  nop
88003d64: 0e002c7f  jal       0x8800b1fc
88003d68: a3c800c5  sb        $t0,0xc5($fp)
88003d6c: 0a000822  j         0x88002088
88003d70: 00000000  nop
