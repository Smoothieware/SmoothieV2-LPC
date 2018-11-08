#include "board.h"
#include "chip.h"
#include "rtc.h"
#include "ff.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

/* SDMMC card info structure */
mci_card_struct sdcardinfo;

/* Delay callback for timed SDIF/SDMMC functions */
static void sdmmc_waitms(uint32_t time)
{
    /* In an RTOS, the thread would sleep allowing other threads to run. */
    vTaskDelay(pdMS_TO_TICKS(time));
    return;
}

/**
 * @brief   Sets up the SD event driven wakeup
 * @param   bits : Status bits to poll for command completion
 * @return  Nothing
 */
static xTaskHandle xTaskToNotify = NULL;
static void sdmmc_setup_wakeup(void *bits)
{
    // I presume that this will be called from the thread that will also be waiting
    // If not we need a binary mutex
    xTaskToNotify= xTaskGetCurrentTaskHandle();

    uint32_t bit_mask = *((uint32_t *)bits);
    /* Wait for IRQ - for an RTOS, you would pend on an event here with a IRQ based wakeup. */
    NVIC_ClearPendingIRQ(SDIO_IRQn);
    Chip_SDIF_SetIntMask(LPC_SDMMC, bit_mask);
    NVIC_EnableIRQ(SDIO_IRQn);
}

/**
 * @brief   A better wait callback for SDMMC driven by the IRQ flag
 * @return  0 on success, or failure condition (-1)
 */
static uint32_t sdmmc_irq_driven_wait(void)
{
    uint32_t status;
    const TickType_t waitms = pdMS_TO_TICKS( 10000 );

    // sanity check may be removed eventually
    if(xTaskGetCurrentTaskHandle() != xTaskToNotify) {
        puts("We have a problem that this was a differnt thread than expected\n");
        __asm("bkpt #0");
    }

    /* Wait for event */
    uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, waitms);
    if( ulNotificationValue != 1 ) {
        /* The call to ulTaskNotifyTake() timed out. */
        return -1;
    }

    /* Get status and clear interrupts */
    status = Chip_SDIF_GetIntStatus(LPC_SDMMC);
    Chip_SDIF_ClrIntStatus(LPC_SDMMC, status);
    Chip_SDIF_SetIntMask(LPC_SDMMC, 0);

    return status;
}

/* Initialize SD/MMC */
static void App_SDMMC_Init()
{
    memset(&sdcardinfo, 0, sizeof(sdcardinfo));
    sdcardinfo.card_info.evsetup_cb = sdmmc_setup_wakeup;
    sdcardinfo.card_info.waitfunc_cb = sdmmc_irq_driven_wait;
    sdcardinfo.card_info.msdelay_func = sdmmc_waitms;

    /*  SD/MMC initialization */
    Board_SDMMC_Init();

    /* The SDIO driver needs to know the SDIO clock rate */
    Chip_SDIF_Init(LPC_SDMMC);
}

/**
 * @brief   SDIO controller interrupt handler
 * @return  Nothing
 */
void SDIO_IRQHandler(void)
{
    /* All SD based register handling is done in the callback
       function. The SDIO interrupt is not enabled as part of this
       driver and needs to be enabled/disabled in the callbacks or
       application as needed. This is to allow flexibility with IRQ
       handling for applicaitons and RTOSes. */
    /* Set wait exit flag to tell wait function we are ready. In an RTOS,
       this would trigger wakeup of a thread waiting for the IRQ. */
    NVIC_DisableIRQ(SDIO_IRQn);

    static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    /* Notify the task that the transmission is complete. */
    vTaskNotifyGiveFromISR( xTaskToNotify, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

bool setup_sdmmc()
{
    App_SDMMC_Init();
    //rtc_initialize();

    NVIC_DisableIRQ(SDIO_IRQn);
    NVIC_SetPriority(SDIO_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);

    /* Enable SD/MMC Interrupt */
    NVIC_EnableIRQ(SDIO_IRQn);

    return true;
}

void shutdown_sdmmc()
{
    NVIC_DisableIRQ(SDIO_IRQn);
    Chip_SDIF_DeInit(LPC_SDMMC);
}
