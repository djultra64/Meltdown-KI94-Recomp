# Identify which object owns the secondary render pass that writes the visible
# Endokuken impact. Run with autoplay_jago_vs_idle_fulgore.lua and -oslog.
#
# temp0 remembers the object pointer received by 0x88001b90. That function
# replaces $fp with a clipping bound before calling the common renderer, so the
# ordinary CPU registers no longer identify the owning object at pixel-write
# time.
focus maincpu

# The impact spans approximately 26.3-27.6 seconds in the controlled replay.
gtime #26300

# Remember the owner of every secondary pass, including class-0x11 effect
# records skipped by the ordinary sprite path.
bp 88001b90,1,{temp0=fp ; g}

# Record renderer calls whose packed frame data covers all type-0x1a tokens
# observed during the impact. The return address distinguishes this call from
# the renderer invocations made by other display paths.
bp 880107a8,ra==ffffffff88001e68&&v1>=ffffffff88097e00&&v1<ffffffff88099200,{logerror "KI_SPECIAL_RENDER phase=renderer_entry owner=%016X type=%02X token=%02X state=%02X field94=%02X frame=%016X header=%08X:%08X return=%016X\n",temp0,b@temp0,b@(temp0+14),b@(temp0+34),b@(temp0+94),v1,d@v1,d@(v1+4),ra ; g}

# Capture writes at the same core and halo pixels used by the independent
# watchpoint experiment. The instruction stores final BGR555 value $t8 at
# -2($v0); $t9 is the blend operand selected by the packed color index.
# The preceding breakpoint runs just after the original LH and preserves the
# destination color from $s5 before the saturating-add sequence overwrites it.
bp 88011938,(v0==ffffffff8003fc08)||(v0==ffffffff80067c08)||(v0==ffffffff8003e2fe)||(v0==ffffffff800662fe),{temp2=s5 ; g}
bp 8801195c,(v0==ffffffff8003fc0a)||(v0==ffffffff80067c0a)||(v0==ffffffff8003e300)||(v0==ffffffff80066300),{logerror "KI_SPECIAL_RENDER phase=target_pixel owner=%016X type=%02X token=%02X state=%02X field94=%02X address=%016X input=%04X blend=%04X output=%04X frame_cursor=%016X row_end=%016X return=%016X\n",temp0,b@temp0,b@(temp0+14),b@(temp0+34),b@(temp0+94),v0-2,temp2,t9,t8,v1,a0,ra ; g}

gtime #1300
bpclear
quit
