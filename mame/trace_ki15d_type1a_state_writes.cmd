# Record every write to the first type-0x1a particle from the end of its
# constructor through release. The write PC assigns each changing field to the
# original update routine instead of inferring ownership from snapshots alone.
focus maincpu

gtime #26000
bp 8800b1ec
g

bpclear
logerror "KI_TYPE1A_STATE phase=ready record=8808C000 words04=%08X:%08X:%08X:%08X token=%02X timer=%08X script=%08X scale58=%04X:%04X\n",d@8808c004,d@8808c008,d@8808c00c,d@8808c010,b@8808c014,d@8808c018,d@8808c020,w@8808c058,w@8808c05a

# Log the pre-write record state together with MAME's pending write value.
wp 8808c000,100,w,1,{logerror "KI_TYPE1A_STATE phase=write pc=%016X address=%016X data=%016X size=%X words04=%08X:%08X:%08X:%08X token=%02X timer=%08X script=%08X state34=%02X scale58=%04X:%04X\n",pc,wpaddr,wpdata,wpsize,d@8808c004,d@8808c008,d@8808c00c,d@8808c010,b@8808c014,d@8808c018,d@8808c020,b@8808c034,w@8808c058,w@8808c05a ; g}

# Stop at the general release entry before its clearing loop erases the final
# state. This excludes the later reuse of slot 0x8808c000 by the last particle.
bp 880054d0,fp==ffffffff8808c000,{logerror "KI_TYPE1A_STATE phase=release record=%016X token=%02X timer=%08X script=%08X words04=%08X:%08X:%08X:%08X scale58=%04X:%04X\n",fp,b@(fp+14),d@(fp+18),d@(fp+20),d@(fp+4),d@(fp+8),d@(fp+c),d@(fp+10),w@(fp+58),w@(fp+5a) ; quit}
g
