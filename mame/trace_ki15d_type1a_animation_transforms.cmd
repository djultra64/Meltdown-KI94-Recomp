# Capture one compact transform sample for every rendered frame of the first
# type-0x1a Endokuken-impact object. Run with the controlled Fulgore autoplay
# and -log; no full instruction trace or game data is committed.
focus maincpu

gtime #26300

# Preserve the secondary-pass owner because 0x88001b90 repurposes $fp.
bp 88001b90,1,{temp0=fp ; g}

# The packed-frame range covers observed tokens 0x0b through 0x13. A flag asks
# the row-loop breakpoint to record only the first destination row per call.
bp 880107a8,temp0==ffffffff8808c000&&ra==ffffffff88001e68&&v1>=ffffffff88097ea4&&v1<ffffffff880993e0,{temp1=1 ; logerror "KI_ANIM_TRANSFORM phase=object object=%016X token=%02X frame=%016X words04=%08X:%08X:%08X:%08X scale58=%04X:%04X facing8c=%02X field94=%02X\n",temp0,b@(temp0+14),v1,d@(temp0+4),d@(temp0+8),d@(temp0+c),d@(temp0+10),w@(temp0+58),w@(temp0+5a),b@(temp0+8c),b@(temp0+94) ; g}

# 0x88011874 runs after the original renderer reloads its horizontal remainder.
# $v0 is the destination origin before applying the packed row's first skip.
bp 88011874,temp1==1,{logerror "KI_ANIM_TRANSFORM phase=first_row token=%02X frame=%016X destination=%016X x_scale=%X x_remainder=%X y_accumulator=%X y_scale=%X\n",b@(temp0+14),d@(temp0+30),v0,t1,t0,t2,t3 ; temp1=0 ; g}

gtime #1300
bpclear
quit
