# smoothie version 2
Smoothie V2 using LPCopen and FreeRTOS.

Currently runs on Bambino boards and V2 Mini proto boards.

Firmware/... is for Smoothie firmware code and Test Units

Currently uses the following toolchain..

gcc version 7.3.1 20180622 (release) [ARM/embedded-7-branch revision 261907] 
(GNU Tools for Arm Embedded Processors 7-2018-q3-update)

(or any 7.x.x will probably work).

To build ```cd Firmware; rake target=Bambino -m```

To build unit tests ```cd Firmware; rake target=Bambino testing=1 -m```

To compile only some unit tests in Firmware:

```rake target=Bambino testing=1 test=streams```

```rake target=Bambino testing=1 test=dispatch,streams,planner```

To compile with debug symbols: (may not work as it is very slow)

```rake target=Bambino testing=1 test=streams debug=1```

You need to install ruby (and rake) to build.

```> sudo apt-get install ruby```


