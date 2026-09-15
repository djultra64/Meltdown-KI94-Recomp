#include "ki/mips_word_ops.h"

#include <stdio.h>

int main(void)
{
    if (ki_mips_andi(UINT64_C(0xffffeeee8000ffff), UINT16_C(0x80f0)) !=
            UINT64_C(0x00000000000080f0) ||
        ki_mips_and(UINT64_C(0xffff0000aaaaaaaa),
                    UINT64_C(0x1234ffff55555555)) !=
            UINT64_C(0x1234000000000000) ||
        ki_mips_ori(UINT64_C(0xffff000000000000), UINT16_C(0x80f0)) !=
            UINT64_C(0xffff0000000080f0) ||
        ki_mips_srl_word(UINT64_C(0x1234567880000001), 0) !=
            UINT64_C(0xffffffff80000001) ||
        ki_mips_srl_word(UINT64_C(0xffffffff80000000), 1) !=
            UINT64_C(0x0000000040000000)) {
        fprintf(stderr, "MIPS resumed word-operation semantics diverged\n");
        return 1;
    }
    puts("MIPS resumed word-operation semantics passed");
    return 0;
}
