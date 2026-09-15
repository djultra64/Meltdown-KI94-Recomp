# Execute the original signed planar-motion routine with controlled records.
# MAME must use -nodrc so debugger PC writes take effect immediately.
focus maincpu
step 2
load work/kipack/ki15d/rom-0.bin,88000000
bp 8800420c
trace work/mame/traces/oracle-ki15d-88004180.trace,maincpu,noloop

# Case 1: exact initial state of the first controlled type-0x1a particle.
fill 88091000,100,a5
do d@88091004 = ffff9a00
do d@88091008 = 00000000
do w@8809107c = ff00
do w@8809107e = 0000
do w@88091080 = 0000
do w@88091084 = 0180
do w@88091086 = 0000
do d@88087b20 = 11223344
do d@88087b24 = 55667788
tracelog "KI_PLANAR case=1 before=%08X:%08X halves=%04X:%04X:%04X:%04X:%04X globals=%08X:%08X\n",d@88091004,d@88091008,w@8809107c,w@8809107e,w@88091080,w@88091084,w@88091086,d@88087b20,d@88087b24
do fp = ffffffff88091000
do ra = ffffffff8800420c
do pc = ffffffff88004180
g
tracelog "KI_PLANAR case=1 after=%08X:%08X halves=%04X:%04X:%04X:%04X:%04X globals=%08X:%08X\n",d@88091004,d@88091008,w@8809107c,w@8809107e,w@88091080,w@88091084,w@88091086,d@88087b20,d@88087b24

# Case 2: a negative one-shot amount overrides the persistent amount at 0x84.
fill 88091200,100,3c
do d@88091204 = 00001000
do d@88091208 = fffff000
do w@8809127c = 0200
do w@8809127e = ff80
do w@88091280 = ff00
do w@88091284 = 0040
do w@88091286 = 0010
do d@88087b20 = aaaaaaaa
do d@88087b24 = bbbbbbbb
tracelog "KI_PLANAR case=2 before=%08X:%08X halves=%04X:%04X:%04X:%04X:%04X globals=%08X:%08X\n",d@88091204,d@88091208,w@8809127c,w@8809127e,w@88091280,w@88091284,w@88091286,d@88087b20,d@88087b24
do fp = ffffffff88091200
do ra = ffffffff8800420c
do pc = ffffffff88004180
g
tracelog "KI_PLANAR case=2 after=%08X:%08X halves=%04X:%04X:%04X:%04X:%04X globals=%08X:%08X\n",d@88091204,d@88091208,w@8809127c,w@8809127e,w@88091280,w@88091284,w@88091286,d@88087b20,d@88087b24

# Case 3: zero one-shot and persistent amounts take the early return.
fill 88091400,100,5a
do d@88091404 = 7fffffff
do d@88091408 = 80000000
do w@8809147c = 7fff
do w@8809147e = 0000
do w@88091480 = 8000
do w@88091484 = 0000
do w@88091486 = 1234
do d@88087b20 = 13579bdf
do d@88087b24 = 2468ace0
tracelog "KI_PLANAR case=3 before=%08X:%08X halves=%04X:%04X:%04X:%04X:%04X globals=%08X:%08X\n",d@88091404,d@88091408,w@8809147c,w@8809147e,w@88091480,w@88091484,w@88091486,d@88087b20,d@88087b24
do fp = ffffffff88091400
do ra = ffffffff8800420c
do pc = ffffffff88004180
g
tracelog "KI_PLANAR case=3 after=%08X:%08X halves=%04X:%04X:%04X:%04X:%04X globals=%08X:%08X\n",d@88091404,d@88091408,w@8809147c,w@8809147e,w@88091480,w@88091484,w@88091486,d@88087b20,d@88087b24

# Case 4: decay larger than the persistent amount clamps offset 0x84 to zero.
fill 88091600,100,c3
do d@88091604 = 7ffffff0
do d@88091608 = 80000010
do w@8809167c = 0100
do w@8809167e = 0000
do w@88091680 = 0100
do w@88091684 = 0020
do w@88091686 = 0040
do d@88087b20 = deadbeef
do d@88087b24 = facefeed
tracelog "KI_PLANAR case=4 before=%08X:%08X halves=%04X:%04X:%04X:%04X:%04X globals=%08X:%08X\n",d@88091604,d@88091608,w@8809167c,w@8809167e,w@88091680,w@88091684,w@88091686,d@88087b20,d@88087b24
do fp = ffffffff88091600
do ra = ffffffff8800420c
do pc = ffffffff88004180
g
tracelog "KI_PLANAR case=4 after=%08X:%08X halves=%04X:%04X:%04X:%04X:%04X globals=%08X:%08X\n",d@88091604,d@88091608,w@8809167c,w@8809167e,w@88091680,w@88091684,w@88091686,d@88087b20,d@88087b24

trace off,maincpu
traceflush
quit
