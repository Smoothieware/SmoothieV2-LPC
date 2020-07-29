This implements a SPIFI flash loader for the command flash.
This will flash a file called flashme.bin on the sdcard then reset.
This is complied to run in RAM at 0x1000000 and is inserted in the main firmware build. When the flash command (or a dfu download) is executed
it is run to flash the SPIFI.
This binary image can also be used to flash a bricked device using the LPC4330 ROM based flash utilities, either over the serial UART or USB.
https://github.com/Smoothieware/SmoothieV2/tree/master/Firmware#debugging-and-flashing
