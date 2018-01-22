The zwhome custom home automation system

Disclaimer: This page documents a home automation system I put together solely for my needs. It includes the source code and development environment for software I wrote over a period of years. During that time, this system evolved in significant ways. So, there is legacy conditional code that might not work anymore. For example, at one time or another, this system ran under Microsoft Windows, had a touchscreen display, and was accessible via a web page. Even though the software satisfies my needs and performs stably, I still consider it unfinished. The system still needs to discover the characteristics of the z-wave devices rather than having them manually entered into zwhome's configuration file. It also needs to handle dimmers instead of just switches.

This system is built around three major software components, which need to be compiled and linked. They are:

1.  zwhome: This is a custom software program that provides these functions
  - Support for Z-wave device switches, dimmers, and the Aeon Labs 4-in-1 multi-sensor
  - Support for custom wifi sensors (https://github.com/obzerving/OTALightSensor) and switches (https://github.com/obzerving/OTAfanctrl)
  - Rules-based scheduling of device actions up to one year in advance
  - Recovery of device schedules and states after power failure
  - Support for immediate control of device actions through a restful service
  - Web status page (e.g. http://zwhome.local:8000/status)
  - Software used
    - Open-zwave library, located at https://github.com/OpenZWave/open-zwave, for z-wave device support
    - Mongoose Embedded Web Server Library, located at https://github.com/cesanta/mongoose
    - Slam-clock, located at https://github.com/jwatte/slam-clock, for real-time clock module support
    - Code::Blocks IDE for editing, compiling, and linking the program
    - prequel, located at https://github.com/obzerving/prequel, for naming Z-wave devices. This software needs to be run once before the initial launch of zwhome. The program can be run whenever a new Z-Wave module is added.
  - Hardware used
    - Raspberry Pi 3 and suitable power supply (5 volts @ 2 amps)
    - Aeon Labs DSA02203-ZWUS Z-Wave Z-Stick Series 2 USB Dongle
    - DS3231-based real-time clock module
    - Z-wave modules of various types for controlling particular lights and appliances

2.  Amazon Echo Bridge: This software package provides support for immediate control of device actions by an Amazon Echo
  - Software used
    - ha-bridge, located at https://github.com/bwssytems/ha-bridge
  - Hardware Used
    - Amazon Echo or Dot

3. zdot: This is optional custom software that provides this additional function
  - Support for bluetooth streaming of audio from an Amazon Echo Dot to a Pioneer VSX model audio receiver
  - Software used
    - Mongoose Embedded Web Server Library, located at https://github.com/cesanta/mongoose
  - Hardware Used:
    - Amazon Echo Dot

This repository is for the zwhome program. See the Installation instructions for further details on all three components.