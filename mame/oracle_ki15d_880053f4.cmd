# Execute the original secondary-object allocator with controlled pool states.
# MAME must run with -nodrc so debugger PC writes take effect immediately.
focus maincpu
step 2
load work/kipack/ki15d/rom-0.bin,88000000
bp 88005430
trace work/mame/traces/oracle-ki15d-880053f4.trace,maincpu,noloop

# Fill the 29 searched records plus the following fallthrough record. A 0x5a
# first byte means occupied; each case below marks its intended free record.

# Case 1: the first searchable record is free.
fill 8808be00,1e00,5a
do b@8808be00 = 0
tracelog "KI_ALLOCATOR_ORACLE case=1 occupied=0 expected=8808BE00 before=%08X:%08X\n",d@8808be00,d@8808befc
do ra = ffffffff88005430
do pc = ffffffff880053f4
g
tracelog "KI_ALLOCATOR_ORACLE case=1 result=%08X after=%08X:%08X\n",a2,d@8808be00,d@8808befc

# Case 2: skip one occupied record.
fill 8808be00,1e00,5a
do b@8808bf00 = 0
tracelog "KI_ALLOCATOR_ORACLE case=2 occupied=1 expected=8808BF00 before=%08X:%08X\n",d@8808bf00,d@8808bffc
do ra = ffffffff88005430
do pc = ffffffff880053f4
g
tracelog "KI_ALLOCATOR_ORACLE case=2 result=%08X after=%08X:%08X\n",a2,d@8808bf00,d@8808bffc

# Case 3: only the last of the 29 searchable records is free.
fill 8808be00,1e00,5a
do b@8808da00 = 0
tracelog "KI_ALLOCATOR_ORACLE case=3 occupied=28 expected=8808DA00 before=%08X:%08X\n",d@8808da00,d@8808dafc
do ra = ffffffff88005430
do pc = ffffffff880053f4
g
tracelog "KI_ALLOCATOR_ORACLE case=3 result=%08X after=%08X:%08X\n",a2,d@8808da00,d@8808dafc

# Case 4: all searched records are occupied. The original has no failure path;
# it clears and returns the immediately following 0x100-byte record.
fill 8808be00,1e00,5a
tracelog "KI_ALLOCATOR_ORACLE case=4 occupied=29 expected=8808DB00 before=%08X:%08X\n",d@8808db00,d@8808dbfc
do ra = ffffffff88005430
do pc = ffffffff880053f4
g
tracelog "KI_ALLOCATOR_ORACLE case=4 result=%08X after=%08X:%08X\n",a2,d@8808db00,d@8808dbfc

trace off,maincpu
traceflush
quit
