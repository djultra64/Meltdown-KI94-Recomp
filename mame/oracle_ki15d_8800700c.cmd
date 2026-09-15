# Execute one original RAM-writing leaf routine as a controlled oracle.
# MAME must run with -nodrc so debugger PC writes take effect immediately.
focus maincpu
step 2
load work/kipack/ki15d/rom-0.bin,88000000
bp 8800702c
trace work/mame/traces/oracle-ki15d-8800700c.trace,maincpu,noloop

# $s5 is the source, $a2 the destination, and $a3 supplies byte zero.
# Preserve the input byte in $s4 because the routine reuses $a3 for its loads.
do d@88090000 = 0badf00d
do d@88090004 = 01234567
do d@88090008 = 89abcdef
do d@8809000c = 13579bdf
do d@88090100 = a5a5a5a5
do d@88090104 = b6b6b6b6
do d@88090108 = c7c7c7c7
do d@8809010c = d8d8d8d8
do s4 = ab
do s5 = ffffffff88090000
do a2 = ffffffff88090100
do a3 = s4
tracelog "KI_MEMORY_ORACLE case=1 phase=before first=%02X src=%08X:%08X:%08X:%08X dst=%08X:%08X:%08X:%08X\n",s4,d@88090000,d@88090004,d@88090008,d@8809000c,d@88090100,d@88090104,d@88090108,d@8809010c
do ra = ffffffff8800702c
do pc = ffffffff8800700c
g
tracelog "KI_MEMORY_ORACLE case=1 phase=after first=%02X src=%08X:%08X:%08X:%08X dst=%08X:%08X:%08X:%08X\n",s4,d@88090000,d@88090004,d@88090008,d@8809000c,d@88090100,d@88090104,d@88090108,d@8809010c

do d@88090000 = deadbeef
do d@88090004 = 00000000
do d@88090008 = ffffffff
do d@8809000c = 80000001
do d@88090100 = 11223344
do d@88090104 = 55667788
do d@88090108 = 99aabbcc
do d@8809010c = ddeeff00
do s4 = 0
do a3 = s4
tracelog "KI_MEMORY_ORACLE case=2 phase=before first=%02X src=%08X:%08X:%08X:%08X dst=%08X:%08X:%08X:%08X\n",s4,d@88090000,d@88090004,d@88090008,d@8809000c,d@88090100,d@88090104,d@88090108,d@8809010c
do ra = ffffffff8800702c
do pc = ffffffff8800700c
g
tracelog "KI_MEMORY_ORACLE case=2 phase=after first=%02X src=%08X:%08X:%08X:%08X dst=%08X:%08X:%08X:%08X\n",s4,d@88090000,d@88090004,d@88090008,d@8809000c,d@88090100,d@88090104,d@88090108,d@8809010c

do d@88090000 = cafebabe
do d@88090004 = 10203040
do d@88090008 = 50607080
do d@8809000c = 90a0b0c0
do d@88090100 = 01020304
do d@88090104 = 11121314
do d@88090108 = 21222324
do d@8809010c = 31323334
do s4 = ff
do a3 = s4
tracelog "KI_MEMORY_ORACLE case=3 phase=before first=%02X src=%08X:%08X:%08X:%08X dst=%08X:%08X:%08X:%08X\n",s4,d@88090000,d@88090004,d@88090008,d@8809000c,d@88090100,d@88090104,d@88090108,d@8809010c
do ra = ffffffff8800702c
do pc = ffffffff8800700c
g
tracelog "KI_MEMORY_ORACLE case=3 phase=after first=%02X src=%08X:%08X:%08X:%08X dst=%08X:%08X:%08X:%08X\n",s4,d@88090000,d@88090004,d@88090008,d@8809000c,d@88090100,d@88090104,d@88090108,d@8809010c

trace off,maincpu
traceflush
quit
