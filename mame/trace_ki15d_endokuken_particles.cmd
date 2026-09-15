# Trace the first receiver-side type-0x1a record after Endokuken contact.
# Run with mame/autoplay_jago_vs_idle_fulgore.lua and the dynamic recompiler.
focus maincpu

gtime #26000
bp 8800b1fc
g

trace work/mame/traces/ki15d-endokuken-particles.trace,maincpu,noloop
tracelog "KI_PARTICLE phase=constructor_entry ra=%016X fp=%016X fighter=%08X:%08X:%08X:%08X counter_c4=%02X height_c6=%02X variant_c7=%02X\n",ra,fp,d@fp,d@(fp+4),d@(fp+8),d@(fp+c),b@(fp+c4),b@(fp+c6),b@(fp+c7)

# This point follows type assignment and the constructor's motion setup.
bp 8800b1d8
g
tracelog "KI_PARTICLE phase=configured record_address=%016X type=%02X position=%08X:%08X:%08X velocity=%08X state24=%02X animation_index=%02X\n",t2,b@t2,d@(t2+4),d@(t2+8),d@(t2+c),d@(t2+10),b@(t2+24),s0

# The shared setup routine assigns the packed script and display flags.
bpclear
bp 8800b1ec
g
tracelog "KI_PARTICLE phase=ready record_address=%016X type=%02X script=%08X flags40=%08X flags4c=%08X\n",t2,b@t2,d@(t2+20),d@(t2+40),d@(t2+4c)

trace off,maincpu
traceflush
bpclear
quit
