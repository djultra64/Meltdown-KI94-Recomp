#include "ki/native_pilot.h"
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    uint32_t bits,root;
    int32_t integer;
    if (!ki_native_pilot_exact_i32_to_f32(1849,&bits) || bits!=UINT32_C(0x44e72000) ||
        !ki_native_pilot_exact_f32_sqrt(bits,&root) || root!=UINT32_C(0x422c0000) ||
        !ki_native_pilot_exact_f32_to_i32(root,&integer) || integer!=43)
        return fprintf(stderr,"observed exact chain failed\n"),1;
    if (ki_native_pilot_exact_i32_to_f32(16777217,&bits) ||
        ki_native_pilot_exact_f32_sqrt(UINT32_C(0x40000000),&root) ||
        ki_native_pilot_exact_f32_sqrt(UINT32_C(0x80000000),&root) ||
        ki_native_pilot_exact_f32_to_i32(UINT32_C(0x3fc00000),&integer))
        return fprintf(stderr,"inexact or unsupported FP domain accepted\n"),1;
    unsigned accepted=0;
    for (unsigned exponent=1;exponent<255;++exponent) {
        for (uint32_t candidate=2896;candidate<=5792;++candidate) {
            uint32_t mantissa=candidate*candidate;
            int power=(int)exponent-150;
            if (power&1) {
                if ((mantissa&1u)!=0) continue;
                mantissa>>=1;
            }
            if (mantissa<UINT32_C(0x800000) || mantissa>=UINT32_C(0x1000000)) continue;
            bits=(exponent<<23)|(mantissa&UINT32_C(0x7fffff));
            if (!ki_native_pilot_exact_f32_sqrt(bits,&root))
                return fprintf(stderr,"exact square rejected: %08x\n",bits),1;
            unsigned top=31;
            while (((candidate>>top)&1u)==0) --top;
            const int adjusted_power=power&1 ? power-1 : power;
            const uint32_t expected=(uint32_t)(adjusted_power/2+(int)top+127)<<23 |
                ((candidate<<(23-top))&UINT32_C(0x7fffff));
            if(root!=expected)
                return fprintf(stderr,"wrong exact root: %08x -> %08x, expected %08x\n",bits,root,expected),1;
            ++accepted;
        }
    }
    if (accepted<100000) return fprintf(stderr,"property set unexpectedly small\n"),1;
    printf("contact exact-FP property gates passed (%u squares)\n",accepted);
    return 0;
}
