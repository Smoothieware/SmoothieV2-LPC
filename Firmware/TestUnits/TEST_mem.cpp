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

REGISTER_TEST(MemoryTest, time_spi)
{
    printf("Timing memory at 0x14000000\n");
    /* Get RIT timer peripheral clock rate */
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);
    taskENTER_CRITICAL();
    runMemoryTest(timerFreq, (void*)0x14000000);
    taskEXIT_CRITICAL();
}

static const IP_EMC_STATIC_CONFIG_T SST39VF320_config = {
    0,
    EMC_STATIC_CONFIG_MEM_WIDTH_16 |
    EMC_STATIC_CONFIG_CS_POL_ACTIVE_LOW |
    EMC_STATIC_CONFIG_BLS_HIGH,

    EMC_NANOSECOND(0),
    EMC_NANOSECOND(35),
    EMC_NANOSECOND(70),
    EMC_NANOSECOND(70),
    EMC_NANOSECOND(40),
    EMC_CLOCK(4)
};

/* EMC clock delay */
#define CLK0_DELAY 5

REGISTER_TEST(MemoryTest, time_flash)
{
    /* NorFlash timing and chip Config */
    printf("Timing memory at 0x1C000000\n");

    // setup EMC first
    /* Move all clock delays together */
    LPC_SCU->EMCDELAYCLK = ((CLK0_DELAY) | (CLK0_DELAY << 4) | (CLK0_DELAY << 8) | (CLK0_DELAY << 12));

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
    Chip_EMC_Static_Init((IP_EMC_STATIC_CONFIG_T *) &SST39VF320_config);

    /* Enable Buffer for External NOR Flash */
    //LPC_EMC->STATICCONFIG0 |= 1 << 19;

    // first try to read the CFI
    // TODO

    /* Get RIT timer peripheral clock rate */
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);
    taskENTER_CRITICAL();
    runMemoryTest(timerFreq, (void*)0x1C000000);
    taskEXIT_CRITICAL();
}
