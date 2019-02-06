This implements a SPIFI flash loader for the command flash.

It needs to be compiled separately then flashed to the spifi at 0x14700000.
(I use JFlashLiteExe an dflash the flashloader.srec as it has the flash address in the file).

If it is not there then the flash comamnd will spit out an error.

This will flash a file called flashme.bin on the sdcard then reset.

flashme.bin needs to be the firmware build in binary compiled for address 0x14000000.
