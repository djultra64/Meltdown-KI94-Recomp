# Execute the original animation-setup routine with controlled object records.
# MAME must run with -nodrc so debugger PC writes take effect immediately.
focus maincpu
step 2
load work/kipack/ki15d/rom-0.bin,88000000
load work/kipack/ki15d/rom-1.bin,88033900
bp 880063ec
trace work/mame/traces/oracle-ki15d-880063ac.trace,maincpu,noloop

# Case 1: table index 0.
fill 88091000,100,5a
do d@88090ffc = 11223344
do d@88091100 = 55667788
tracelog "KI_ANIMATION_SETUP case=1 index=0 table=%08X:%08X before=%08X:%08X:%08X:%08X:%08X:%08X:%08X guards=%08X:%08X\n",d@8805e310,d@8805e314,d@88091014,d@88091018,d@8809101c,d@88091020,d@8809102c,d@88091034,d@88091088,d@88090ffc,d@88091100
do s0 = 0
do t2 = ffffffff88091000
do ra = ffffffff880063ec
do pc = ffffffff880063ac
g
tracelog "KI_ANIMATION_SETUP case=1 after=%08X:%08X:%08X:%08X:%08X:%08X:%08X guards=%08X:%08X\n",d@88091014,d@88091018,d@8809101c,d@88091020,d@8809102c,d@88091034,d@88091088,d@88090ffc,d@88091100

# Case 2: table index 3 uses different script and state values.
fill 88091200,100,ff
do d@880911fc = 89abcdef
do d@88091300 = 01234567
tracelog "KI_ANIMATION_SETUP case=2 index=3 table=%08X:%08X before=%08X:%08X:%08X:%08X:%08X:%08X:%08X guards=%08X:%08X\n",d@8805e328,d@8805e32c,d@88091214,d@88091218,d@8809121c,d@88091220,d@8809122c,d@88091234,d@88091288,d@880911fc,d@88091300
do s0 = 3
do t2 = ffffffff88091200
do ra = ffffffff880063ec
do pc = ffffffff880063ac
g
tracelog "KI_ANIMATION_SETUP case=2 after=%08X:%08X:%08X:%08X:%08X:%08X:%08X guards=%08X:%08X\n",d@88091214,d@88091218,d@8809121c,d@88091220,d@8809122c,d@88091234,d@88091288,d@880911fc,d@88091300

# Case 3: index 12 is the exact entry used by the first traced hit particle.
fill 88091400,100,3c
do d@880913fc = 0badf00d
do d@88091500 = c001d00d
tracelog "KI_ANIMATION_SETUP case=3 index=12 table=%08X:%08X before=%08X:%08X:%08X:%08X:%08X:%08X:%08X guards=%08X:%08X\n",d@8805e370,d@8805e374,d@88091414,d@88091418,d@8809141c,d@88091420,d@8809142c,d@88091434,d@88091488,d@880913fc,d@88091500
do s0 = c
do t2 = ffffffff88091400
do ra = ffffffff880063ec
do pc = ffffffff880063ac
g
tracelog "KI_ANIMATION_SETUP case=3 after=%08X:%08X:%08X:%08X:%08X:%08X:%08X guards=%08X:%08X\n",d@88091414,d@88091418,d@8809141c,d@88091420,d@8809142c,d@88091434,d@88091488,d@880913fc,d@88091500

# Case 4: a synthetic table row proves that bytes 6 and 7 are independent.
do d@8805e3b0 = 88776655
do d@8805e3b4 = 9a563412
fill 88091600,100,c3
do d@880915fc = deadbeef
do d@88091700 = facefeed
tracelog "KI_ANIMATION_SETUP case=4 index=20 table=%08X:%08X before=%08X:%08X:%08X:%08X:%08X:%08X:%08X guards=%08X:%08X\n",d@8805e3b0,d@8805e3b4,d@88091614,d@88091618,d@8809161c,d@88091620,d@8809162c,d@88091634,d@88091688,d@880915fc,d@88091700
do s0 = 14
do t2 = ffffffff88091600
do ra = ffffffff880063ec
do pc = ffffffff880063ac
g
tracelog "KI_ANIMATION_SETUP case=4 after=%08X:%08X:%08X:%08X:%08X:%08X:%08X guards=%08X:%08X\n",d@88091614,d@88091618,d@8809161c,d@88091620,d@8809162c,d@88091634,d@88091688,d@880915fc,d@88091700

trace off,maincpu
traceflush
quit
