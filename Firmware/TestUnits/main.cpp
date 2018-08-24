#include <stdio.h>

#include <vector>
#include <tuple>
#include <functional>

#include <malloc.h>
#include <string.h>

#include "../Unity/src/unity.h"
#include "TestRegistry.h"
#include "board.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

#include "OutputStream.h"

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
    auto tests = TestRegistry::instance().get_tests();
    printf("There are %d registered tests...\n", tests.size());
    for(auto& i : tests) {
        printf("  %s\n", std::get<1>(i));
    }

    UnityBegin("TestUnits");

    for(auto i : tests) {
        TestBase *fnc = std::get<0>(i);
        const char *name = std::get<1>(i);
        int ln = std::get<2>(i);
        Unity.TestFile = std::get<3>(i);
        test_wrapper_fnc = std::bind(&TestBase::test, fnc);
        bool st = std::get<4>(i);
        if(st) {
            setup_fnc = std::bind(&TestBase::setUp, fnc);
            teardown_fnc = std::bind(&TestBase::tearDown, fnc);
        } else {
            setup_fnc = nullptr;
            teardown_fnc = nullptr;
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


#if 0
void safe_sleep(uint32_t ms)
{
    // here we need to sleep (and yield) for 10ms then check if we need to handle the query command
    while(ms > 0) {
        usleep(10000); // 10 ms sleep (minimum anyway due to thread slice time)

        if(ms > 10) {
            ms -= 10;
        } else {
            break;
        }
    }
}

void print_to_all_consoles(const char *str)
{
    printf("%s", str);
}
#endif

/* LED1 toggle thread */
extern "C" void vLEDTask1(void *pvParameters)
{
    bool LedState = false;

    while (1) {
        Board_LED_Set(0, LedState);
        LedState = (bool) !LedState;

        /* About a 3Hz on/off toggle rate */
        vTaskDelay(configTICK_RATE_HZ / 6);
    }
}

extern "C" void vRunTestsTask(void *pvParameters)
{
    run_tests(0, nullptr); // argc, argv);

    //vTaskGetTaskState();
    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    printf("High water mark= %lu\n", uxHighWaterMark);
    vTaskDelete( NULL );
}

extern "C" int write_cdc(const char *buf, size_t len);
extern "C" int setup_cdc(void *os);
extern "C" void vComTask(void *pvParameters)
{
    static OutputStream os([](const char *buf, size_t len){ return write_cdc(buf, len); });
    setup_cdc(&os);
    // does not return
}

QueueHandle_t dispatch_queue;

using dispatch_message_t = struct {
    char line[132];
    OutputStream *os;
};

// this would be the command thread in the firmware
extern "C" void dispatch(void *pvParameters)
{
    const TickType_t waitms = pdMS_TO_TICKS( 100 );
    dispatch_message_t message;

    while(1) {
        // now read lines and dispatch them
        if( xQueueReceive( dispatch_queue, &message, waitms) ) {
            // got line
            if(strlen(message.line) == 1) {
                switch(message.line[0]) {
                    case 24: message.os->printf("Got KILL\n"); break;
                    case '?': message.os->printf("Got Query\n"); break;
                    case '!': message.os->printf("Got Hold\n"); break;
                    case '~': message.os->printf("Got Release\n"); break;
                    default: message.os->printf("Got 1 char line: %s\n", message.line);
                }

            }else{

                if(strcmp(message.line, "mem") == 0) {
                    char pcWriteBuffer[500];
                    vTaskList( pcWriteBuffer );
                    message.os->puts(pcWriteBuffer);
                    // message.os->puts("\n\n");
                    // vTaskGetRunTimeStats(pcWriteBuffer);
                    // message.os->puts(pcWriteBuffer);

                    struct mallinfo mi = mallinfo();
                    message.os->printf("\n\nfree malloc memory= %d, free sbrk memory= %d, Total free= %d\n", mi.fordblks, xPortGetFreeHeapSize() - mi.fordblks, xPortGetFreeHeapSize());
                }else{
                    message.os->printf("Got line: %s\n", message.line);
                }

                message.os->puts("ok\n");
            }
        }  else {
            // timed out, flash idle led
            Board_LED_Toggle(1);
            continue;
        }
    }
}

extern "C" void vApplicationTickHook( void )
{
    /* This function will be called by each tick interrupt if
    configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
    added here, but the tick hook is called from an interrupt context, so
    code must not attempt to block, and only the interrupt safe FreeRTOS API
    functions can be used (those that end in FromISR()). */
}

extern "C" void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    taskDISABLE_INTERRUPTS();
    __asm("bkpt #0");
    for( ;; );
}

extern "C" void vApplicationIdleHook( void )
{
    /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
    to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
    task.  It is essential that code added to this hook function never attempts
    to block in any way (for example, call xQueueReceive() with a block time
    specified, or call vTaskDelay()).  If the application makes use of the
    vTaskDelete() API function (as this demo application does) then it is also
    important that vApplicationIdleHook() is permitted to return to its calling
    function, because it is the responsibility of the idle task to clean up
    memory allocated by the kernel to any task that has since been deleted. */
}

extern "C" void vApplicationMallocFailedHook( void )
{
    /* vApplicationMallocFailedHook() will only be called if
    configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
    function that will get called if a call to pvPortMalloc() fails.
    pvPortMalloc() is called internally by the kernel whenever a task, queue,
    timer or semaphore is created.  It is also called by various parts of the
    demo application.  If heap_1.c or heap_2.c are used, then the size of the
    heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
    FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
    to query the size of free heap space that remains (although it does not
    provide information on how the remaining heap might be fragmented). */
    taskDISABLE_INTERRUPTS();
    __asm("bkpt #0");
    for( ;; );
}

int main()   //int argc, char *argv[])
{
    //HAL_NVIC_SetPriorityGrouping( NVIC_PRIORITYGROUP_4 );
    NVIC_SetPriorityGrouping( 0 );

    configureSPIFI(); // full speed ahead

    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();

    // Set up and initialize all required blocks and
    // functions related to the board hardware
    Board_Init();
    // Set the LED to the state of "On"
    Board_LED_Set(0, true);

    printf("MCU clock rate= %lu Hz\n", SystemCoreClock);

    /* LED1 toggle thread */
    xTaskCreate(vLEDTask1, "vTaskLed1", configMINIMAL_STACK_SIZE,
                NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

    xTaskCreate(vRunTestsTask, "vTestsTask", 512, /* *4 as 32bit words */
                NULL, (tskIDLE_PRIORITY + 2UL), (TaskHandle_t *) NULL);

    // create queue for dispatch of lines, can be sent to by several tasks
    dispatch_queue = xQueueCreate( 10, sizeof( dispatch_message_t ) );
    if( dispatch_queue == 0 ) {
        // Failed to create the queue.
        printf("ERROR: failed to create dispatch queue\n");
        __asm("bkpt #0");
    }

    xTaskCreate(vComTask, "vComTask", 256,
                NULL, (tskIDLE_PRIORITY + 4UL), (TaskHandle_t *) NULL);

    xTaskCreate(dispatch, "dispatch", 512, NULL, (tskIDLE_PRIORITY + 3UL), NULL);

    struct mallinfo mi = mallinfo();
    printf("free malloc memory= %d, free sbrk memory= %d, Total free= %d\n", mi.fordblks, xPortGetFreeHeapSize() - mi.fordblks, xPortGetFreeHeapSize());

    /* Start the scheduler */
    vTaskStartScheduler();

    // should never reach here
    __asm("bkpt #0");
}
