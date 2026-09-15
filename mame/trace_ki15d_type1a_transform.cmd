# Capture the transform state for one type-0x1a Endokuken-impact frame.
# Run with autoplay_jago_vs_idle_fulgore.lua and -oslog. The script stops after
# the common renderer returns, so every logged pixel belongs to one object.
focus maincpu

gtime #26300

# The object pointer is lost inside 0x88001b90. Preserve every candidate, then
# let the exact frame-pointer and return-address test below select token 0x0b.
bp 88001b90,1,{temp0=fp ; g}

# Log the values handed to the packed-frame renderer. temp1 gates pixel logging
# until this exact call returns to the secondary object pass.
bp 880107a8,v1==ffffffff88097ea4&&ra==ffffffff88001e68,{temp1=1 ; temp2=0 ; temp4=0 ; logerror "KI_TRANSFORM phase=object object=%016X words04=%08X:%08X:%08X:%08X token=%02X state=%02X frame=%08X flags40=%08X scale58=%04X:%04X offsets7c=%04X:%04X:%04X:%04X facing8c=%02X field94=%02X\n",temp0,d@(temp0+4),d@(temp0+8),d@(temp0+c),d@(temp0+10),b@(temp0+14),b@(temp0+34),d@(temp0+30),d@(temp0+40),w@(temp0+58),w@(temp0+5a),w@(temp0+7c),w@(temp0+7e),w@(temp0+80),w@(temp0+84),b@(temp0+8c),b@(temp0+94) ; logerror "KI_TRANSFORM phase=renderer object=%016X frame=%016X a0=%016X a1=%016X a2=%016X a3=%016X v0=%016X t0=%016X t1=%016X t2=%016X t3=%016X t4=%016X t5=%016X t6=%016X t7=%016X t8=%016X t9=%016X s0=%016X s1=%016X s2=%016X s3=%016X s4=%016X s5=%016X s6=%016X s7=%016X gp=%016X fp=%016X\n",temp0,v1,a0,a1,a2,a3,v0,t0,t1,t2,t3,t4,t5,t6,t7,t8,t9,s0,s1,s2,s3,s4,s5,s6,s7,gp,fp ; g}

# At 0x88011840 the row header and destination origin are established. At
# 0x88011874 the renderer has reloaded the horizontal fixed-point remainder.
bp 88011840,temp1==1,{logerror "KI_TRANSFORM phase=row pass=%X source=%016X destination=%016X t1=%X t2=%X t3=%X\n",temp4,v1,v0,t1,t2,t3 ; g}
bp 88011874,temp1==1,{logerror "KI_TRANSFORM phase=row_phase pass=%X source=%016X destination=%016X x_remainder=%X\n",temp4,v1,v0,t0 ; temp4=temp4+1 ; g}

# Record every final store as an exact address/input/blend/output oracle. The
# preceding breakpoint saves the destination color before the blend mutates it.
bp 88011938,temp1==1,{temp3=s5 ; g}
bp 8801195c,temp1==1,{logerror "KI_TRANSFORM phase=pixel n=%X address=%016X input=%04X blend=%04X output=%04X source=%016X row_end=%016X\n",temp2,v0-2,temp3,t9,t8,v1,a0 ; temp2=temp2+1 ; g}

bp 88001e68,temp1==1,{logerror "KI_TRANSFORM phase=done object=%016X pixels=%X\n",temp0,temp2 ; quit}
g
