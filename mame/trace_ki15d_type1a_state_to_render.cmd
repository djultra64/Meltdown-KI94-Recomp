# Trace every state-to-render conversion for type-0x1a particles during one
# controlled Endokuken impact. Run with autoplay_jago_vs_idle_fulgore.lua and
# -oslog. The compact analyzer derives its fixture from the resulting log.
focus maincpu

# Enter before the first particle is visible. temp9 is a monotonically
# increasing render-call identity; temp0 retains the object pointer because
# 0x88001b90 later replaces $fp with the viewport height.
gtime #25900
temp9=0
bp 88001b90,1,{temp0=fp ; temp1=0 ; temp8=t8 ; g}

# The palette helper computes its selected source in $a0 before writing the
# cacheable global at 0x880359c8. Retain the executing register so the trace
# does not depend on whether debugger-side RAM reflects dirty R4600 cache data.
bp 8800200c,b@temp0==1a,{temp4=a0 ; g}

# 0x88001ffc has just returned after selecting/copying the palette bank. Log
# every record/global input read by the type-0x1a path. Register $v1 at the
# renderer entry below is authoritative for the frame pointer when cache state
# makes a debugger-side record read stale.
bp 88001bcc,b@temp0==1a,{temp1=1 ; temp2=0 ; temp3=0 ; logerror "KI_S2R phase=input call=%X object=%016X type=%02X token=%02X pos04=%08X:%08X:%08X vel10=%08X byte24=%02X frame30=%08X state34=%02X scale58=%04X:%04X word68=%08X word6c=%08X word70=%08X facing8c=%02X palette8e=%02X display94=%02X flags95=%02X aux97=%02X clipc8=%04X widthca=%04X fbselect=%02X viewport=%08X palette_source=%08X\n",temp9,temp0,b@temp0,b@(temp0+14),d@(temp0+4),d@(temp0+8),d@(temp0+c),d@(temp0+10),b@(temp0+24),d@(temp0+30),b@(temp0+34),w@(temp0+58),w@(temp0+5a),d@(temp0+68),d@(temp0+6c),d@(temp0+70),b@(temp0+8c),b@(temp0+8e),b@(temp0+94),b@(temp0+95),b@(temp0+97),w@(temp0+c8),w@(temp0+ca),b@88086200,temp8,temp4 ; g}

# This return address selects the final type-0x1a call from the secondary pass.
bp 880107a8,temp1==1&&ra==ffffffff88001e68,{temp2=1 ; temp3=1 ; temp5=0 ; temp6=0 ; temp7=v1 ; logerror "KI_S2R phase=renderer call=%X object=%016X frame=%016X frame_header=%04X:%04X:%04X:%04X a0=%016X gp=%016X t1=%016X t3=%016X t4=%016X s5=%016X s6=%016X s7=%016X t8=%016X k0=%016X k1=%016X fp=%016X s2=%016X\n",temp9,temp0,v1,w@v1,w@(v1+2),w@(v1+4),w@(v1+6),a0,gp,t1,t3,t4,s5,s6,s7,t8,k0,k1,fp,s2 ; g}

# Key points in the common renderer expose the exact fixed-point calculations.
bp 8801083c,temp1==1,{logerror "KI_S2R phase=vertical_fixed call=%X frame_origin_y_product=%016X y_scale=%016X y_base_fixed=%016X y_with_origin=%016X\n",temp9,s0,t3,t8,t9 ; g}
bp 88010900,temp1==1,{temp8=a0 ; g}
bp 88010968,temp1==1,{logerror "KI_S2R phase=surface call=%X fbselect_register=%X framebuffer=%016X row_y=%016X row_byte_offset=%016X frame_origin_x_product=%016X scaled_width=%016X scaled_height=%016X x_base_fixed=%016X row_pointer=%016X\n",temp9,temp8,v0,t8,a0,t5,t6,s0,s7,a3 ; g}
bp 88010aa0,temp1==1,{logerror "KI_S2R phase=horizontal_fixed call=%X x_integer=%016X x_fraction=%016X x_anchor=%016X scaled_width=%016X scaled_height=%016X row_pointer=%016X\n",temp9,s7,a0,a1,t6,s0,a3 ; g}
bp 88010bd4,temp1==1,{logerror "KI_S2R phase=clip call=%X render_mode=%X next_row_left_boundary=%016X next_row_right_boundary=%016X available_rows=%016X y_integer=%016X vertical_clip_origin=%016X vertical_fraction_prebias=%016X horizontal_remainder=%016X row_stride=%016X\n",temp9,s6,a1,a2,s3,t9,s5,t2,a0,s2 ; g}

# The mode-indexed dispatch loads the per-row callback into $s3. Bit 0x08 of
# $s6 selects a horizontally clipped callback; recording both values prevents
# confusing callback choice with transparency or palette behavior.
bp 88010ce8,temp1==1,{logerror "KI_S2R phase=dispatch call=%X render_mode=%X row_callback=%016X palette_table=%016X\n",temp9,s6,s3,t6 ; g}

# At the first accepted packed row all transform fields used by the native
# renderer are resolved. The actual palette lookup is captured separately.
# 0x88011800 is reached only when the downscale loop rejects a complete packed
# source row. Count it and retain the cumulative source-byte advance from live
# registers so the first accepted source cursor can be verified without
# copying expressive frame bytes into the repository.
bp 88011800,temp1==1&&temp3==1,{temp5=temp5+1 ; temp6=a0-temp7-8 ; g}
bp 88011874,temp1==1&&temp3==1,{temp3=0 ; logerror "KI_S2R phase=first_row call=%X object=%016X frame=%016X source=%016X skipped_source_rows=%X skipped_source_bytes=%X destination=%016X x_scale=%016X x_remainder=%016X y_accumulator=%016X y_scale=%016X rows_minus_one=%016X row_stride=%016X pixel_step=%016X palette=%016X\n",temp9,temp0,d@(temp0+30),v1,temp5,temp6,v0,t1,t0,t2,t3,s0,k1,t4,t6 ; g}

# 0x880118bc is the actual signed-halfword palette load. Capture its address,
# once per call, before the instruction executes. Unlike a callback address,
# this directly proves which five-bit palette index the packed stream used.
bp 880118bc,temp1==1&&temp2==1,{temp2=0 ; logerror "KI_S2R phase=palette_lookup call=%X table=%016X address=%016X\n",temp9,t6,t9 ; g}

# Close the call only at the return from this renderer invocation. A call that
# never reaches first_row is still visible to the analyzer as incomplete.
bp 88001e68,temp1==1,{logerror "KI_S2R phase=done call=%X object=%016X\n",temp9,temp0 ; temp1=0 ; temp2=0 ; temp3=0 ; temp9=temp9+1 ; g}

# Remain below the next three-second autoplay interval while allowing the last
# particle from this contact to disappear.
gtime #2500
bpclear
quit
