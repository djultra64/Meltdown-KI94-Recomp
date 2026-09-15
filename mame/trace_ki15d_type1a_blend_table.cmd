# Recover the live five-bit-index to BGR555-blend mapping used by one type-0x1a
# frame. Reading the operand from $t9 avoids stale debugger-side cache views.
# Run with autoplay_jago_vs_idle_fulgore.lua and -oslog.
focus maincpu

gtime #26300

# 0x88001b90 replaces $fp before entering the common renderer. Preserve the
# current object pointer and arm logging only for record 0x8808c000, token 0x0b.
bp 88001b90,1,{temp0=fp ; g}
bp 880107a8,temp0==ffffffff8808c000&&v1==ffffffff88097ea4&&ra==ffffffff88001e68,{temp1=1 ; logerror "KI_BLEND phase=frame_start owner=%016X type=%02X token=%02X frame=%016X\n",temp0,b@temp0,b@(temp0+14),v1 ; g}

# The packed packet has already advanced $v1 when this unclipped run handler is
# reached. Its previous byte contains the five-bit index and run length; $t9
# contains the live 16-bit blend operand loaded through the R4600 data cache.
bp 88011918,temp1==1,{logerror "KI_BLEND phase=run packet=%02X index=%02X length=%X blend=%04X table=%016X\n",b@(v1-1),b@(v1-1)>>3,(b@(v1-1)&7)+1,t9,t6 ; g}

# Stop after the selected frame returns so later objects cannot pollute the map.
bp 88001e68,temp1==1,{temp1=0 ; quit}
g
