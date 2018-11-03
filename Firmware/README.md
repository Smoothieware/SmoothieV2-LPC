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

