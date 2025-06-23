# Toolbox for Emulated SCSI devices

This project aims to provide a toolbox for host computers with emulated SCSI devices.
These include the BlueSCSI and ZuluSCSI firmwares.
The supported hosts are intended to be 16 bit MS-DOS, Windows 3.x, and Windows 9x.

Firmwares known to support this interface:
* https://github.com/BlueSCSI/BlueSCSI-v2 (version 2024.05.21 and later)
* https://github.com/ZuluSCSI/ZuluSCSI-firmware (version 2024.05.17 and later)


## Status

The DOS version of this tool is partially functional.
Be aware that the firmware support for these features is also still under
development and is not yet stable.

Working:
- Enumerate installed SCSI devices
- Querying for list of available CD/disk images
- Changing mounted CD/disk image
- List files on shared directory on SD card
- Download files from shared directory
- Upload files to shared directory
- Toggle firmware debug logging mode

Not working:
- See the "beware" notes in the Usage section below for places where
  the device firmware might not behave as expected.
- The program doesn't actively check for the devices being queried having the
  toolbox feature available or enabled. If you attempt to send commands to
  a device that doesn't have toolbox support, the device might misbehave.

Not started:
- Windows versions of the software
  (lower priority, since the DOS version works under both Windows 3.x and 9x)

## Usage (DOS)

Make sure your BlueSCSI or ZuluSCSI device is running a firmware version with
Toolbox support. For ZuluSCSI you also need to make sure the Toolbox is
enabled in your config file:
```
[SCSI]
; ... various other settings here
EnableToolbox = 1
```

Make sure you have installed an appropriate ASPI driver for DOS in your `config.sys` file.
The driver to use depends on your SCSI host adapter. Note that some driver versions
are known to cause problems currently. Bug reports about specific hardware and driver
versions are very welcome.

The DOS version also works in MS-DOS Prompt under Windows 9x/Me, where Windows will
provide ASPI services, instead of using a real-mode DOS driver.

Copy the `SCSITB.EXE` file to your computer with the emulated SCSI device installed.
Ideally, place it in a directory on your PATH, for easy access.
(No other files are required for running the tool.)

Verify you can run the tool by typing the command: `scsitb help`.

### List installed SCSI devices

```
scsitb info
```

This searches for installed SCSI devices and prints their addresses to the screen.
The output will look something like this:

```
C:\> scsitb info
Addr   Vendor   Model            Type       Adapter            Emulation Dev
----------------------------------------------------------------------------
0:0:0  QUANTUM  BlueSCSI Pico    Disk       ADAPTEC AIC-7870   Fixed      A
0:1:0  BlueSCSI CD-ROM CDU-55S   CD-ROM     ADAPTEC AIC-7870   Optical    A
```

The device address is three-part: Host adapter ID, Target ID, Logical Unit Number (LUN).
When any of the other commands require you to enter a device address,
it is these names you need to enter.

You can shorten addresses: If you are addressing LUN 0 on a device,
you can leave out the LUN, so the two above device addresses can also be shortened to
`0:0` and `0:1`.
For devices with LUN 0 on host adapter 0, you can leave out both parts,
so the above two can also be shortened to simply `0` and `1`.
That is, if both the first and last number in an address are zero,
you only need to use the middle number.

The Emulation and Dev columns are only filled for SCSI devices with Toolbox support.
In the Emulation column is shown which type of device is being emulated.
The Dev column indicates which physical hardware device a logical device belongs to,
such that when one physical BlueSCSI or ZuluSCSI device emulates multiple logical devices,
they will all show the same letter under Dev.

### List disk images (for CD-ROM etc.)

```
scsitb lsimg <device>
```

This requests a list of all images available for a given device.
Different devices will typically have separate image directories
on your SD card.

The output will look something like this:

```
C:\> scsitb lsimg 1
Retrieving images from device 0:1:0 type 5 (CD-ROM)...
3 files found
0  FD13LGCY.iso                        244,176,896 B
1  ezscsi4.iso                         413,448,192 B
2  Development tools.iso                85,196,800 B
```

The number before each file in the list is its index.

Note that there is a limit of max 100 images. If you have more than
100 files in your folder on the SD card, the command will fail.
This is a restriction imposed from the protocol used.

### Change mounted disk image

```
scsitb setimg <device> <imageindex>
```

This requests that a different disk image is mounted on the emulated device.
Use the image index given by the `lsimg` command.

```
C:\> scsitb setimg 0:1 2
Set loaded image for device 0:1:0 type 5 (CD-ROM)
Set next image command sent successfully.

C:\> dir d:

 Volume in drive D is DEVELOPMENT
 Directory of D:\
[...]
```

_**BEWARE:** BlueSCSI releases before 2024.05.21, and ZuluSCSI releases before
2024.05.17, do not report the media change correctly, and will confuse the CD-ROM
device driver. Make sure to use an updated firmware to avoid these issues._

_**Note:** On current firmware versions, as of June 23, 2025, the firmwares for
BlueSCSI and ZuluSCSI devices only search for disk images in a folder named "CDx",
"FDx", "ZPx", and so on, depending on the device type, and where "x" is the device
number. Image file directory in the ini file is not used._

_**Note:** Depending on the hardware and firmware version used, this may allow
switching image only on emulated CD-ROM devices, or on several other removable
media type devices. Please check the documentation for your specific device._

### List shared directory on SD card

To use this feature,
you must first create a directory named `shared` in the root of your SD card.
(Also see the documentation for your device firmware, this can be configured further.)

```
scsitb lsdir <device>
```

Works similar to listing image files for a disk device, but instead lists the files
in the shared directory. Note that all SCSI devices emulated by the same physical device
will have the same shared directory, but you still need to specify a device name to
select which hardware device you are querying.

```
C:\> scsitb lsdir 0
Retrieving file list from device 0:0:0 type 0 (Disk)...
2 files found
0  test.txt                                     27 B
1  scsitb.zip                               53,873 B

C:\> scsitb lsdir 1
Retrieving file list from device 0:1:0 type 5 (CD-ROM)...
2 files found
0  test.txt                                     27 B
1  scsitb.zip                               53,873 B
```

The number before each file in the list is its index.

Note that there is a limit of max 100 files. If you have more than
100 files in your shared directory, the command will fail.

### Download file from shared directory

```
scsitb get <device> <fileindex> [filename]
```

Copies a file from the shared directory on the SD card onto your computer.

You can specify the destination filename, but if you leave it out, the original
filename will be used.

_**Note:** The tool will attempt to clean up filenames to be DOS compatible,
but if you have files with long names, or unusual characters, it may still be
a good idea to specify the destination filename yourself regardless._

_**Careful:** Make sure your destination disk has enough free space for the
file you download._

```
C:\> scsitb get 0 1 scsitb2.zip
Retrieving file list from device 0:0:0 type 0 (Disk)...
Output filename: scsitb2.zip
  Block 5 / 14 (35%)...
```

_**BEWARE:** If a transfer is aborted before it completes, the hardware device
firmware might not close the source file on the SD card correctly.
If this happens, it is currently unknown whether it's safe to continue using
the hardware device. Consider switching the computer off and back on to ensure
the device is properly reset and does not have unexpected open files._

### Upload file to shared directory

```
scsitb put <device> <filename>
```

Copies a file from the computer to the shared directory on the SD card.

The destination filename will be the same as the original filename.

_**Note:** Current release versions (as of 2024-12-30) of BlueSCSI and ZuluSCSI
firmware have an issue with at least some SCSI adapters, causing the transfer
to fail._

```
C:\> scsitb put 0 D:\dev\output.log
Verifying destination device 0:0:0 type 0 (Disk)...
Sending: D:\dev\output.log => output.log
  Finished sending 118 blocks

C:\> scsitb put 0 D:\dev\output.log
Verifying destination device 0:0:0 type 0 (Disk)...
Destination filename: output.log
The destination already contains a file with this name. Overwrite? (Y/N) y
Sending: D:\dev\output.log => output.log
  Block 45 / 120 (37%)...
```

### Toggle debug logging on device firmware

```
scsitb debug <device> [flag]
```

Displays, and optionally changes, the device firmware's internal debug logging flag.

The _flag_ parameter can be 0 or 1, to disable or enable debug logging. If the parameter
is omitted, the current flag is displayed without changing it.

## Development environment

Currently this project is developed with Open Watcom C++ 1.9 and 2.0 beta,
and is not tested with any other compilers/toolchains. It is possible to build the
software on modern Windows or Linux systems.

The software is tested with a variety of SCSI host adapters, both for PCI bus and for
ISA bus, but the tests are not comprehensive. Specific host adapters, or specific ASPI
driver versions, may cause issues. Please report these, and include as much detail about
the hardware and software environment as possible.

## License

This software is licensed under the terms of the GPL v3 license,
to be able to integrate code from the firmware projects it is designed for.
See [COPYING.md](COPYING.md) for the full license text.

Several of the header files under the `include/` directory are either verbatim
or heavily based on files from the Adaptec ASPI Software Development Kit.
Those files can be recognized by the comments at the top of them, and are
not covered by the above license terms.
