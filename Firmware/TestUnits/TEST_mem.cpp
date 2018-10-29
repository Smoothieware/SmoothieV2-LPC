#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#include "board.h"

#include "TestRegistry.h"

#include "FreeRTOS.h"
#include "task.h"

REGISTER_TEST(MemoryTest, stats)
{
    struct mallinfo mem= mallinfo();
    printf("             total       used       free    largest\n");
    printf("Mem:   %11d%11d%11d%11d\n", mem.arena, mem.uordblks, mem.fordblks, mem.usmblks);
}

char test_ahb0_ram[100] __attribute__ ((section (".bss.$RamAHB32")));
char test_ahb1_ram[100] __attribute__ ((section (".bss.$RamAHB16")));
REGISTER_TEST(MemoryTest, AHBn)
{
    TEST_ASSERT_EQUAL_INT(0x20000000, (unsigned int)&test_ahb0_ram);
    TEST_ASSERT_EQUAL_INT(0x20008000, (unsigned int)&test_ahb1_ram);

    for (int i = 0; i < 100; ++i) {
        test_ahb0_ram[i]= i;
        test_ahb1_ram[i]= i+10;
    }

    for (int i = 0; i < 100; ++i) {
        TEST_ASSERT_EQUAL_INT(i, test_ahb0_ram[i]);
        TEST_ASSERT_EQUAL_INT(i+10, test_ahb1_ram[i]);
    }
}

#define _ramfunc_ __attribute__ ((section(".ramfunctions"),long_call,noinline))

_ramfunc_ int testramfunc() { return 123; }
REGISTER_TEST(MemoryTest, ramfunc)
{
    printf("ramfunc is at %p\n", testramfunc);
    TEST_ASSERT_TRUE((unsigned int)testramfunc >= 0x10000000 && (unsigned int)testramfunc < 0x10020000);
    TEST_ASSERT_EQUAL_INT(123, testramfunc());
}

using systime_t= uint32_t;
#define clock_systimer() ((systime_t)LPC_RITIMER->COUNTER)
#define TICK2USEC(x) ((systime_t)(((uint64_t)(x)*1000000)/timerFreq))

_ramfunc_ void runMemoryTest(uint32_t timerFreq, void *addr)
{
    register uint32_t* p = (uint32_t *)addr;
    register uint32_t r1;
    register uint32_t r2;
    register uint32_t r3;
    register uint32_t r4;
    register uint32_t r5;
    register uint32_t r6;
    register uint32_t r7;
    register uint32_t r8;

    uint32_t n= 8000000;
    systime_t st = clock_systimer();
    while(p < (uint32_t *)((uint32_t)addr+n)) {
        asm volatile ("ldm.w %[ptr]!,{%[r1],%[r2],%[r3],%[r4],%[r5],%[r6],%[r7],%[r8]}" :
                        [r1] "=r" (r1), [r2] "=r" (r2), [r3] "=r" (r3), [r4] "=r" (r4),
                        [r5] "=r" (r5), [r6] "=r" (r6),[r7] "=r" (r7), [r8] "=r" (r8),
                        [ptr] "=r" (p)                                                  :
                        "r" (p)                                                         : );
    }
    systime_t en = clock_systimer();

    printf("elapsed time %lu us over %lu bytes %1.4f mb/sec\n", TICK2USEC(en-st), n, (float)n/TICK2USEC(en-st));
}

REGISTER_TEST(MemoryTest, time_spifi)
{
    printf("Timing memory at 0x14000000\n");
    /* Get RIT timer peripheral clock rate */
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);
    taskENTER_CRITICAL();
    runMemoryTest(timerFreq, (void*)0x14000000);
    taskEXIT_CRITICAL();
}

#ifdef BOARD_PRIMEALPHA
// TODO move these to sys init
/*
     Chip select     0
     Configuration value     EMC_STATIC_CONFIG_MEM_WIDTH_16 | EMC_STATIC_CONFIG_CS_POL_ACTIVE_LOW | EMC_STATIC_CONFIG_BLS_HIGH | EMC_STATIC_CONFIG_PAGE_MODE_ENABLE
     Write Enable Wait 0ns
     Output Enable Wait 0ns
     Read Wait 90ns
     Page Access Wait 25ns
     Write Wait 90ns
     Turn around wait 1 clock
*/
static const IP_EMC_STATIC_CONFIG_T S29GL064N_config = {
    0,
    EMC_STATIC_CONFIG_MEM_WIDTH_16 | EMC_STATIC_CONFIG_CS_POL_ACTIVE_LOW | EMC_STATIC_CONFIG_BLS_HIGH | EMC_STATIC_CONFIG_PAGE_MODE_ENABLE,
    EMC_NANOSECOND(0),   // Delay from chip select assertion to write enable
    EMC_NANOSECOND(0),   // Delay from chip select assertion to output enable.
    EMC_NANOSECOND(90),  // Non-page mode read wait states or asynchronous page mode read firstaccess wait state.
    EMC_NANOSECOND(25),  // Asynchronous page mode read after the first read wait states. Number of wait states for asynchronous page mode read accesses after the first read:
    EMC_NANOSECOND(90),  // delay from the chip select to the write access
    EMC_CLOCK(1)        // Bus turnaround cycles.
};

/* EMC clock delay */
//#define CLK0_DELAY 1

REGISTER_TEST(MemoryTest, time_flash)
{
    /* NorFlash timing and chip Config */

    // setup EMC first
    /* Move all clock delays together NOT needed for flash */
    //LPC_SCU->EMCDELAYCLK = ((CLK0_DELAY) | (CLK0_DELAY << 4) | (CLK0_DELAY << 8) | (CLK0_DELAY << 12));

    /* Setup EMC Clock Divider for divide by 2 - this is done in both the CCU (clocking)
           and CREG. For frequencies over 120MHz, a divider of 2 must be used. For frequencies
           less than 120MHz, a divider of 1 or 2 is ok. */
    Chip_Clock_EnableOpts(CLK_MX_EMC_DIV, true, true, 2);
    LPC_CREG->CREG6 |= (1 << 16);
    /* Enable EMC clock */
    Chip_Clock_Enable(CLK_MX_EMC);

    /* Init EMC Controller -Enable-LE mode- clock ratio 1:1 */
    Chip_EMC_Init(1, 0, 0);
    /* Init EMC Static Controller CS0 */
    Chip_EMC_Static_Init((IP_EMC_STATIC_CONFIG_T *) &S29GL064N_config);
    /* Enable Buffer for External NOR Flash NOTE seems to slow it down and cause issues */
    //LPC_EMC->STATICCONFIG0 |= 1 << 19;

    uint32_t clk= Chip_Clock_GetEMCRate();
    printf("EMC clock rate= %lu\n", clk);

#if 1
    printf("Timing memory at 0x1C000000\n");
    /* Get RIT timer peripheral clock rate */
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);
    taskENTER_CRITICAL();
    runMemoryTest(timerFreq, (void*)0x1C000000);
    taskEXIT_CRITICAL();
#endif

    // next try to read the CFI

    uint16_t *pr= (uint16_t *)0x1C000020;
    uint16_t d;

    d= *pr;
    printf("Array Read %p: %04X\n", pr, d);
    TEST_ASSERT_EQUAL_INT(d, 0xFFFF);

    // Write CFI Query
    uint16_t *pw= (uint16_t *)0x1C0000AA;
    *pw= 0x0098;

    uint16_t qry_data[11]= {0x51, 0x52, 0x59, 0x02, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00 };
    for (int i = 0; i < 11; ++i) {
        d= *(pr+i);
        printf("CFI Read %p: %02X (%c)\n", pr+i, d, d);
        TEST_ASSERT_EQUAL_INT(qry_data[i], d);
    }

    pr= (uint16_t *)0x1C000000;
    for (int i = 0x27; i <= 0x3C; ++i) {
        d= *(pr+i);
        printf("CFI GEO %p: %02X\n", pr+i, d);
    }

    for (int i = 0x40; i <= 0x50; ++i) {
        d= *(pr+i);
        printf("CFI VEQ %p: %02X (%c)\n", pr+i, d, d);
    }

    // reset
    pr= (uint16_t *)0x1C000020;
    *pw= 0x0098;
    d= *(pr+0); TEST_ASSERT_EQUAL_INT(d, 'Q');
    d= *(pr+1); TEST_ASSERT_EQUAL_INT(d, 'R');
    d= *(pr+2); TEST_ASSERT_EQUAL_INT(d, 'Y');
    *((uint16_t *)0x1C000020) = 0x00FF;
    vTaskDelay(pdMS_TO_TICKS(1));
    d= *pr;
    printf("After reset Array Read %p: %04X\n", pr, d);
    TEST_ASSERT_EQUAL_INT(d, 0xFFFF);
    d= *pr;
    printf("After reset Read %p: %04X\n", pr, d);

}
#endif
