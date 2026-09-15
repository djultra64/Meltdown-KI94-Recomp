#ifndef KI_ORIGINAL_KI15D_88003D30_H
#define KI_ORIGINAL_KI15D_88003D30_H

#include "ki/memory.h"

/*
 * KI v1.5d receiver emission block 0x88003d30..0x88003d70, including its
 * constructor call. Invoke only when the ORIGINAL caller allows a receiver
 * update: this is not a once-per-displayed-frame scheduler or hit initializer.
 *
 * fighter_address is $fp; gp_value is the constructor's orientation selector.
 * Required non-null particle_address is reset to zero (no call). On success
 * a nonzero value identifies the constructed record. On memory failure,
 * earlier writes are retained and a nonzero address may identify a partially
 * initialized record; callers must check the result before using that record.
 * The original allocator falls back to 0x8808db00 when all 29 searched slots
 * are occupied; this API does not invent an out-of-pool failure result.
 *
 * Guest memory effects are preserved. Temporary registers and the unrelated
 * continuation at 0x88002088 are outside this reconstruction's contract.
 */
KiMemoryResult ki15d_88003d30(KiMemory *memory, uint64_t fighter_address,
                             uint32_t gp_value, uint64_t *particle_address);

#endif
