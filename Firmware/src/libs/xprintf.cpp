#include <stdio.h>

#include <tuple>
#include <sstream>

#include "FreeRTOS.h"
#include "semphr.h"

using parg_t = std::tuple<char **, size_t, size_t*>;
using farg_t = std::tuple<std::string*, FILE*, bool*>;
static QueueHandle_t xPrintMutex;

extern "C" {
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

int __wrap_vsprintf(char *str, const char *fmt, va_list ap)
{
    return __wrap_vsnprintf(str, 1000000, fmt, ap);
}

#if 0
// this should work but is untested
static void my_foutchar(void *arg, char c)
{
    farg_t *a= static_cast<farg_t*>(arg);
    std::string *b= std::get<0>(*a);
    FILE *fp= std::get<1>(*a);
    bool *err= std::get<2>(*a);
    if(*err) return;

    b->push_back(c);
    size_t n= b->size();
    if(n >= 1024) {
        if(fwrite(b->data(), 1, n, fp) != n) {
            *err= true;
            return;
        }
        b->clear();
    }
}

int __wrap_fprintf(FILE *fp, const char *fmt, ...)
{
    va_list list;
    unsigned count;
    std::string buf;
    va_start(list, fmt);
    bool err= false;
    farg_t arg {&buf, fp, &err};
    count = xvformat(my_foutchar, &arg, fmt, list);
    va_end(list);
    if(err) return -1;

    size_t n= buf.size();
    if(n > 0) {
        if(fwrite(buf.data(), 1, n, fp) != n) {
            err= true;
        }
    }

    return err ? -1 : count;
}
#endif

}
