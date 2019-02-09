# smoothie version 2
Smoothie V2 using LPCopen and FreeRTOS.

Currently runs on Bambino boards and V2 Prime Alpha proto boards.

Firmware/... is for Smoothie firmware code and Test Units

Currently uses the following toolchain..

gcc version 7.3.1 20180622 (release) [ARM/embedded-7-branch revision 261907] 
(GNU Tools for Arm Embedded Processors 7-2018-q3-update)

(or any 7.x.x or 8.x.x will probably work).

To get the tool chain you should do the following on Ubuntu based Linuxes...

    sudo add-apt-repository ppa:team-gcc-arm-embedded/ppa
    sudo apt-get update
    sudo apt-get install gcc-arm-embedded

or for Debian Stretch get the tar from here...
    https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads
    and download gcc-arm-none-eabi-8-2018-q4-major-linux.tar.bz2

Then detar to a directory and do...
    export ARMTOOLS=/downloaddir/gcc-arm-none-eabi-8-2018-q4-major/bin

To build ```cd Firmware; rake target=Primealpha -m```

To build unit tests ```cd Firmware; rake target=Primealpha testing=1 -m```

To compile only some unit tests in Firmware:

```rake target=Primealpha testing=1 test=streams```

```rake target=Primealpha testing=1 test=dispatch,streams,planner```

To compile with debug symbols: (may not work as it is very slow)

```rake target=Primealpha testing=1 test=streams debug=1```

You need to install ruby (and rake) to build.

```> sudo apt-get install ruby```

Replace Bambino with Primealpha or Minialpha if you have those boards.

# Debian grr-arm-embedded installation

The method listed above might not work on Debian systems ( Ubuntu systems are officially supported ).

This method is based on https://wiki.debian.org/CreatePackageFromPPA :

    sudo add-apt-repository ppa:team-gcc-arm-embedded/ppa
    sudo apt-get update
    
Then, edit the source list file for gcc-arm-embedded found at : 

    /etc/apt/sources.list.d/team-gcc-arm-embedded-ubuntu-ppa-disco.list

to look like this : 

    #deb [trusted=yes] http://ppa.launchpad.net/team-gcc-arm-embedded/ppa/ubuntu trusty main
    deb-src [trusted=yes] http://ppa.launchpad.net/team-gcc-arm-embedded/ppa/ubuntu trusty main
    
Note : I had to add the [trusted=yes] because gpg verification wouldn't work on my system, it might work on yours. Also, "trusty" here is the latest ubuntu version supported by team-gcc-arm-embedded, it might be different when you install.

Now we build a new debian package :

    sudo apt update
    sudo apt install autogen flex bison texlive texinfo texlive-extra-utils gcc-multilib texlive-font-utils texlive-latex-base texlive-latex-recommended texlive-generic-recommended libncurses5-dev libmpfr-dev libmpc-dev libcloog-isl-dev
    cd /tmp
    apt source --build gcc-arm-embedded
    
 Finally install the package : 
 
    sudo dpkg --install gcc-arm-embedded*.deb
    

    
    
 







    



    



