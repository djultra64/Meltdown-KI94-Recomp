# Reach 0x8800700c through a real Jago-vs-Riptor match rather than forcing PC.
# Run with the dynamic recompiler enabled and mame/autoplay_jago.lua loaded.
focus maincpu

# Wait for combat, then arm the byte-0xf9 timer used by the 0x88002bb4 caller.
# Changing both fixed fighter records avoids assuming the current $gp mapping.
gtime #30000
do b@8808bcf9 = 4
do b@8808bdf9 = 4

bp 8800700c
g
trace work/mame/traces/live-ki15d-8800700c.trace,maincpu,noloop
tracelog "KI_LIVE_OBJECT phase=before ra=%016X fp=%016X a2=%016X s5=%016X type=%02X src=%08X:%08X:%08X:%08X dst=%08X:%08X:%08X:%08X\n",ra,fp,a2,s5,a3,d@s5,d@(s5+4),d@(s5+8),d@(s5+c),d@a2,d@(a2+4),d@(a2+8),d@(a2+c)

bp 88002bbc
g
tracelog "KI_LIVE_OBJECT phase=after  ra=%016X fp=%016X a2=%016X s5=%016X type=%02X src=%08X:%08X:%08X:%08X dst=%08X:%08X:%08X:%08X\n",ra,fp,a2,s5,b@a2,d@s5,d@(s5+4),d@(s5+8),d@(s5+c),d@a2,d@(a2+4),d@(a2+8),d@(a2+c)

trace off,maincpu
traceflush
bpclear
quit
