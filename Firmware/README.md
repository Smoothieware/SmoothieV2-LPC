To build the test cases do ```rake testing=1 -m```
then flash to Bambino, the results print out to the uart/serial

To make the Firmware do ```rake target=Bambino -m```

Firmware currently runs on UART at 115200 baud and on USB serial at ttyACM0.
On USB Serial you need to hit Enter to get it to start.

The config file is called config.ini on the sdcard and examples are shown in the ConfigSamples diretory, config-3d.ini is for a 3d printer, and config-laser.ini is for laser, these would be renamed config.ini and copied to the sdcard.

The config.ini may also be builtin and is defined in string-config-bambino.h, a #define is needed in the main.cpp to use the builtin config.ini.

Currently the max stepping rate is limited to 100Khz as this seems the upper limit to handle the 10us interrupt.

Enough modules have been ported to run a 3D printer, also a laser is supported.

Modules that have been ported so far...

* endstops
* extruder
* laser
* switch
* temperaturecontrol
* temperatureswitch
* zprobe
* currentcontrol
* killbutton
* player

*NOTE* for the smooothiev2 Prime Alpha replace Bambino above with Primealpha...

```rake target=Primealpha -m```

builtin config would be called string-config-minialpha.h (but the default is to read the config.ini on sdcard).

The Prime Alpha is currently working quite well.

on the Prime Alpha there are 4 leds..

1. led3 - smoothie led, flashes slowly when idle, does not flash when busy
2. led4 - smoothie led, on when executing moves, flashes when in HALT

The debug UART port is on PF.10 (TX) and PF.11 (RX) on the PrimeAlpha
The debug UART port is on P6.4 (TX) and P6.5 (RX) on the Bambino Socket 2 pin 4,5

Debugging and Flashing
----------------------
You will need a JLink to flash and debug, plug it into the jtag port.
Run the jlink gdb server:
```/opt/jlink/JLinkGDBServer -device LPC4330_M4 -speed auto -rtos GDBServer/RTOSPlugin_FreeRTOS.so -timeout 10000```

Then run gdb:
```arm-none-eabi-gdb-64bit -ex "target remote localhost:2331" smoothiev2_Primealpha/smoothiev2.elf```

The ```arm-none-eabi-gdb-64bit``` binary is in the tools directory, it is a fixed version that handles Hard Faults correctly.

To flash use the load command in gdb, it is recommended you do a ```mon reset``` before and after the load.

Once flashed you use the c command to run.

Flashing V2 Smoothie using J-Link (Windows)
===========================================

Install and launch Jflash Lite
Set to LPC4330_M4, JTAG 4000khz
Select V2 smoothie bin file
Set Address to 0x14000000
Click Program Device
Install SD with config.ini into V2 board
Plug into USB and boot.  Verify serial connection

Contributions
=============

Pull requests to the master branch will not be accepted as this will be the stable, well tested branch.
Pull requests should be made to the unstable branch, which is a branch off the edge branch.
Once a PR has been merged into unstable and has been fairly well tested it will be merged to the edge branch.
Only when edge is considered stable will it be merged into master.

