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
#include "MessageQueue.h"

// // place holder
bool dispatch_line(OutputStream& os, const char *line)
{
    return true;
}

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

void safe_sleep(uint32_t ms)
{
    // here we need to sleep (and yield) for 10ms then check if we need to handle the query command
    TickType_t delayms= pdMS_TO_TICKS(10); // 10 ms sleep
    while(ms > 0) {
        vTaskDelay(delayms);
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

extern "C" size_t write_cdc(const char *buf, size_t len);
extern "C" size_t read_cdc(char *buf, size_t len);
extern "C" int setup_cdc(void *taskhandle);

extern "C" void usbComTask(void *pvParameters)
{
    static OutputStream theos([](const char *buf, size_t len){ return write_cdc(buf, len); });
    static char linebuf[MAX_LINE_LENGTH];
    static size_t linecnt;
    static char rxBuff[256];

    // setup the USB CDC and give it the handle of our task to wake up when we get an interrupt
    setup_cdc(xTaskGetCurrentTaskHandle());

    const TickType_t waitms = pdMS_TO_TICKS( 300 );
    bool first= true;
    uint32_t timeouts= 0;
    linecnt= 0;
    bool discard= false;

    while (1) {
        // Wait to be notified that there has been a USB irq.
        uint32_t ulNotificationValue = ulTaskNotifyTake( pdTRUE, waitms );

        if( ulNotificationValue != 1 ) {
            /* The call to ulTaskNotifyTake() timed out. */
            timeouts++;
        }

        if(first) {
            // wait for first character
            int rdCnt = read_cdc(rxBuff, sizeof(rxBuff));
            if(rdCnt > 0) {
                for (int i = 0; i < rdCnt; ++i) {
                    if(rxBuff[i] == '\n') {
                        first= false;
                    }
                }
                if(!first) {
                    write_cdc("Welcome to Smoothev2\r\n", 22);
                }
            }

        }else{
            // we read as much as we can, process it into lines and send it to the dispatch thread
            // certain characters are sent immediately the rest wait for end of line
            size_t rdCnt = read_cdc(rxBuff, sizeof(rxBuff));
            for (size_t i = 0; i < rdCnt; ++i) {
                linebuf[linecnt]= rxBuff[i];

                // the following are single character commands that are dispatched immediately
                if(linebuf[linecnt] == 24) { // ^X
                    // discard all recieved data
                    linebuf[linecnt+1]= '\0'; // null terminate
                    send_message_queue(&linebuf[linecnt], &theos);
                    linecnt= 0;
                    discard= false;
                    break;
                } else if(linebuf[linecnt] == '?') {
                    linebuf[linecnt+1]= '\0'; // null terminate
                    send_message_queue(&linebuf[linecnt], &theos);
                } else if(linebuf[linecnt] == '!') {
                    linebuf[linecnt+1]= '\0'; // null terminate
                    send_message_queue(&linebuf[linecnt], &theos);
                } else if(linebuf[linecnt] == '~') {
                    linebuf[linecnt+1]= '\0'; // null terminate
                    send_message_queue(&linebuf[linecnt], &theos);
                // end of immediate commands

                } else if(discard) {
                    // we discard long lines until we get the newline
                    if(linebuf[linecnt] == '\n') discard = false;

                } else if(linecnt >= sizeof(linebuf) - 1) {
                    // discard long lines
                    discard = true;
                    linecnt = 0;

                } else if(linebuf[linecnt] == '\n') {
                    linebuf[linecnt] = '\0'; // remove the \n and nul terminate
                    send_message_queue(linebuf, &theos);
                    linecnt= 0;

                } else if(linebuf[linecnt] == '\r') {
                    // ignore CR
                    continue;

                } else if(linebuf[linecnt] == 8 || linebuf[linecnt] == 127) { // BS or DEL
                    if(linecnt > 0) --linecnt;

                } else {
                    ++linecnt;
                }
           }
       }
   }
}

// this would be the command thread in the firmware
extern "C" void dispatch(void *pvParameters)
{
    char *line;
    OutputStream *os;

    while(1) {
        // now read lines and dispatch them
        if( receive_message_queue(&line, &os) ) {
            // got line
            if(strlen(line) == 1) {
                switch(line[0]) {
                    case 24: os->printf("Got KILL\n"); break;
                    case '?': os->printf("Got Query\n"); break;
                    case '!': os->printf("Got Hold\n"); break;
                    case '~': os->printf("Got Release\n"); break;
                    default: os->printf("Got 1 char line: %s\n", line);
                }

            }else{

                if(strcmp(line, "mem") == 0) {
                    char pcWriteBuffer[500];
                    vTaskList( pcWriteBuffer );
                    os->puts(pcWriteBuffer);
                    // os->puts("\n\n");
                    // vTaskGetRunTimeStats(pcWriteBuffer);
                    // os->puts(pcWriteBuffer);

                    struct mallinfo mi = mallinfo();
                    os->printf("\n\nfree malloc memory= %d, free sbrk memory= %d, Total free= %d\n", mi.fordblks, xPortGetFreeHeapSize() - mi.fordblks, xPortGetFreeHeapSize());
                }else{
                    os->printf("Got line: %s\n", line);
                }

                os->puts("ok\n");
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

    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();

    // Set up and initialize all required blocks and
    // functions related to the board hardware
    Board_Init();
    // Set the LED to the state of "On"
    Board_LED_Set(0, true);

    configureSPIFI(); // full speed ahead

    printf("MCU clock rate= %lu Hz\n", SystemCoreClock);

    /* LED1 toggle thread */
    xTaskCreate(vLEDTask1, "vTaskLed1", configMINIMAL_STACK_SIZE,
                NULL, (tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

    xTaskCreate(vRunTestsTask, "vTestsTask", 512, /* *4 as 32bit words */
                NULL, (tskIDLE_PRIORITY + 2UL), (TaskHandle_t *) NULL);

    // create queue for dispatch of lines, can be sent to by several tasks
    if(!create_message_queue()) {
        // Failed to create the queue.
        printf("ERROR: failed to create dispatch queue\n");
        __asm("bkpt #0");
    }

    xTaskCreate(usbComTask, "usbComTask", 256, NULL, (tskIDLE_PRIORITY + 4UL), (TaskHandle_t *) NULL);
    xTaskCreate(dispatch, "dispatch", 512, NULL, (tskIDLE_PRIORITY + 3UL), NULL);

    struct mallinfo mi = mallinfo();
    printf("free malloc memory= %d, free sbrk memory= %d, Total free= %d\n", mi.fordblks, xPortGetFreeHeapSize() - mi.fordblks, xPortGetFreeHeapSize());

    /* Start the scheduler */
    vTaskStartScheduler();

    // should never reach here
    __asm("bkpt #0");
}
