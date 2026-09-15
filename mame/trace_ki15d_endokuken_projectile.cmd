# Trace the first Endokuken record in the controlled two-player match.
# Run with mame/autoplay_jago_vs_idle_fulgore.lua and the dynamic recompiler.
focus maincpu

# The first secondary record is still free here. Arm its type byte before the
# scripted Endokuken is created.
gtime #24500
wp 8808be00,1,w
g

trace work/mame/traces/ki15d-endokuken-projectile.trace,maincpu,noloop
tracelog "KI_ENDOKUKEN phase=slot_cleared pc=%016X ra=%016X fp=%016X value=%02X\n",pc,ra,fp,b@8808be00

# The next type-byte write assigns the initializer state.
g
tracelog "KI_ENDOKUKEN phase=initializer pc=%016X ra=%016X fp=%016X value=%02X record=%08X:%08X:%08X:%08X\n",pc,ra,fp,b@8808be00,d@8808be00,d@8808be04,d@8808be08,d@8808be0c

# The first object update changes type 0x13 to active type 0x12 in place.
g
tracelog "KI_ENDOKUKEN phase=active pc=%016X ra=%016X fp=%016X value=%02X record=%08X:%08X:%08X:%08X\n",pc,ra,fp,b@8808be00,d@8808be00,d@8808be04,d@8808be08,d@8808be0c
trace off,maincpu
traceflush

# Keep the watchpoint armed until contact releases the same record.
g
trace work/mame/traces/ki15d-endokuken-release.trace,maincpu,noloop
tracelog "KI_ENDOKUKEN phase=released pc=%016X ra=%016X fp=%016X value=%02X record=%08X:%08X:%08X:%08X\n",pc,ra,fp,b@8808be00,d@8808be00,d@8808be04,d@8808be08,d@8808be0c

trace off,maincpu
traceflush
wpclear
quit
