#include "../Unity/src/unity.h"
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

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
