# Observe one complete controlled Endokuken impact. Use the documented Lua
# driver and 35-second safety limit. Values read from memory at init_entry,
# init_table and update/render are observations, not authoritative dirty-cache
# reads; c4/c5/c6/c7 initializer values and emitter decisions use CPU registers.
focus maincpu
gtime #25000
temp0=0
temp1=0
temp3=0
temp7=0

bp 8800a2e4,fp==ffffffff8808bd00,{logerror "KI_EMIT phase=init_entry receiver=%016X attacker=%016X descriptor=%016X effect1d=%02X descriptor13=%02X descriptor1a=%02X attacker_variant=%02X\n",fp,t4,t5,b@(t5+1d),b@(t5+13),b@(t5+1a),b@(t4+8e) ; g}
bp 8800a304,fp==ffffffff8808bd00,{logerror "KI_EMIT phase=init_table row=%016X values=%02X:%02X:%02X effect1d=%02X\n",t7,b@t7,b@(t7+1),b@(t7+2),b@(t5+1d) ; g}
bp 8800a308,fp==ffffffff8808bd00,{logerror "KI_EMIT phase=init_c4 value=%02X\n",t6 ; g}
bp 8800a310,fp==ffffffff8808bd00,{logerror "KI_EMIT phase=init_c5 value=%02X\n",t6 ; g}
bp 8800a344,fp==ffffffff8808bd00,{logerror "KI_EMIT phase=init_c6 value=%02X\n",a2 ; g}
bp 8800a378,fp==ffffffff8808bd00,{logerror "KI_EMIT phase=init_c7 value=%02X\n",a2 ; g}

# Both lbu instructions have executed at 3d40; log their live registers.
bp 88003d40,fp==ffffffff8808bd00,{logerror "KI_EMIT phase=step index=%X countdown=%02X cadence=%02X gp=%X\n",temp0,a2,a3,gp ; temp0=temp0+1 ; g}
bp 88003d58,fp==ffffffff8808bd00,{logerror "KI_EMIT phase=decision index=%X countdown_after=%02X cadence_high=%02X cadence_low=%02X spawn=%X\n",temp0-1,a2,a3,t0,a3>=t0 ; g}
bp 88003d64,fp==ffffffff8808bd00,{logerror "KI_EMIT phase=spawn index=%X ordinal=%X countdown_after=%02X cadence_reset=%02X gp=%X\n",temp0-1,temp1,a2,t0,gp ; temp1=temp1+1 ; g}
# a2 now holds the allocator result, NOT the receiver countdown.
bp 8800b214,fp==ffffffff8808bd00,{logerror "KI_EMIT phase=allocated ordinal=%X object=%016X gp=%X\n",temp1-1,t2,gp ; g}
bp 88004e54,b@fp==1a,{logerror "KI_EMIT phase=update sequence=%X object=%016X token=%02X script=%08X\n",temp7,fp,b@(fp+14),d@(fp+20) ; temp7=temp7+1 ; g}
bp 880054d0,b@fp==1a,{logerror "KI_EMIT phase=release object=%016X token=%02X script=%08X\n",fp,b@(fp+14),d@(fp+20) ; g}

# The executing selector register avoids a stale debugger-side bank read.
bp 88001b90,b@fp==1a,{temp2=fp ; temp3=1 ; g}
bp 88010900,temp3==1,{temp4=a0 ; g}
bp 88010968,temp3==1,{logerror "KI_EMIT phase=render object=%016X token=%02X bank=%X\n",temp2,b@(temp2+14),temp4&1 ; temp3=0 ; g}
gtime #6000
bpclear
quit
