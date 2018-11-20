#include <stdlib.h>
#include <stdio.h>

void *operator new(size_t size)
{
    return malloc(size);
}

void *operator new[](size_t size)
{
    return malloc(size);
}

__attribute__ ((weak)) void operator delete(void *p)
{
    free(p);
}

__attribute__ ((weak)) void operator delete[](void *p)
{
    free(p);
}

extern "C" int __aeabi_atexit(void *object,
		void (*destructor)(void *),
		void *dso_handle)
{
	return 0;
}

#ifdef CPP_NO_HEAP
extern "C" void *malloc(size_t) {
	return (void *)0;
}

extern "C" void free(void *) {
}
#endif

#ifndef CPP_USE_CPPLIBRARY_TERMINATE_HANDLER
/******************************************************************
 * __verbose_terminate_handler()
 *
 * This is the function that is called when an uncaught C++
 * exception is encountered. The default version within the C++
 * library prints the name of the uncaught exception, but to do so
 * it must demangle its name - which causes a large amount of code
 * to be pulled in. The below minimal implementation can reduce
 * code size noticeably. Note that this function should not return.
 ******************************************************************/
namespace __gnu_cxx {
void __verbose_terminate_handler()
{
  ::puts("FATAL: Uncaught exception\n");
  __asm("bkpt #0");
  while(1) ;
}
}
#endif
