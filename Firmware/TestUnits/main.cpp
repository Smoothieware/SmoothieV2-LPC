#include <stdio.h>

#include <vector>
#include <tuple>
#include <functional>

#include "../Unity/src/unity.h"
#include "TestRegistry.h"
#include "board.h"

// #include "OutputStream.h"

// // place holder
// bool dispatch_line(OutputStream& os, const char *line)
// {
//     return true;
// }

static std::function<void(void)> setup_fnc;
void setUp(void)
{
    if(setup_fnc)
        setup_fnc();
}

static std::function<void(void)> teardown_fnc;
void tearDown(void)
{
    if(teardown_fnc)
        teardown_fnc();
}

static std::function<void(void)> test_wrapper_fnc;
static void test_wrapper(void)
{
    test_wrapper_fnc();
}

static int test_runner(void)
{
    auto tests= TestRegistry::instance().get_tests();
    printf("There are %d registered tests...\n", tests.size());
    for(auto& i : tests) {
        printf("  %s\n", std::get<1>(i));
    }

    UnityBegin("TestUnits");

    for(auto i : tests) {
        TestBase *fnc= std::get<0>(i);
        const char *name= std::get<1>(i);
        int ln= std::get<2>(i);
        Unity.TestFile= std::get<3>(i);
        test_wrapper_fnc= std::bind(&TestBase::test, fnc);
        bool st= std::get<4>(i);
        if(st) {
            setup_fnc= std::bind(&TestBase::setUp, fnc);
            teardown_fnc= std::bind(&TestBase::tearDown, fnc);
        }else{
            setup_fnc= nullptr;
            teardown_fnc= nullptr;
        }

        UnityDefaultTestRun(test_wrapper, name, ln);

        // if we get any errors stop here
        if(Unity.TestFailures > 0) break;
    }

    return (UnityEnd());
}

static int run_tests(int argc, char *argv[])
{
    printf("Starting tests...\n");
    int ret = test_runner();
    printf("Done\n");
    return ret;
}

void configureSPIFI();
int main() //int argc, char *argv[])
{
    configureSPIFI(); // full speed ahead

    // int ret = boardctl(BOARDIOC_INIT, 0);
    // if(OK != ret) {
    //     printf("ERROR: BOARDIOC_INIT falied\n");
    // }

    // task_create("tests", SCHED_PRIORITY_DEFAULT,
    //             30000,
    //             (main_t)run_tests,
    //             (FAR char * const *)NULL);
    // return 1;

    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();
    // Set up and initialize all required blocks and
    // functions related to the board hardware
    Board_Init();
    // Set the LED to the state of "On"
    Board_LED_Set(0, true);

    return run_tests(0, nullptr); // argc, argv);
}

#if 0
void safe_sleep(uint32_t ms)
{
    // here we need to sleep (and yield) for 10ms then check if we need to handle the query command
    while(ms > 0) {
        usleep(10000); // 10 ms sleep (minimum anyway due to thread slice time)

        if(ms > 10) {
            ms -= 10;
        }else{
            break;
        }
    }
}

void print_to_all_consoles(const char *str)
{
    printf("%s", str);
}
#endif
