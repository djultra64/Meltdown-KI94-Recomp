# Record every final BGR555 write made by the six overlapping type-0x1a
# particles in one peak Endokuken-impact frame. Live register values avoid the
# R4600 cache-coherency ambiguity of debugger-side framebuffer dumps.
focus maincpu

gtime #25900
bp 88001b90,1,{temp0=fp ; g}

# Arm at the first type-0x1a renderer call of the unique six-particle frame.
bp 880107a8,temp0==ffffffff8808c000&&b@(temp0+14)==15&&b@8808be14==13&&b@8808bf14==10&&b@8808c114==0c&&b@8808c214==08&&b@8808c314==04,{temp1=1 ; temp4=1 ; logerror "KI_COMPOSITE_PIXELS phase=start\n" ; g}

# Capture each call's transform in the same run as its pixels. Later calls use
# this general entry breakpoint; temp4 restricts the row log to the first row.
bp 880107a8,temp1==1&&b@temp0==1a,{temp4=1 ; g}
bp 88011874,temp1==1&&temp4==1,{logerror "KI_COMPOSITE_PIXELS phase=transform object=%016X token=%02X frame=%016X source=%016X destination=%016X x_scale=%X x_remainder=%X y_accumulator=%X y_scale=%X rows_minus_one=%X\n",temp0,b@(temp0+14),d@(temp0+30),v1,v0,t1,t0,t2,t3,s0 ; temp4=0 ; g}

# s5 is the destination word immediately before the saturating blend; t8 is
# the exact word stored two bytes behind the post-incremented $v0 pointer.
bp 88011938,temp1==1,{temp2=s5 ; g}
bp 8801195c,temp1==1,{logerror "KI_COMPOSITE_PIXELS phase=pixel object=%016X address=%016X input=%04X blend=%04X output=%04X\n",temp0,v0-2,temp2,t9,t8 ; g}

# Record all six calls through the last object's return.
bp 88001e68,temp1==1&&temp0==ffffffff8808c300&&b@(temp0+14)==04,{logerror "KI_COMPOSITE_PIXELS phase=done\n" ; quit}
g
