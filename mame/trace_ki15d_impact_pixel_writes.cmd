# Identify the original instructions that write two pixels inside the visible
# Endokuken impact. Both framebuffer banks are watched because KI alternates
# between them every frame. Run MAME with -oslog so the compact logerror lines
# are available on standard error without enabling a full instruction trace.
focus maincpu

# Frame 1600 in the controlled capture is about 27.1 seconds after boot at the
# board's measured refresh rate. Arm the watchpoints shortly beforehand and
# observe 1.3 seconds spanning the contact and rising green plume.
gtime #26300

# Pixel (260, 100), near the bright core. Its byte offset is
# ((100 * 320) + 260) * 2 = 0xfc08.
wp 8003fc08,2,w,1,{logerror "KI_IMPACT_PIXEL sample=core bank=0 pc=%016X address=%016X data=%016X size=%X return=%016X frame_cursor=%016X row_end=%016X source_end=%016X\n",pc,wpaddr,wpdata,wpsize,ra,v1,a0,s5 ; g}
wp 80067c08,2,w,1,{logerror "KI_IMPACT_PIXEL sample=core bank=1 pc=%016X address=%016X data=%016X size=%X return=%016X frame_cursor=%016X row_end=%016X source_end=%016X\n",pc,wpaddr,wpdata,wpsize,ra,v1,a0,s5 ; g}

# Pixel (255, 90), in the green halo. Its byte offset is
# ((90 * 320) + 255) * 2 = 0xe2fe.
wp 8003e2fe,2,w,1,{logerror "KI_IMPACT_PIXEL sample=halo bank=0 pc=%016X address=%016X data=%016X size=%X return=%016X frame_cursor=%016X row_end=%016X source_end=%016X\n",pc,wpaddr,wpdata,wpsize,ra,v1,a0,s5 ; g}
wp 800662fe,2,w,1,{logerror "KI_IMPACT_PIXEL sample=halo bank=1 pc=%016X address=%016X data=%016X size=%X return=%016X frame_cursor=%016X row_end=%016X source_end=%016X\n",pc,wpaddr,wpdata,wpsize,ra,v1,a0,s5 ; g}

gtime #1300
wpclear
quit
