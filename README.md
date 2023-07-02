NekoInk
=======

NekoInk is an open-source, programmable, and versatile E-paper display platform. It offers connectivity options to various type of E-paper screens, and flexible programming environment choices.

# Hardware

## Mainboard

The 1st generation of NekoInk has the following specs:

* Processing
    - NXP i.MX6ULL, 900 MHz Cortex-A7 processor
    - 512 MB DDR3L-1066 memory
* Display
    - Support for EPD screens from 6" to 13.3"
    - Up to 32 greyscale levels / 32768 colors (depends on screen)
    - 40 pin connector for ED103TC1/ES103TC2/GDEW101C01
    - 6 and 12 pin connector for capacitive touch screen (shared signal)
    - 30 pin connector for LVDS LCDs
* Connectivity
    - MicroSD slot for storage
    - 1x USB Micro-B for USB Device and charging
    - 1x USB Micro-AB for USB Host
    - 1x USB Micro-B for USB Serial terminal
    - 1x DVP port for HDMI input or camera module
    - Integrated WiFi and Bluetooth
* Power
    - AXP209 PMIC with Lithium battery charger and coulomb counter
    - Power consumption T.B.D.

## Screen Adapter

Adapter boards are available in this repo to adapt the following screens:

* 39 pin connector for ED060SC4/ED133UT2
* 34 pin connector for ED060SCF/SCN/SCP/XC3/XC5/XC9/XD4/XD6/XH2/KC1/KD1
* 35 pin connector for ED060XC8/TC1
* 50 pin connector for ES108FC1

It's possible to use a simple FPC extender to support the following screens:

* ED078KC1/KH4/GDEW078M01/078C01

The hardware is designed with KiCAD 7.0.

## Status

The 1st gen mainboard is finished, soldered, and tested and considered stable. However there is no plan for mass-production.

# Software

T.B.D.

## Operating System

Linux T.B.D.

## Waveform

This project uses a human-readable waveform format (iwf, Interchangable Waveform Format) described below. Currently this could be converted into .fw format used by i.MX6/7 EPDC/EPDCv2.

### Waveform Format

The waveform consists of one descriptor file in iwf extension (ini format) and various lut data files in csv format.

The descriptor contains the follwoing required fields:

* VERISON: the version of the descriptor, also determines the waveform type
* PREFIX: the filename prefix for actual waveform files
* MODES: the total modes supported by the waveform
* TEMPS: the total number of temperature ranges supported by the waveform
* TxRANGE: the supported temperature in degC, where x is the temperature ID

And each mode has its own mode section named \[MODEx\], where x is the mode ID, containing the following fields:

* NAME: the name for that mode
* TxFC: the frame count for the temperature in that mode, where x is the temperature ID

There should be in total of modes x temps of LUTs, saved in the filename of PREFIX_Mx_Ty.csv. Each csv file should contain the a LUT like this: lut\[src\]\[dst\]\[frame\], which means, to transition from src greyscale level to dst greyscale level, at a certain frame in a frame sequence, what voltage should be applied to the screen (0/3: GND / Keep, 1: VPOS / To black, 2: VNEG / To white). Each line contains the frame sequence for one or more source to destination pairs.

For example:

* ```4,7,1,1,1,0,2``` means to transition from greyscale level 4 to greyscale level 7, there should be 5 frames, each applying VPOS VPOS VPOS GND VNEG
* ```0:14:15,2,2,2``` means to transition from any greyscale level from 0 to 14 to greyscale level 15, there should be 3 frames, each applying VNEG VNEG VNEG

These are provided to only illustrate the file format, they are not valid or meaningful Eink driving sequences.

With current design, each mode should have fixed frame count for all associated frame sequences.

### Converting

Tools are provided in utils/ folder.

* To convert from iwf to fw (iMX6/7 EPDC format): ```./mxc_wvfm_asm input.iwf output.fw``` 
* To convert from fw to iwf: ```./mxc_wvfm_dump input.wbf output_prefix```

### Generating

To be implemented.

# License

The design, unless otherwise specified, is released under the CERN Open Source Hardware License version 2 permissive variant, CERN-OHL-P. A copy of the license is provided in the source repository. Additionally, user guide of the license is provided on ohwr.org.

The waveform converting tool is licensed under GNU GPLv2+ due to use of Linux kernel source code.

The image viewer tool is licensed under MIT. stb library used in the image viewer is public domain.
