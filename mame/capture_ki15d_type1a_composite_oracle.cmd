# Save the BGR555 framebuffer immediately before and after the six overlapping
# type-0x1a particles of a peak Endokuken-impact frame. Both banks are saved so
# the oracle stays independent of the bank selected by the replay's timing.
focus maincpu

gtime #25900

# Preserve the secondary-pass owner across 0x88001b90, which reuses $fp.
bp 88001b90,1,{temp0=fp ; g}

# This combination of animation tokens uniquely identifies the first peak
# frame in the controlled impact. Record 0x8808c000 renders first in pass order.
bp 880107a8,temp0==ffffffff8808c000&&b@(temp0+14)==15&&b@8808be14==13&&b@8808bf14==10&&b@8808c114==0c&&b@8808c214==08&&b@8808c314==04
g

save work/mame/oracles/type1a-composite-input-bank0.bgr555,80030000,25800
save work/mame/oracles/type1a-composite-input-bank1.bgr555,80058000,25800

# Resume until the sixth and final object has returned from the common packed
# renderer, then capture the completed framebuffer.
bpclear
bp 88001b90,1,{temp0=fp ; g}
bp 88001e68,temp0==ffffffff8808c300&&b@(temp0+14)==04&&b@8808c014==15&&b@8808be14==13&&b@8808bf14==10&&b@8808c114==0c&&b@8808c214==08
g

save work/mame/oracles/type1a-composite-output-bank0.bgr555,80030000,25800
save work/mame/oracles/type1a-composite-output-bank1.bgr555,80058000,25800

bpclear
quit
