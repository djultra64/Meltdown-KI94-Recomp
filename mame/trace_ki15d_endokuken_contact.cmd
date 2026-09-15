# Trace the central contact record created by the first controlled Endokuken.
# Run with mame/autoplay_jago_vs_idle_fulgore.lua and the dynamic recompiler.
focus maincpu

# The projectile occupies 0x8808be00, so the contact initializer selects the
# next secondary record. Arm its type byte shortly before collision.
gtime #25500
wp 8808bf00,1,w
g

trace work/mame/traces/ki15d-endokuken-contact.trace,maincpu,noloop
tracelog "KI_CONTACT phase=slot_cleared pc=%016X ra=%016X fp=%016X value=%02X\n",pc,ra,fp,b@8808bf00

# Capture the initializer type and the position copied from the projectile.
g
tracelog "KI_CONTACT phase=initializer pc=%016X ra=%016X fp=%016X value=%02X record=%08X:%08X:%08X:%08X\n",pc,ra,fp,b@8808bf00,d@8808bf00,d@8808bf04,d@8808bf08,d@8808bf0c

# Its next update changes type 0x14 to active type 0x15 in place.
g
tracelog "KI_CONTACT phase=active pc=%016X ra=%016X fp=%016X value=%02X record=%08X:%08X:%08X:%08X\n",pc,ra,fp,b@8808bf00,d@8808bf00,d@8808bf04,d@8808bf08,d@8808bf0c

trace off,maincpu
traceflush
wpclear
quit
