#ifndef KI_MIPS_WORD_OPS_H
#define KI_MIPS_WORD_OPS_H

#include <stdint.h>

static inline uint64_t ki_mips_sign_extend_word(uint32_t value)
{
    return value & UINT32_C(0x80000000) ? UINT64_C(0xffffffff00000000) | value : value;
}

static inline uint64_t ki_mips_srl_word(uint64_t value, unsigned int shift)
{
    return ki_mips_sign_extend_word((uint32_t)value >> shift);
}

static inline uint64_t ki_mips_andi(uint64_t value, uint16_t immediate)
{
    return value & (uint64_t)immediate;
}

static inline uint64_t ki_mips_and(uint64_t left, uint64_t right)
{
    return left & right;
}

static inline uint64_t ki_mips_ori(uint64_t value, uint16_t immediate)
{
    return value | (uint64_t)immediate;
}

#endif
