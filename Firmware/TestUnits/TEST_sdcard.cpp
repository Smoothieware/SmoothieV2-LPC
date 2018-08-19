#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Start of support routines for sdmmc, need sto go into a support file of some sort */
#include "board.h"
#include "chip.h"
#include "rtc.h"
#include "ff.h"

/* SDMMC card info structure */
mci_card_struct sdcardinfo;

static volatile int32_t sdio_wait_exit = 0;

/* Delay callback for timed SDIF/SDMMC functions */
static void sdmmc_waitms(uint32_t time)
{
    /* In an RTOS, the thread would sleep allowing other threads to run.
       For standalone operation, we just spin on RI timer */
    int32_t curr = (int32_t) Chip_RIT_GetCounter(LPC_RITIMER);
    int32_t final = curr + ((SystemCoreClock / 1000) * time);

    if (final == curr) return;

    if ((final < 0) && (curr > 0)) {
        while (Chip_RIT_GetCounter(LPC_RITIMER) < (uint32_t) final) {}
    } else {
        while ((int32_t) Chip_RIT_GetCounter(LPC_RITIMER) < final) {}
    }

    return;
}

/**
 * @brief   Sets up the SD event driven wakeup
 * @param   bits : Status bits to poll for command completion
 * @return  Nothing
 */
static void sdmmc_setup_wakeup(void *bits)
{
    uint32_t bit_mask = *((uint32_t *)bits);
    /* Wait for IRQ - for an RTOS, you would pend on an event here with a IRQ based wakeup. */
    NVIC_ClearPendingIRQ(SDIO_IRQn);
    sdio_wait_exit = 0;
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

    /* Wait for event, would be nice to have a timeout, but keep it  simple */
    while (sdio_wait_exit == 0) {}

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
extern "C" void SDIO_IRQHandler(void)
{
    /* All SD based register handling is done in the callback
       function. The SDIO interrupt is not enabled as part of this
       driver and needs to be enabled/disabled in the callbacks or
       application as needed. This is to allow flexibility with IRQ
       handling for applicaitons and RTOSes. */
    /* Set wait exit flag to tell wait function we are ready. In an RTOS,
       this would trigger wakeup of a thread waiting for the IRQ. */
    NVIC_DisableIRQ(SDIO_IRQn);
    sdio_wait_exit = 1;
}

/* ----------- End of support code ------ */


static FATFS fatfs; /* File system object */
REGISTER_TEST(SDCardTest, mount)
{
    App_SDMMC_Init();
    //rtc_initialize();

    NVIC_DisableIRQ(SDIO_IRQn);
    /* Enable SD/MMC Interrupt */
    NVIC_EnableIRQ(SDIO_IRQn);

    TEST_ASSERT_EQUAL_INT(FR_OK, f_mount(&fatfs, "sd", 1));
}

REGISTER_TEST(SDCardTest, raw_read_write)
{
    int ret;
    char fn[64];
    strcpy(fn, "/sd/test_file.raw");

    // delete it if it was there
    ret = f_unlink(fn);
    //TEST_ASSERT_EQUAL_INT(FR_OK, ret);

    FIL fp;  /* File object */

    ret = f_open(&fp, fn, FA_WRITE | FA_CREATE_ALWAYS); // fopen(fn, "w");
    TEST_ASSERT_EQUAL_INT(FR_OK, ret);

    for (int i = 1; i <= 10; ++i) {
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "Line %d\n", i);
        unsigned x;
        ret = f_write(&fp, buf, n, &x);
        TEST_ASSERT_EQUAL_INT(FR_OK, ret);
        TEST_ASSERT_EQUAL_INT(n, x);
    }

    f_close(&fp);

    // Open file
    ret = f_open(&fp, fn, FA_READ);
    TEST_ASSERT_EQUAL_INT(FR_OK, ret);

    // check each line of the file
    for (int i = 1; i <= 10; ++i) {
        TEST_ASSERT_TRUE(f_eof(&fp) == 0);
        char buf[32];
        char *l = f_gets(buf, sizeof(buf), &fp);
        TEST_ASSERT_NOT_NULL(l);
        printf("test: %s", buf);
        // now verify
        char vbuf[32];
        int n = snprintf(vbuf, sizeof(vbuf), "Line %d\n", i);
        TEST_ASSERT_EQUAL_INT(0, strncmp(buf, vbuf, n));
        TEST_ASSERT_TRUE(f_error(&fp) == 0);
    }

    TEST_ASSERT_TRUE(f_eof(&fp) != 0);

    ret = f_close(&fp);
    TEST_ASSERT_EQUAL_INT(FR_OK, ret);
}

// using posix calls
REGISTER_TEST(SDCardTest, write_read)
{
    char fn[64];
    strcpy(fn, "/sd/test_file.tst");

    // delete it if it was there
    unlink(fn);

    FILE *fp;
    fp = fopen(fn, "w");
    TEST_ASSERT_NOT_NULL(fp);

    for (int i = 1; i <= 10; ++i) {
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "Line %d\n", i);
        int x = fwrite(buf, 1, n, fp);
        TEST_ASSERT_EQUAL_INT(n, x);
    }

    fclose(fp);

    // Open file
    fp = fopen(fn, "r");
    TEST_ASSERT_NOT_NULL(fp);

    // check each line of the file
    for (int i = 1; i <= 10; ++i) {
        TEST_ASSERT_TRUE(!feof(fp));
        char buf[32];
        char *l = fgets(buf, sizeof(buf), fp);
        TEST_ASSERT_NOT_NULL(l);
        printf("test: %s", buf);
        // now verify
        char vbuf[32];
        int n = snprintf(vbuf, sizeof(vbuf), "Line %d\n", i);
        TEST_ASSERT_EQUAL_INT(0, strncmp(buf, vbuf, n));
    }
    fclose(fp);
}

REGISTER_TEST(SDCardTest, directory)
{
#if 1
    // newlib does not support dirent so use ff lib directly
    DIR dir;
    FILINFO finfo;
    FATFS *fs;
    FRESULT res = f_opendir(&dir, "/sd");
    TEST_ASSERT_EQUAL_INT(FR_OK, res);
    DWORD p1, s1, s2;
    p1 = s1 = s2 = 0;
    for(;;) {
        res = f_readdir(&dir, &finfo);
        if ((res != FR_OK) || !finfo.fname[0]) break;
        if (finfo.fattrib & AM_DIR) {
            s2++;
        } else {
            s1++; p1 += finfo.fsize;
        }
        printf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %s\n",
                (finfo.fattrib & AM_DIR) ? 'D' : '-',
                (finfo.fattrib & AM_RDO) ? 'R' : '-',
                (finfo.fattrib & AM_HID) ? 'H' : '-',
                (finfo.fattrib & AM_SYS) ? 'S' : '-',
                (finfo.fattrib & AM_ARC) ? 'A' : '-',
                (finfo.fdate >> 9) + 1980, (finfo.fdate >> 5) & 15, finfo.fdate & 31,
                (finfo.ftime >> 11), (finfo.ftime >> 5) & 63,
                (DWORD)finfo.fsize, finfo.fname);
    }
    printf("%4lu File(s),%10lu bytes total\n%4lu Dir(s)", s1, p1, s2);
    res = f_getfree("/sd", (DWORD*)&p1, &fs);
    TEST_ASSERT_EQUAL_INT(FR_OK, res);
    printf(", %10lu bytes free\n", p1 * fs->csize * 512);

#else
    DIR *dirp;

    /* Open the directory */
    dirp = opendir("/sd");
    TEST_ASSERT_NOT_NULL(dirp);

    /* Read each directory entry */
    int cnt = 0;
    for (; ; ) {
        struct dirent *entryp = readdir(dirp);
        if (entryp == NULL) {
            /* Finished with this directory */
            break;
        }

        printf("%s\n", entryp->d_name);
        cnt++;
    }
    closedir(dirp);
    TEST_ASSERT_TRUE(cnt > 0);
#endif
}

#if 0
REGISTER_TEST(SDCardTest, read_config_init)
{
    TEST_IGNORE();
}


REGISTER_TEST(SDCardTest, time_read_write)
{
    char fn[64];
    strcpy(fn, g_target);
    strcat(fn, "/test_large_file.tst");

    // delete it if it was there
    unlink(fn);

    FILE *fp;
    fp = fopen(fn, "w");
    TEST_ASSERT_NOT_NULL(fp);

    systime_t st = clock_systimer();

    uint32_t n = 5000;
    for (uint32_t i = 1; i <= n; ++i) {
        char buf[512];
        size_t x = fwrite(buf, 1, sizeof(buf), fp);
        if(x != sizeof(buf)) {
            TEST_FAIL();
        }
    }

    systime_t en = clock_systimer();
    printf("elapsed time %d us for writing %d bytes, %1.4f bytes/sec\n", TICK2USEC(en - st), n * 512, (n * 512.0F) / (TICK2USEC(en - st) / 1e6F));

    fclose(fp);

    // Open file
    fp = fopen(fn, "r");
    TEST_ASSERT_NOT_NULL(fp);

    // read back data
    st = clock_systimer();
    for (uint32_t i = 1; i <= n; ++i) {
        char buf[512];
        size_t x = fread(buf, 1, sizeof(buf), fp);
        if(x != sizeof(buf)) {
            TEST_FAIL();
        }
    }
    en = clock_systimer();
    printf("elapsed time %d us for reading %d bytes, %1.4f bytes/sec\n", TICK2USEC(en - st), n * 512, (n * 512.0F) / (TICK2USEC(en - st) / 1e6F));

    fclose(fp);
}
#endif

REGISTER_TEST(SDCardTest, unmount)
{
    int ret = f_unmount("sd");
    TEST_ASSERT_EQUAL_INT(FR_OK, ret);
}
