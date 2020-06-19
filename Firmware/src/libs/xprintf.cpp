#include <stdio.h>

#include <tuple>

#include "FreeRTOS.h"
#include "semphr.h"

extern "C" {

static QueueHandle_t xPrintMutex;

void setup_xprintf()
{
    xPrintMutex= xSemaphoreCreateMutex();
}

// replace newlib printf, snprintf, vsnprintf
static void my_outchar(void *, char c)
{
    putchar(c);
}

#include "xformatc.h"
int __wrap_printf(const char *fmt, ...)
{
    xSemaphoreTake(xPrintMutex, portMAX_DELAY);
    va_list list;
    unsigned count;

    va_start(list, fmt);
    count = xvformat(my_outchar, 0, fmt, list);
    va_end(list);
    xSemaphoreGive(xPrintMutex);
    return count;
}

using parg_t = std::tuple<char **, size_t, size_t*>;
static void my_stroutchar(void *arg, char c)
{
    parg_t *a= static_cast<parg_t*>(arg);
    char **s = std::get<0>(*a);
    size_t size= std::get<1>(*a);
    size_t *cnt= std::get<2>(*a);
    if(*cnt < size) {
        *(*s)++ = c;
        ++(*cnt);
    }
}

int __wrap_snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list list;
    va_start(list, fmt);
    size_t cnt= 0;
    char *p= str;
    parg_t arg {&p, size-1, &cnt};
    size_t count= xvformat(my_stroutchar, &arg, fmt, list);
    va_end(list);
    str[cnt]= '\0';
    return count;
}

int __wrap_sprintf(char *str, const char *fmt, ...)
{
    va_list list;
    va_start(list, fmt);
    size_t cnt= 0;
    char *p= str;
    parg_t arg {&p, 1000000, &cnt};
    size_t count= xvformat(my_stroutchar, &arg, fmt, list);
    str[cnt]= '\0';
    va_end(list);
    return count;
}

int __wrap_vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    size_t cnt= 0;
    char *p= str;
    parg_t arg {&p, size-1, &cnt};
    size_t count= xvformat(my_stroutchar, &arg, fmt, ap);
    str[cnt]= '\0';
    return count;
}

// int __wrap_vsprintf(char *str, const char *fmt, va_list ap)
// {
//     return __wrap_vsnprintf(str, 1000000, fmt, ap);
// }
}
