/* Includes */
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <fcntl.h>

#include "board_api.h"

/*
 * Map newlib calls to fflib
 */

#define __debugbreak()  { __asm volatile ("bkpt #0"); }

int _write(int file, char *buffer, int length)
{
    if(file < 3) {
        // Note this will block until all sent
        for (int i = 0; i < length; ++i) {
            Board_UARTPutChar(buffer[i]);
        }
    }

    return length;
}

int _read(int file, char *buffer, int length)
{
    if(file < 3) {
        // Note this can return less than request or even 0
        for (int i = 0; i < length; ++i) {
            buffer[i]= Board_UARTGetChar();
        }

    }

    return length;
}

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

int _close(int file)
{
    return 0;
}

int _fstat(int file, struct stat *st)
{
    return 0;
}

int _stat(char *file, struct stat *st)
{
    return 0;
}

int _isatty(int file)
{
	return (file >= 0 || file <=2) ? 1 : 0;
}

int _lseek(int file, int position, int whence)
{
    return 0;
}


int _wait(int *status)
{
	errno = ECHILD;
	return -1;
}

int _unlink(char *name)
{
	return 0;
}

int _times(struct tms *buf)
{
	return -1;
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
    extern char __HeapBase; /* Defined by the linker */
    static char *heap_end= 0;
    char *prev_heap_end;
    if (heap_end == 0) {
        heap_end = &__HeapBase;
    }
    prev_heap_end = heap_end;
    heap_end += incr;
    return (caddr_t) prev_heap_end;
}
