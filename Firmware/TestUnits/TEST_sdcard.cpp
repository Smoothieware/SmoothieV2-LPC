#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "ff.h"

#include "FreeRTOS.h"
#include "task.h"

extern "C" bool setup_sdmmc();

static FATFS fatfs; /* File system object */
REGISTER_TEST(SDCardTest, mount)
{
    TEST_ASSERT_TRUE(setup_sdmmc());
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

REGISTER_TEST(SDCardTest, seek)
{
    const char *fn = "/sd/test_file.tst";
    FILE *fp= fopen(fn, "r");
    TEST_ASSERT_NOT_NULL(fp);
    int ret= fseek(fp, 5, SEEK_SET);
    TEST_ASSERT_EQUAL_INT(0, ret);
    char buf[1];
    int n= fread(buf, 1, 1, fp);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT('1', buf[0]);

    ret= fseek(fp, 6, SEEK_CUR);
    TEST_ASSERT_EQUAL_INT(0, ret);
    n= fread(buf, 1, 1, fp);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT('2', buf[0]);

    ret= fseek(fp, -2, SEEK_END);
    TEST_ASSERT_EQUAL_INT(0, ret);
    n= fread(buf, 1, 1, fp);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT('0', buf[0]);

    fclose(fp);
}

REGISTER_TEST(SDCardTest, stat)
{
    // test stat
    struct stat buf;
    int res= stat("/sd/test_file.tst", &buf);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_INT(71, buf.st_size);
    TEST_ASSERT_FALSE(S_ISDIR(buf.st_mode));
    TEST_ASSERT_TRUE(S_ISREG(buf.st_mode));
}

REGISTER_TEST(SDCardTest, rename)
{
    // delete new if it was there
    unlink("/sd/test_file_2.tst");

    int res= rename("/sd/test_file.tst", "/sd/test_file_2.tst");
    TEST_ASSERT_EQUAL_INT(0, res);
    struct stat buf;
    res= stat("/sd/test_file.tst", &buf);
    TEST_ASSERT_EQUAL_INT(-1, res);
    res= stat("/sd/test_file_2.tst", &buf);
    TEST_ASSERT_EQUAL_INT(0, res);
}

REGISTER_TEST(SDCardTest, errors)
{
    FILE *fp = fopen("no_such_file.xyz", "r");
    TEST_ASSERT_NULL(fp);
    TEST_ASSERT_EQUAL_INT(ENOENT, errno);
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
    TEST_ASSERT_EQUAL_INT(FR_OK, f_closedir(&dir));

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

REGISTER_TEST(SDCardTest, read_config_init)
{
    TEST_IGNORE();
}

using systime_t= uint32_t;
#define clock_systimer() ((systime_t)Chip_RIT_GetCounter(LPC_RITIMER))
#define TICK2USEC(x) ((systime_t)(((uint64_t)(x)*1000000)/timerFreq))

REGISTER_TEST(SDCardTest, time_read_write)
{
    /* Get RIT timer peripheral clock rate */
    uint32_t timerFreq = Chip_Clock_GetRate(CLK_MX_RITIMER);
    #if 0
    printf("RITIMER clock rate= %lu\n", timerFreq);
    {
        systime_t st = clock_systimer();
        vTaskDelay(pdMS_TO_TICKS(1000));
        systime_t en = clock_systimer();
        printf("1 second delay= %lu us\n", TICK2USEC(en-st));
    }
    #endif

    printf("Starting timing tests....\n");

    char fn[64];
    strcpy(fn, "/sd/test_large_file.tst");

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
    printf("elapsed time %lu us for writing %lu bytes, %1.4f bytes/sec\n", TICK2USEC(en - st), n * 512, (n * 512.0F) / (TICK2USEC(en - st) / 1e6F));

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
    printf("elapsed time %lu us for reading %lu bytes, %1.4f bytes/sec\n", TICK2USEC(en - st), n * 512, (n * 512.0F) / (TICK2USEC(en - st) / 1e6F));

    fclose(fp);
}

REGISTER_TEST(SDCardTest, unmount)
{
    int ret = f_unmount("sd");
    TEST_ASSERT_EQUAL_INT(FR_OK, ret);
}
