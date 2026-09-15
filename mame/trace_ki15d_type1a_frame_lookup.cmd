# Follow one type-0x1a animation token into its selected frame-data pointer.
# Run with mame/autoplay_jago_vs_idle_fulgore.lua and the dynamic recompiler.
focus maincpu

gtime #26500
bp 880016c4,fp==ffffffff8808c000
g

trace work/mame/traces/ki15d-type1a-frame-lookup.trace,maincpu,noloop
tracelog "KI_FRAME_LOOKUP phase=entry fp=%016X type=%02X token=%02X state34=%02X type_class=%02X pointer_slot=%08X backing_value=%08X script=%08X timer=%08X\n",fp,b@fp,b@(fp+14),b@(fp+34),b@(880341f0+b@fp),880900fc+(b@fp*4),d@(880900fc+(b@fp*4)),d@(fp+20),d@(fp+18)

# The R4600 load sees a dirty cached value in the pointer slot. Logging $v1
# after the load is therefore essential; a direct debugger read can be stale.
bpclear
bp 88001768
g
tracelog "KI_FRAME_LOOKUP row=0 address=%016X key=%02X relative_next=%08X\n",v1,a0,t6
g
tracelog "KI_FRAME_LOOKUP row=1 address=%016X key=%02X relative_next=%08X\n",v1,a0,t6
g
tracelog "KI_FRAME_LOOKUP row=2 address=%016X key=%02X relative_next=%08X\n",v1,a0,t6
g
tracelog "KI_FRAME_LOOKUP row=3 address=%016X key=%02X relative_next=%08X\n",v1,a0,t6

# At the matching state row, the token selects offsets into the frame data.
bpclear
bp 880017c0
g
tracelog "KI_FRAME_LOOKUP phase=frame_entry row=%016X token=%02X frame_count=%02X selected_offset=%08X next_offset=%08X\n",v1,k0,s6,s2,t6

bpclear
# Stop at the caller after the return-delay-slot write to offset 0xd0.
bp 88001898
g
tracelog "KI_FRAME_LOOKUP phase=selected token=%02X frame_pointer=%08X limit_pointer=%08X frame_head=%08X:%08X:%08X:%08X\n",b@(fp+14),d@(fp+30),d@(fp+d0),d@(d@(fp+30)),d@(d@(fp+30)+4),d@(d@(fp+30)+8),d@(d@(fp+30)+c)

trace off,maincpu
traceflush
bpclear
quit
