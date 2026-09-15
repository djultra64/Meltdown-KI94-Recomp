# Execute the original type-0x1a particle constructor with controlled fighters.
# The constructor calls the original allocator and animation initializer, so
# both verified extracted segments are loaded before each call.
focus maincpu
step 2
load work/kipack/ki15d/rom-0.bin,88000000
load work/kipack/ki15d/rom-1.bin,88033900
bp 8800b300
trace work/mame/traces/oracle-ki15d-8800b1fc.trace,maincpu,noloop

# Case 1: exact first-particle inputs from the controlled Endokuken contact.
fill 8808be00,1e00,00
fill 88091000,100,5a
do d@88091004 = ffff9a00
do d@88091008 = 00000000
do d@8809100c = 00000000
do d@88091074 = ffffff00
do d@88091078 = 00000000
do b@880910c4 = 30
do b@880910c6 = 55
do b@880910c7 = 00
do d@88087274 = 11223344
tracelog "KI_PARTICLE_CONSTRUCTOR case=1 before fighter=%08X:%08X:%08X directions=%08X:%08X params=%02X:%02X:%02X gp=1 first=%02X global=%08X\n",d@88091004,d@88091008,d@8809100c,d@88091074,d@88091078,b@880910c4,b@880910c6,b@880910c7,b@8808be00,d@88087274
do fp = ffffffff88091000
do gp = 1
do ra = ffffffff8800b300
do pc = ffffffff8800b1fc
g
tracelog "KI_PARTICLE_CONSTRUCTOR case=1 after selected=8808BE00 fields=%02X:%02X:%08X:%08X:%08X:%04X:%04X:%04X:%04X:%04X:%04X:%02X:%08X:%08X:%02X:%08X:%08X:%08X:%02X:%04X:%04X:%08X:%08X global=%08X\n",b@8808be00,b@8808be8e,d@8808be04,d@8808be08,d@8808be0c,w@8808be7c,w@8808be80,w@8808be7e,w@8808be84,w@8808be58,w@8808be5a,b@8808be24,d@8808be3c,d@8808be10,b@8808be94,d@8808be40,d@8808be4c,d@8808be20,b@8808be1c,w@8808be2c,w@8808be8a,d@8808be14,d@8808be18,d@88087274

# Case 2: a low countdown selects computed scale; gp=0 reverses directions.
fill 8808be00,1e00,00
fill 8808be00,100,ff
fill 88091200,100,3c
do d@88091204 = 12345678
do d@88091208 = 89abcdef
do d@8809120c = ffffff00
do d@88091274 = 12345678
do d@88091278 = ffff0100
do b@880912c4 = 10
do b@880912c6 = 20
do b@880912c7 = e5
do d@88087274 = aaaaaaaa
tracelog "KI_PARTICLE_CONSTRUCTOR case=2 before fighter=%08X:%08X:%08X directions=%08X:%08X params=%02X:%02X:%02X gp=0 first=%02X global=%08X\n",d@88091204,d@88091208,d@8809120c,d@88091274,d@88091278,b@880912c4,b@880912c6,b@880912c7,b@8808be00,d@88087274
do fp = ffffffff88091200
do gp = 0
do ra = ffffffff8800b300
do pc = ffffffff8800b1fc
g
tracelog "KI_PARTICLE_CONSTRUCTOR case=2 after selected=8808BF00 fields=%02X:%02X:%08X:%08X:%08X:%04X:%04X:%04X:%04X:%04X:%04X:%02X:%08X:%08X:%02X:%08X:%08X:%08X:%02X:%04X:%04X:%08X:%08X global=%08X\n",b@8808bf00,b@8808bf8e,d@8808bf04,d@8808bf08,d@8808bf0c,w@8808bf7c,w@8808bf80,w@8808bf7e,w@8808bf84,w@8808bf58,w@8808bf5a,b@8808bf24,d@8808bf3c,d@8808bf10,b@8808bf94,d@8808bf40,d@8808bf4c,d@8808bf20,b@8808bf1c,w@8808bf2c,w@8808bf8a,d@8808bf14,d@8808bf18,d@88087274

# Case 3: zero countdown and maximum byte offsets expose wraparound behavior.
fill 8808be00,1e00,00
fill 88091400,100,5a
do d@88091404 = 7ffffff0
do d@88091408 = 80000010
do d@8809140c = ffffff80
do d@88091474 = 00008001
do d@88091478 = ffff7fff
do b@880914c4 = 00
do b@880914c6 = ff
do b@880914c7 = ff
do d@88087274 = bbbbbbbb
tracelog "KI_PARTICLE_CONSTRUCTOR case=3 before fighter=%08X:%08X:%08X directions=%08X:%08X params=%02X:%02X:%02X gp=1 first=%02X global=%08X\n",d@88091404,d@88091408,d@8809140c,d@88091474,d@88091478,b@880914c4,b@880914c6,b@880914c7,b@8808be00,d@88087274
do fp = ffffffff88091400
do gp = 1
do ra = ffffffff8800b300
do pc = ffffffff8800b1fc
g
tracelog "KI_PARTICLE_CONSTRUCTOR case=3 after selected=8808BE00 fields=%02X:%02X:%08X:%08X:%08X:%04X:%04X:%04X:%04X:%04X:%04X:%02X:%08X:%08X:%02X:%08X:%08X:%08X:%02X:%04X:%04X:%08X:%08X global=%08X\n",b@8808be00,b@8808be8e,d@8808be04,d@8808be08,d@8808be0c,w@8808be7c,w@8808be80,w@8808be7e,w@8808be84,w@8808be58,w@8808be5a,b@8808be24,d@8808be3c,d@8808be10,b@8808be94,d@8808be40,d@8808be4c,d@8808be20,b@8808be1c,w@8808be2c,w@8808be8a,d@8808be14,d@8808be18,d@88087274

# Case 4: countdown 0x1a is the exact boundary for fixed scale 0x1000.
fill 8808be00,1e00,00
fill 88091600,100,c3
do d@88091604 = fffffff0
do d@88091608 = 00000010
do d@8809160c = 7fffff80
do d@88091674 = 00000100
do d@88091678 = fffffe00
do b@880916c4 = 1a
do b@880916c6 = 01
do b@880916c7 = 60
do d@88087274 = cccccccc
tracelog "KI_PARTICLE_CONSTRUCTOR case=4 before fighter=%08X:%08X:%08X directions=%08X:%08X params=%02X:%02X:%02X gp=0 first=%02X global=%08X\n",d@88091604,d@88091608,d@8809160c,d@88091674,d@88091678,b@880916c4,b@880916c6,b@880916c7,b@8808be00,d@88087274
do fp = ffffffff88091600
do gp = 0
do ra = ffffffff8800b300
do pc = ffffffff8800b1fc
g
tracelog "KI_PARTICLE_CONSTRUCTOR case=4 after selected=8808BE00 fields=%02X:%02X:%08X:%08X:%08X:%04X:%04X:%04X:%04X:%04X:%04X:%02X:%08X:%08X:%02X:%08X:%08X:%08X:%02X:%04X:%04X:%08X:%08X global=%08X\n",b@8808be00,b@8808be8e,d@8808be04,d@8808be08,d@8808be0c,w@8808be7c,w@8808be80,w@8808be7e,w@8808be84,w@8808be58,w@8808be5a,b@8808be24,d@8808be3c,d@8808be10,b@8808be94,d@8808be40,d@8808be4c,d@8808be20,b@8808be1c,w@8808be2c,w@8808be8a,d@8808be14,d@8808be18,d@88087274

trace off,maincpu
traceflush
quit
