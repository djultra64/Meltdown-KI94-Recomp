#include "ki/native_pilot.h"
#include <limits.h>
#include <stdio.h>

int main(void)
{
    int32_t result;
    if(!ki_native_pilot_sub_word(17,9,&result)||result!=8||
       ki_native_pilot_sub_word(INT32_MIN,1,&result)||
       ki_native_pilot_sub_word(INT32_MAX,-1,&result))
        return fprintf(stderr,"trapping SUB admission failed\n"),1;
    puts("renderer arithmetic frontier gates passed");
    return 0;
}
