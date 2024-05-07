# Toolbox for Emulated SCSI devices

This project aims to provide a toolbox for host computers with emulated SCSI devices.
These include the BlueSCSI and ZuluSCSI firmwares.
The supported hosts are intended to be 16 bit MS-DOS, Windows 3.x, and Windows 9x.

Firmwares known to support this interface:
* https://github.com/BlueSCSI/BlueSCSI-v2
* https://github.com/ZuluSCSI/ZuluSCSI-firmware

The Toolbox functionality in this line of firmwares was originally designed for a
Mac OS application and has a few design choices relating to that.

## Status

This is a very early stage and is probably not useful to anyone yet.

Working:
- Enumerate installed SCSI host adapters
- Enumerate installed SCSI devices
- Commandline parsing

Not working:
- Any of the toolbox commands

Not started:
- Querying for list of available CD images
- Changing mounted CD image
- Windows versions of the software
- Send and receive files to Share directory on SD card

## Development environment

Currently this project is developed with Open Watcom C 1.9,
and is not tested with any other compilers/toolchains.

Testing is done with an Adaptec AIC-7870 host controller and a ZuluSCSI RP2040.

## License

This software is licensed under the terms of the GPL v3 license,
to be able to integrate code from the firmware projects it is designed for.
See [COPYING.md](COPYING.md) for the full license text.

Several of the header files under the `include/` directory are either verbatim
or heavily based on files from the Adaptec ASPI Software Development Kit.
Those files can be recognized by the comments at the top of them, and are
not covered by the above license terms.
