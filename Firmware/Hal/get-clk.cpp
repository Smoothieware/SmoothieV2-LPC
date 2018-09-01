#include <stdlib.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#define putreg32(v,a)  (*(volatile uint32_t *)(a) = (v))

float get_pll1_clk()
{
    uint32_t x= *(uint32_t *)0x40050044; // read PLL1 register
    uint32_t clksrc= (x >> 24)&0x1F;
    if(clksrc != 0x06) {
        printf("WARNING: PLL1 is notr sourced by xtal\n");
        return 0;
    }

    uint32_t m= (x>>16) & 0xFF;
    uint32_t n= (x>>12) & 0x03;
    //uint32_t psel = (x>>8) & 0x03;
    float fclkout=  (m+1) * (12e6F / (n+1));
    printf("FCLKOUT= %10.1f MHz\n", fclkout/1000000.0F);

    // test frequencies using Frequency monitor register
    // get PLL1
    uint32_t fmr= (0x09<<24) | (1<<23) | (200);
    putreg32(fmr, 0x40050014);
    vTaskDelay(pdMS_TO_TICKS(100)); //usleep(100000);
    uint32_t mf= *(uint32_t *)0x40050014;
    //printf("mf= %08X\n", mf);
    float freq= 12e6F * (((mf>>9) & 0x3FFF)/200.0F);
    printf("Measured PLL1= %10.1f Hz\n", freq);

    // get DIVB
    fmr= (0x0D<<24) | (1<<23) | (200);
    putreg32(fmr, 0x40050014);
    vTaskDelay(pdMS_TO_TICKS(100)); //usleep(100000);
    mf= *(uint32_t *)0x40050014;
    freq= 12e6F * (((mf>>9) & 0x3FFF)/200.0F);
    printf("Measured DIVB= %10.1f Hz\n", freq);

    return fclkout;
}
