# Capture the memory needed for offline analysis of the Endokuken renderer.
# Run with autoplay_jago_vs_idle_fulgore.lua. Generated dumps stay under work/.
focus maincpu

# This point is inside the rising green impact in the deterministic replay.
gtime #27100

# Main RAM contains the active object records, frame tables, packed frame data,
# renderer tables, and dynamically loaded game code. Low RAM contains both
# alternating 320x240 BGR555 framebuffer banks beginning at offset 0x30000.
save work/mame/dumps/endokuken-mainram-27.1s.bin,88000000,800000
save work/mame/dumps/endokuken-lowram-27.1s.bin,80000000,80000

quit
