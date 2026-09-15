# Exhaustive byte-state oracle for the original 0x88003d30 emitter block.
# Run with -nodrc. All addresses in breakpoint actions are debugger landing
# points, never replacement guest instructions. The ORIGINAL jal delay slot
# executes before the constructor entry is intercepted. Only the constructor
# body is skipped; it has its own full-memory differential tests.
focus maincpu
step 2
load work/kipack/ki15d/rom-0.bin,88000000
fill 88092000,100,5a
# A redirected PC executes once before breakpoints are checked again. These
# explicitly zeroed landing instructions are nops, outside the original block.
fill 88090ffc,10,00
fp=ffffffff88092000
gp=1
temp0=0
temp1=0
temp2=811c9dc5
temp3=0
temp4=0

# temp0=countdown, temp1=cadence, temp2=rolling uint32 digest, temp3=call seen.
# Hash one packed result word (c4<<16 | c5<<8 | called) per cadence, ascending.
bp 8800b1fc,1,{temp3=1 ; pc=ffffffff88003d6c ; g}
bp 88002088,1,{temp2=((temp2^((b@880920c4<<10)|(b@880920c5<<8)|temp3))*01000193)&ffffffff ; temp1=temp1+1 ; temp4=temp4+1 ; pc=ffffffff88090ffc+((temp1>>8)*4) ; g}
bp 88091000,1,{b@880920c4=temp0 ; b@880920c5=temp1 ; temp3=0 ; pc=ffffffff88003d30 ; g}
bp 88091004,1,{logerror "KI_EMITTER_ORACLE countdown=%02X cases=%X digest=%08X\n",temp0,temp1,temp2 ; temp0=temp0+1 ; temp1=0 ; temp2=811c9dc5 ; pc=ffffffff88090ffc+((temp0>>8)*8) ; g}
bp 88091008,1,{logerror "KI_EMITTER_ORACLE complete=%X\n",temp4 ; quit}

b@880920c4=0
b@880920c5=0
pc=ffffffff88003d30
g
