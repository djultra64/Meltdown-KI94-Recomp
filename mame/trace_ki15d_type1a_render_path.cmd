# Prove how the ordinary object-rendering pass handles one known type-0x1a record.
# This trace ends at the class filter rather than assuming that every object
# with selected frame data reaches the renderer through this particular pass.
focus maincpu

# The controlled match reaches a stable type-0x1a record at 0x8808c000 near
# this time. Enter through frame selection first, because the caller reuses
# $fp for a clipping constant before it invokes the renderer.
gtime #26500
bp 880016c4,fp==ffffffff8808c000
g

trace work/mame/traces/ki15d-type1a-render-path.trace,maincpu,noloop
tracelog "KI_RENDER phase=selection object=%016X token=%02X state=%02X\n",fp,b@(fp+14),b@(fp+34)

bpclear
bp 880018b8
g
tracelog "KI_RENDER phase=class_filter object=8808C000 type=%02X class=%02X branch_target=880019F0 frame=%08X limit=%08X\n",b@8808c000,t4,d@(8808c000+30),d@(8808c000+d0)

trace off,maincpu
traceflush
bpclear
quit
