# Killer Instinct v1.5d, R4600 little-endian
# Segment 0 offset 0x0000842c; 72 bytes including the return delay slot
# SHA-256: 86a3ddb73844ae9c9cde1c6709ed75a354d766aa4b79e81bd30a1eaecc49382b

8800842c: 87c60040  lh        $a2,0x40($fp)
88008430: 87c4003c  lh        $a0,0x3c($fp)
88008434: 8fc30010  lw        $v1,0x10($fp)
88008438: 00860018  mult      $a0,$a2
8800843c: 00031a00  sll       $v1,$v1,8
88008440: 00002012  mflo      $a0
88008444: 00641823  subu      $v1,$v1,$a0
88008448: 8fc4000c  lw        $a0,0xc($fp)
8800844c: 00660018  mult      $v1,$a2
88008450: 00033a03  sra       $a3,$v1,8
88008454: 00042200  sll       $a0,$a0,8
88008458: afc70010  sw        $a3,0x10($fp)
8800845c: 00001812  mflo      $v1
88008460: 00031a03  sra       $v1,$v1,8
88008464: 00832021  addu      $a0,$a0,$v1
88008468: 00042203  sra       $a0,$a0,8
8800846c: 03e00008  jr        $ra
88008470: afc4000c  sw        $a0,0xc($fp)
