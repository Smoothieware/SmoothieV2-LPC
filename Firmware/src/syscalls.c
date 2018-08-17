/* Includes */
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>

#include "board.h"

#define __debugbreak()  { __asm volatile ("bkpt #0"); }

/* Variables */
#undef errno
extern int errno;

char *__env[1] = { 0 };
char **environ = __env;

int _getpid(void)
{
	return 1;
}

int _kill(int pid, int sig)
{
	errno = EINVAL;
	return -1;
}

void _exit (int status)
{
	_kill(status, -1);
	while (1) {}		/* Make sure we hang here */
}

#if 0
int _write(int iFileHandle, char *pcBuffer, int iLength)
{
#if defined(DEBUG_ENABLE)
        unsigned int i;
        for (i = 0; i < iLength; i++) {
                Board_UARTPutChar(pcBuffer[i]);
        }
#endif

        return iLength;
}

/* Called by bottom level of scanf routine within RedLib C library to read
   a character. With the default semihosting stub, this would read the character
   from the debugger console window (which acts as stdin). But this version reads
   the character from the LPC1768/RDB1768 UART. */
int _read(void)
{
#if defined(DEBUG_ENABLE)
        int c = Board_UARTGetChar();
        return c;

#else
        return (int) -1;
#endif
}
#endif

int _close(int file)
{
	return -1;
}

int _fstat(int file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int file)
{
	return 1;
}

int _lseek(int file, int ptr, int dir)
{
	return 0;
}

int _open(char *path, int flags, ...)
{
	/* Pretend like we always fail */
	return -1;
}

int _wait(int *status)
{
	errno = ECHILD;
	return -1;
}

int _unlink(char *name)
{
	errno = ENOENT;
	return -1;
}

int _times(struct tms *buf)
{
	return -1;
}

int _stat(char *file, struct stat *st)
{
	st->st_mode = S_IFCHR;
	return 0;
}

int _link(char *old, char *new)
{
	errno = EMLINK;
	return -1;
}

int _fork(void)
{
	errno = EAGAIN;
	return -1;
}

int _execve(char *name, char **argv, char **env)
{
	errno = ENOMEM;
	return -1;
}

extern caddr_t _sbrk(int incr);
caddr_t _sbrk(int incr)
{
    extern char _pvHeapStart; /* Defined by the linker */
    static char *heap_end= 0;
    char *prev_heap_end;
    if (heap_end == 0) {
        heap_end = &_pvHeapStart;
    }
    prev_heap_end = heap_end;
    char *stack=  (char *)__get_MSP();
    if (heap_end + incr >= stack) {
        //write (1, "Heap and stack collision\n", 25);
        __debugbreak();
        abort ();
    }
    heap_end += incr;
    return (caddr_t) prev_heap_end;
}

