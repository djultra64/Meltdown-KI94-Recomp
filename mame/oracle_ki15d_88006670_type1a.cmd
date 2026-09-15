# Exercise the installed-script path of the original common animation routine.
# These cases cover every command used by the type-0x1a particle stream while
# deliberately avoiding the unrelated zero-script descriptor initialization.
focus maincpu
step 2
load work/kipack/ki15d/rom-0.bin,88000000
load work/kipack/ki15d/rom-1.bin,88033900
bp 8800b300
trace work/mame/traces/oracle-ki15d-88006670-type1a.trace,maincpu,noloop

# Case 1: original stream start executes opcode 0x10, then selects token 0x04.
fill 88091000,100,a5
fill 880872a0,40,c3
do b@880872a0 = 01
do d@88091014 = 00000000
do d@88091018 = 00000000
do d@88091020 = 8805e6f2
do w@88091042 = 0100
do d@8808727c = 11223344
tracelog "KI_ANIMATION_TYPE1A case=1 before token=%02X timer=%08X script=%08X scale=%04X gp=1 global=%08X arrays=%08X:%08X\n",b@88091014,d@88091018,d@88091020,w@88091042,d@8808727c,d@880872a0,d@880872c0
do fp = ffffffff88091000
do gp = 1
do ra = ffffffff8800b300
do pc = ffffffff88006670
g
tracelog "KI_ANIMATION_TYPE1A case=1 after token=%02X timer=%08X script=%08X scale=%04X global=%08X arrays=%08X:%08X\n",b@88091014,d@88091018,d@88091020,w@88091042,d@8808727c,d@880872a0,d@880872c0

# Case 2: an active 0x0100 timer is decremented without consuming script data.
fill 88091200,100,3c
do d@88091214 = 00000004
do d@88091218 = 00000100
do d@88091220 = 8805e6f8
do w@88091242 = 0100
do d@8808727c = 55667788
tracelog "KI_ANIMATION_TYPE1A case=2 before token=%02X timer=%08X script=%08X scale=%04X gp=1 global=%08X\n",b@88091214,d@88091218,d@88091220,w@88091242,d@8808727c
do fp = ffffffff88091200
do gp = 1
do ra = ffffffff8800b300
do pc = ffffffff88006670
g
tracelog "KI_ANIMATION_TYPE1A case=2 after token=%02X timer=%08X script=%08X scale=%04X global=%08X\n",b@88091214,d@88091218,d@88091220,w@88091242,d@8808727c

# Case 3: zero timer consumes the next original token-duration pair.
fill 88091400,100,5a
do d@88091414 = 00000004
do d@88091418 = 00000000
do d@88091420 = 8805e6f8
do w@88091442 = 0100
do d@8808727c = 99aabbcc
tracelog "KI_ANIMATION_TYPE1A case=3 before token=%02X timer=%08X script=%08X scale=%04X gp=1 global=%08X\n",b@88091414,d@88091418,d@88091420,w@88091442,d@8808727c
do fp = ffffffff88091400
do gp = 1
do ra = ffffffff8800b300
do pc = ffffffff88006670
g
tracelog "KI_ANIMATION_TYPE1A case=3 after token=%02X timer=%08X script=%08X scale=%04X global=%08X\n",b@88091414,d@88091418,d@88091420,w@88091442,d@8808727c

# Case 4: a synthetic pair verifies signed division by a half-rate scale.
fill 88090000,10,00
do b@88090000 = 33
do b@88090001 = 03
fill 88091600,100,c3
do d@88091614 = 00000020
do d@88091618 = 00000000
do d@88091620 = 88090000
do w@88091642 = 0080
do d@8808727c = ddeeff00
tracelog "KI_ANIMATION_TYPE1A case=4 before token=%02X timer=%08X script=%08X scale=%04X gp=5 global=%08X\n",b@88091614,d@88091618,d@88091620,w@88091642,d@8808727c
do fp = ffffffff88091600
do gp = 5
do ra = ffffffff8800b300
do pc = ffffffff88006670
g
tracelog "KI_ANIMATION_TYPE1A case=4 after token=%02X timer=%08X script=%08X scale=%04X global=%08X\n",b@88091614,d@88091618,d@88091620,w@88091642,d@8808727c

# Case 5: parsing uses only the low timer byte before adding a new duration.
fill 88090020,10,00
do b@88090020 = 44
do b@88090021 = 01
fill 88091800,100,7e
do d@88091814 = 00000021
do d@88091818 = 00000080
do d@88091820 = 88090020
do w@88091842 = 0100
do d@8808727c = 01234567
tracelog "KI_ANIMATION_TYPE1A case=5 before token=%02X timer=%08X script=%08X scale=%04X gp=5 global=%08X\n",b@88091814,d@88091818,d@88091820,w@88091842,d@8808727c
do fp = ffffffff88091800
do gp = 5
do ra = ffffffff8800b300
do pc = ffffffff88006670
g
tracelog "KI_ANIMATION_TYPE1A case=5 after token=%02X timer=%08X script=%08X scale=%04X global=%08X\n",b@88091814,d@88091818,d@88091820,w@88091842,d@8808727c

# Case 6: original opcode 0x14 terminates the particle script.
fill 88091a00,100,9b
do d@88091a14 = 00000015
do d@88091a18 = 00000000
do d@88091a20 = 8805e71a
do w@88091a42 = 0100
do d@8808727c = 89abcdef
tracelog "KI_ANIMATION_TYPE1A case=6 before token=%02X timer=%08X script=%08X scale=%04X gp=1 global=%08X\n",b@88091a14,d@88091a18,d@88091a20,w@88091a42,d@8808727c
do fp = ffffffff88091a00
do gp = 1
do ra = ffffffff8800b300
do pc = ffffffff88006670
g
tracelog "KI_ANIMATION_TYPE1A case=6 after token=%02X timer=%08X script=%08X scale=%04X global=%08X\n",b@88091a14,d@88091a18,d@88091a20,w@88091a42,d@8808727c

# Case 7: opcode 0x10 moves gp=5 to requested one-based array position 3.
fill 88090040,10,00
do b@88090040 = 00
do b@88090041 = 10
do b@88090042 = 03
do b@88090044 = 22
do b@88090045 = 02
do d@880872a0 = 03020100
do d@880872a4 = 07060504
do d@880872a8 = 0b0a0908
do d@880872ac = 0f0e0d0c
do d@880872b0 = 13121110
do d@880872b4 = 17161514
do d@880872b8 = 1b1a1918
do d@880872bc = 1f1e1d1c
fill 880872c0,20,a5
fill 88091c00,100,b6
do d@88091c14 = 00000000
do d@88091c18 = 00000000
do d@88091c20 = 88090040
do w@88091c42 = 0100
do d@8808727c = fedcba98
tracelog "KI_ANIMATION_TYPE1A case=7 before token=%02X timer=%08X script=%08X scale=%04X gp=5 global=%08X base=%08X:%08X:%08X:%08X:%08X:%08X:%08X:%08X scratch=%08X\n",b@88091c14,d@88091c18,d@88091c20,w@88091c42,d@8808727c,d@880872a0,d@880872a4,d@880872a8,d@880872ac,d@880872b0,d@880872b4,d@880872b8,d@880872bc,d@880872c0
do fp = ffffffff88091c00
do gp = 5
do ra = ffffffff8800b300
do pc = ffffffff88006670
g
tracelog "KI_ANIMATION_TYPE1A case=7 after token=%02X timer=%08X script=%08X scale=%04X global=%08X base=%08X:%08X:%08X:%08X:%08X:%08X:%08X:%08X scratch=%08X:%08X:%08X:%08X:%08X:%08X:%08X:%08X\n",b@88091c14,d@88091c18,d@88091c20,w@88091c42,d@8808727c,d@880872a0,d@880872a4,d@880872a8,d@880872ac,d@880872b0,d@880872b4,d@880872b8,d@880872bc,d@880872c0,d@880872c4,d@880872c8,d@880872cc,d@880872d0,d@880872d4,d@880872d8,d@880872dc

trace off,maincpu
traceflush
quit
