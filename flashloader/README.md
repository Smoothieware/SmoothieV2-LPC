This implements a SPIFI flash loader for the command flash.
This will flash a file called flashme.bin on the sdcard then reset.
This is complied to run in RAM at 0x1000000 and is inserted in the main firmware build. When the flash command (or a dfu download) is executed
it is run to flash the SPIFI.
