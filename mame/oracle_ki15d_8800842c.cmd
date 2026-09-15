# Execute the original scaled vertical-motion routine with controlled records.
# MAME must use -nodrc so debugger PC writes take effect immediately.
focus maincpu
step 2
load work/kipack/ki15d/rom-0.bin,88000000
bp 88008474
trace work/mame/traces/oracle-ki15d-8800842c.trace,maincpu,noloop

# Case 1: exact initial gravity state of the first type-0x1a particle.
fill 88091000,100,a5
do d@8809100c = 00005500
do d@88091010 = 000000a0
do w@8809103c = fff6
do w@88091040 = 0100
tracelog "KI_VERTICAL case=1 before=%08X:%08X controls=%04X:%04X\n",d@8809100c,d@88091010,w@8809103c,w@88091040
do fp = ffffffff88091000
do ra = ffffffff88008474
do pc = ffffffff8800842c
g
tracelog "KI_VERTICAL case=1 after=%08X:%08X controls=%04X:%04X\n",d@8809100c,d@88091010,w@8809103c,w@88091040

# Case 2: half-rate time scale exercises both signed multiply paths.
fill 88091200,100,3c
do d@8809120c = fffff000
do d@88091210 = ffffff80
do w@8809123c = 0018
do w@88091240 = 0080
tracelog "KI_VERTICAL case=2 before=%08X:%08X controls=%04X:%04X\n",d@8809120c,d@88091210,w@8809123c,w@88091240
do fp = ffffffff88091200
do ra = ffffffff88008474
do pc = ffffffff8800842c
g
tracelog "KI_VERTICAL case=2 after=%08X:%08X controls=%04X:%04X\n",d@8809120c,d@88091210,w@8809123c,w@88091240

# Case 3: zero time scale keeps position while still replacing velocity.
fill 88091400,100,5a
do d@8809140c = 7fffffff
do d@88091410 = 80000000
do w@8809143c = 7fff
do w@88091440 = 0000
tracelog "KI_VERTICAL case=3 before=%08X:%08X controls=%04X:%04X\n",d@8809140c,d@88091410,w@8809143c,w@88091440
do fp = ffffffff88091400
do ra = ffffffff88008474
do pc = ffffffff8800842c
g
tracelog "KI_VERTICAL case=3 after=%08X:%08X controls=%04X:%04X\n",d@8809140c,d@88091410,w@8809143c,w@88091440

# Case 4: large signed values expose 32-bit shift and low-product wrapping.
fill 88091600,100,c3
do d@8809160c = 70000001
do d@88091610 = 7f123456
do w@8809163c = 8123
do w@88091640 = 7f01
tracelog "KI_VERTICAL case=4 before=%08X:%08X controls=%04X:%04X\n",d@8809160c,d@88091610,w@8809163c,w@88091640
do fp = ffffffff88091600
do ra = ffffffff88008474
do pc = ffffffff8800842c
g
tracelog "KI_VERTICAL case=4 after=%08X:%08X controls=%04X:%04X\n",d@8809160c,d@88091610,w@8809163c,w@88091640

trace off,maincpu
traceflush
quit
