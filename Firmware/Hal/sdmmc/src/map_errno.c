/*
 This code copied from...
 Copyright &copy; 2014-2017 Mike Gore, All rights reserved. GPL  License
 @see http://github.com/magore/hp85disk
 @see http://github.com/magore/hp85disk/COPYRIGHT.md for specific Copyright details
 @par You are free to use this code under the terms of GPL
   please retain a copy of this notice in any code you use it in.
*/
#include "ff.h"
#include <errno.h>

int fatfs_to_errno( FRESULT Result )
{
	switch( Result )
	{
		case FR_OK:              /* FatFS (0) Succeeded */
			return (0);          /* POSIX OK */
		case FR_DISK_ERR:        /* FatFS (1) A hard error occurred in the low level disk I/O layer */
			return (EIO);        /* POSIX Input/output error (POSIX.1) */

		case FR_INT_ERR:         /* FatFS (2) Assertion failed */
			return (EPERM);      /* POSIX Operation not permitted (POSIX.1) */

		case FR_NOT_READY:       /* FatFS (3) The physical drive cannot work */
			return (EBUSY);      /* POSIX Device or resource busy (POSIX.1) */

		case FR_NO_FILE:         /* FatFS (4) Could not find the file */
			return (ENOENT);     /* POSIX No such file or directory (POSIX.1) */

		case FR_NO_PATH:         /* FatFS (5) Could not find the path */
			return (ENOENT);     /* POSIX No such file or directory (POSIX.1) */

		case FR_INVALID_NAME:    /* FatFS (6) The path name format is invalid */
			return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

		case FR_DENIED:          /* FatFS (7) Access denied due to prohibited access or directory full */
			return (EACCES);     /* POSIX Permission denied (POSIX.1) */
		case FR_EXIST:           /* FatFS (8) Access denied due to prohibited access */
			return (EACCES);     /* POSIX Permission denied (POSIX.1) */

		case FR_INVALID_OBJECT:  /* FatFS (9) The file/directory object is invalid */
			return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

		case FR_WRITE_PROTECTED: /* FatFS (10) The physical drive is write protected */
			return(EROFS);       /* POSIX Read-only filesystem (POSIX.1) */

		case FR_INVALID_DRIVE:   /* FatFS (11) The logical drive number is invalid */
			return(ENXIO);       /* POSIX No such device or address (POSIX.1) */

		case FR_NOT_ENABLED:     /* FatFS (12) The volume has no work area */
			return (ENOSPC);     /* POSIX No space left on device (POSIX.1) */

		case FR_NO_FILESYSTEM:   /* FatFS (13) There is no valid FAT volume */
			return(ENXIO);       /* POSIX No such device or address (POSIX.1) */

		case FR_MKFS_ABORTED:    /* FatFS (14) The f_mkfs() aborted due to any parameter error */
			return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

		case FR_TIMEOUT:         /* FatFS (15) Could not get a grant to access the volume within defined period */
			return (EBUSY);      /* POSIX Device or resource busy (POSIX.1) */

		case FR_LOCKED:          /* FatFS (16) The operation is rejected according to the file sharing policy */
			return (EBUSY);      /* POSIX Device or resource busy (POSIX.1) */


		case FR_NOT_ENOUGH_CORE: /* FatFS (17) LFN working buffer could not be allocated */
			return (ENOMEM);     /* POSIX Not enough space (POSIX.1) */

		case FR_TOO_MANY_OPEN_FILES:/* FatFS (18) Number of open files > _FS_SHARE */
			return (EMFILE);     /* POSIX Too many open files (POSIX.1) */

		case FR_INVALID_PARAMETER:/* FatFS (19) Given parameter is invalid */
			return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

	}
	return (EBADMSG);            /* POSIX Bad message (POSIX.1) */
}
