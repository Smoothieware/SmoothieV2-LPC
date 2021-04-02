# smoothie version 2
Smoothie V2 using LPCopen and FreeRTOS.

Currently runs on Bambino boards and V2 Prime Alpha proto boards.

It is currently being used to control a Delta 3D printer, and has been used to control a CNC laser.

Firmware/... is for Smoothie firmware code and Test Units

Currently uses the following toolchain..

gcc version 9.2.1 20191025
(or any 7.x.x thru 10.x.x will probably work).

To get the tool chain you should do the following on Ubuntu based Linuxes...

    sudo apt install gcc-arm-none-eabi

or for Debian Stretch (and Ubuntu) get the tar from here...
    https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads

    Then detar to a directory and do...
        export ARMTOOLS=/downloaddir/gcc-arm-none-eabi-{version}/bin
        (replacing {version} with the version you downloaded)

To build ```cd Firmware; rake target=Primealpha -m```

To build unit tests ```cd Firmware; rake target=Primealpha testing=1 -m```

To compile only some unit tests in Firmware:

```rake target=Primealpha testing=1 test=streams```

```rake target=Primealpha testing=1 test=dispatch,streams,planner```

To compile with debug symbols: (may not work as it is very slow)

```rake target=Primealpha testing=1 test=streams debug=1```

To compile a unit test that tests a module, and to include that module

```rake target=Bambino testing=1 test=temperatureswitch modules=tools/temperatureswitch```

You need to install ruby (and rake) to build.

```> sudo apt-get install ruby```

Replace Bambino with Primealpha or Minialpha if you have those boards.

