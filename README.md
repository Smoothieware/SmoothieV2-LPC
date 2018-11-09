# smoothie version 2
Smoothie V2 using LPCopen and FreeRTOS.

Currently runs on Bambino boards and V2 Prime Alpha proto boards.

Firmware/... is for Smoothie firmware code and Test Units

Currently uses the following toolchain..

gcc version 7.3.1 20180622 (release) [ARM/embedded-7-branch revision 261907] 
(GNU Tools for Arm Embedded Processors 7-2018-q3-update)

(or any 7.x.x will probably work).

To get the tool chain you should do the following on compatible Linuxes... (Ubuntu/Debian)

    sudo add-apt-repository ppa:team-gcc-arm-embedded/ppa
    sudo apt-get update
    sudo apt-get install gcc-arm-embedded
        

<<<<<<< HEAD
To build ```cd Firmware; rake target=Primealpha -m```

To build unit tests ```cd Firmware; rake target=Primealpha testing=1 -m```
=======
To build the Firmware

    cd Firmware
    rake target=Bambino -m

To build unit tests 
     
    cd Firmware
    rake target=Bambino testing=1 -m
>>>>>>> upstream/master

To compile only some unit tests in Firmware:

```rake target=Primealpha testing=1 test=streams```

```rake target=Primealpha testing=1 test=dispatch,streams,planner```

To compile with debug symbols: (may not work as it is very slow)

```rake target=Primealpha testing=1 test=streams debug=1```

You need to install ruby (and rake) to build.

```> sudo apt-get install ruby```

Replace Bambino with Primealpha or Minialpha if you have those boards.
