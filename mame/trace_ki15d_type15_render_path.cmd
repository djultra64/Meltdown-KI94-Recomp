# Prove how the ordinary object-rendering pass handles the type-0x15
# Endokuken-contact object. Like type 0x1a, it selects frame data but is routed
# away from this pass as class 0x11. A separate pass may still draw it.
focus maincpu

# The first controlled Endokuken activates the contact record at 0x8808bf00
# shortly after this point. Waiting for its frame-selection call keeps every
# later breakpoint on the same object's uninterrupted rendering path.
gtime #25500
bp 880016c4,fp==ffffffff8808bf00
g

trace work/mame/traces/ki15d-type15-render-path.trace,maincpu,noloop
tracelog "KI_RENDER15 phase=selection object=%016X type=%02X token=%02X state=%02X\n",fp,b@fp,b@(fp+14),b@(fp+34)

# Confirm the class value used by the main pass and the branch that skips the
# ordinary sprite renderer.
bpclear
bp 880018b8
g
tracelog "KI_RENDER15 phase=class_filter type=%02X class=%02X frame=%08X limit=%08X\n",b@8808bf00,t4,d@(8808bf00+30),d@(8808bf00+d0)

trace off,maincpu
traceflush
bpclear
quit
