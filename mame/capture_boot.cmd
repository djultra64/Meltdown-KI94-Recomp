# Run by MAME with -debug -debugscript.
# Unprefixed numbers are hexadecimal; #10000 is decimal.
focus maincpu
memdump work/mame/dumps/memory-map.log,maincpu
gtime #10000
save work/mame/dumps/lowram-10s.bin,80000000:maincpu,80000
save work/mame/dumps/mainram-10s.bin,88000000:maincpu,800000
save work/mame/dumps/bootrom.bin,bfc00000:maincpu,80000
quit
