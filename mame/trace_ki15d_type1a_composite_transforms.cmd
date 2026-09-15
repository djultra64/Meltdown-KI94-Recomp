# Capture every type-0x1a object sent through KI's secondary renderer during
# one controlled Endokuken impact. Each OBJECT/FIRST_ROW pair describes one
# packed sprite. Consecutive objects targeting the same framebuffer bank belong
# to the same displayed frame and retain the arcade's original pass order.
focus maincpu

# Enter the first controlled impact before its first particle is visible.
gtime #25900

# 0x88001b90 receives the current object in $fp, then reuses $fp internally.
# Preserve the pointer in a debugger temporary for the renderer breakpoints.
bp 88001b90,1,{temp0=fp ; g}

# The call returning to 0x88001e68 is the class-0x11 secondary object path.
# Accept the entire type-0x1a animation, including the small early tokens.
bp 880107a8,b@temp0==1a&&ra==ffffffff88001e68,{temp1=1 ; logerror "KI_COMPOSITE phase=object object=%016X token=%02X frame=%016X words04=%08X:%08X:%08X:%08X scale58=%04X:%04X facing8c=%02X field94=%02X\n",temp0,b@(temp0+14),v1,d@(temp0+4),d@(temp0+8),d@(temp0+c),d@(temp0+10),w@(temp0+58),w@(temp0+5a),b@(temp0+8c),b@(temp0+94) ; g}

# At 0x88011874 the renderer has resolved placement, directions, both scales,
# and both fixed-point remainders for the first packed row.
bp 88011874,temp1==1,{logerror "KI_COMPOSITE phase=first_row object=%016X token=%02X frame=%016X source=%016X destination=%016X x_scale=%X x_remainder=%X y_accumulator=%X y_scale=%X rows_minus_one=%X\n",temp0,b@(temp0+14),d@(temp0+30),v1,v0,t1,t0,t2,t3,s0 ; temp1=0 ; g}

# Stay below the replay's three-second input interval so only one contact is
# included, while allowing the last emitted particle to finish.
gtime #2500
bpclear
quit
