#include "ki/pc_viewport.h"

#include <stdio.h>

static int expect(int dw,int dh,KiPcFitMode mode,int x,int y,int w,int h,int scale)
{
    KiPcViewport value;
    return ki_pc_viewport_layout(320,240,dw,dh,mode,&value) &&
           value.x==x && value.y==y && value.width==w && value.height==h &&
           value.integer_scale==scale;
}

int main(void)
{
    if (!expect(1920,1080,KI_PC_FIT_INTEGER,320,60,1280,960,4) ||
        !expect(3840,2160,KI_PC_FIT_INTEGER,480,0,2880,2160,9) ||
        !expect(1280,1024,KI_PC_FIT_INTEGER,0,32,1280,960,4) ||
        !expect(100,100,KI_PC_FIT_INTEGER,0,12,100,75,0) ||
        !expect(1000,700,KI_PC_FIT_ASPECT,33,0,933,700,0) ||
        ki_pc_viewport_layout(0,240,1920,1080,KI_PC_FIT_INTEGER,0)) {
        fputs("PC viewport layout failed\n",stderr);return 1;
    }
    puts("PC viewport layout passed");return 0;
}
