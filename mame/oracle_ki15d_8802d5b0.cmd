# Execute one original routine as a controlled oracle.
# MAME must run with -nodrc so debugger PC writes take effect immediately.
focus maincpu
step 2
load work/kipack/ki15d/rom-0.bin,88000000
bp 8802d5e0
trace work/mame/traces/oracle-ki15d-8802d5b0.trace,maincpu,noloop

do a0 = 0
do ra = ffffffff8802d5e0
do pc = ffffffff8802d5b0
g
tracelog "KI_ORACLE input=%016X output=%016X\n",a0,v0

do a0 = 1
do ra = ffffffff8802d5e0
do pc = ffffffff8802d5b0
g
tracelog "KI_ORACLE input=%016X output=%016X\n",a0,v0

do a0 = 100001000
do ra = ffffffff8802d5e0
do pc = ffffffff8802d5b0
g
tracelog "KI_ORACLE input=%016X output=%016X\n",a0,v0

do a0 = 12345678
do ra = ffffffff8802d5e0
do pc = ffffffff8802d5b0
g
tracelog "KI_ORACLE input=%016X output=%016X\n",a0,v0

do a0 = 80000000
do ra = ffffffff8802d5e0
do pc = ffffffff8802d5b0
g
tracelog "KI_ORACLE input=%016X output=%016X\n",a0,v0

do a0 = ffffffff
do ra = ffffffff8802d5e0
do pc = ffffffff8802d5b0
g
tracelog "KI_ORACLE input=%016X output=%016X\n",a0,v0

do a0 = 1ffffffff
do ra = ffffffff8802d5e0
do pc = ffffffff8802d5b0
g
tracelog "KI_ORACLE input=%016X output=%016X\n",a0,v0

trace off,maincpu
traceflush
quit
