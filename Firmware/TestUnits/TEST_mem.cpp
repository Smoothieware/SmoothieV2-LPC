#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#include "board.h"

#include "TestRegistry.h"

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
#define clock_systimer() ((systime_t)Chip_RIT_GetCounter(LPC_RITIMER))
#define TICK2USEC(x) ((systime_t)(((uint64_t)(x)*1000000)/timerFreq))

_ramfunc_ void runMemoryTest()
{
    /* Get RIT timer peripheral clock rate */
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);

    register uint32_t* p = (uint32_t *)0x14000000;
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
    while(p < (uint32_t *)(0x14000000+n)) {
        asm volatile ("ldm.w %[ptr]!,{%[r1],%[r2],%[r3],%[r4],%[r5],%[r6],%[r7],%[r8]}" :
                        [r1] "=r" (r1), [r2] "=r" (r2), [r3] "=r" (r3), [r4] "=r" (r4),
                        [r5] "=r" (r5), [r6] "=r" (r6),[r7] "=r" (r7), [r8] "=r" (r8),
                        [ptr] "=r" (p)                                                  :
                        "r" (p)                                                         : );
    }
    systime_t en = clock_systimer();

    printf("elapsed time %lu us over %lu bytes %1.4f mb/sec\n", TICK2USEC(en-st), n, (float)n/TICK2USEC(en-st));
}

REGISTER_TEST(MemoryTest, time_flash)
{
    runMemoryTest();
}
