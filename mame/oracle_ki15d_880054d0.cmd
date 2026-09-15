# Execute the original object-record clear routine with controlled RAM contents.
# MAME must run with -nodrc so debugger PC writes take effect immediately.
focus maincpu
step 2
load work/kipack/ki15d/rom-0.bin,88000000
bp 880054ec
trace work/mame/traces/oracle-ki15d-880054d0.trace,maincpu,noloop

# Each case surrounds the 0x100-byte target with distinct guard words. The
# trace records both ends of the target and both guards before and after the
# original R4600 routine runs.

# Case 1: clear a record filled with 0x5a.
fill 88090000,100,5a
do d@8808fffc = 11223344
do d@88090100 = 55667788
tracelog "KI_CLEAR_ORACLE case=1 target=88090000 before=%08X:%08X guards=%08X:%08X\n",d@88090000,d@880900fc,d@8808fffc,d@88090100
do fp = ffffffff88090000
do ra = ffffffff880054ec
do pc = ffffffff880054d0
g
tracelog "KI_CLEAR_ORACLE case=1 after=%08X:%08X guards=%08X:%08X a2=%08X a3=%08X\n",d@88090000,d@880900fc,d@8808fffc,d@88090100,a2,a3

# Case 2: repeat at a different aligned address with all bits initially set.
fill 88090200,100,ff
do d@880901fc = 89abcdef
do d@88090300 = 01234567
tracelog "KI_CLEAR_ORACLE case=2 target=88090200 before=%08X:%08X guards=%08X:%08X\n",d@88090200,d@880902fc,d@880901fc,d@88090300
do fp = ffffffff88090200
do ra = ffffffff880054ec
do pc = ffffffff880054d0
g
tracelog "KI_CLEAR_ORACLE case=2 after=%08X:%08X guards=%08X:%08X a2=%08X a3=%08X\n",d@88090200,d@880902fc,d@880901fc,d@88090300,a2,a3

# Case 3: a third fill pattern helps catch accidental value-specific behavior.
fill 88090400,100,3c
do d@880903fc = 0badf00d
do d@88090500 = c001d00d
tracelog "KI_CLEAR_ORACLE case=3 target=88090400 before=%08X:%08X guards=%08X:%08X\n",d@88090400,d@880904fc,d@880903fc,d@88090500
do fp = ffffffff88090400
do ra = ffffffff880054ec
do pc = ffffffff880054d0
g
tracelog "KI_CLEAR_ORACLE case=3 after=%08X:%08X guards=%08X:%08X a2=%08X a3=%08X\n",d@88090400,d@880904fc,d@880903fc,d@88090500,a2,a3

trace off,maincpu
traceflush
quit
