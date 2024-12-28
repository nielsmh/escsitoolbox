# Toolbox for Emulated SCSI devices

This project aims to provide a toolbox for host computers with emulated SCSI devices.
These include the BlueSCSI and ZuluSCSI firmwares.
The supported hosts are intended to be 16 bit MS-DOS, Windows 3.x, and Windows 9x.

Firmwares known to support this interface:
* https://github.com/BlueSCSI/BlueSCSI-v2 (version 2024.05.21 and later)
* https://github.com/ZuluSCSI/ZuluSCSI-firmware (version 2024.05.17 and later)

The Toolbox functionality in this line of firmwares was originally designed for a
Mac OS application and has a few design choices relating to that.

## Status

The DOS version of this tool is partially functional.
Be aware that the firmware support for these features is also still under
development and is not yet stable.

Working:
- Enumerate installed SCSI devices
- Querying for list of available CD images
- Changing mounted CD image
- List files on shared directory on SD card
- Download files from shared directory

Not working:
- See the "beware" notes in the Usage section below for places where
  the device firmware might not behave as expected.

Not started:
- Upload files to shared directory
- Windows versions of the software

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
are known to cause problems currently. Please report any you discover as bugs.

The DOS version also appears to work in MS-DOS Prompt under Windows 9x/Me,
where Windows will provide ASPI services instead of using a real-mode DOS driver.

Copy the `SCSITB.EXE` file to your computer with the emulated SCSI device installed.
Ideally, place it in a directory on your PATH, for easy access.
(No other files are required for running the tool.)

Verify you can run the tool by typing the command: `scsitb help`.

### List installed SCSI devices

```
scsitb info
```

This searches for installed SCSI devices and prints their names to the screen.
The output will look something like this:

```
C:\> scsitb info
Name   Type       Manufacturer         Model                Adapter           
------------------------------------------------------------------------------
0:0:0  Disk       ZULUSCSIHARDDRIV     HARDDRIVE            ADAPTEC AIC-7870
0:1:0  CD-ROM     ZULUSCSICDROM        CDROM                ADAPTEC AIC-7870
```

The names are three-part: Host adapter ID, Target ID, Logical Unit Number (LUN).
When any of the other commands require you to enter a device name,
it is these names you need to enter.

You can shorten names: If you are addressing LUN 0 on a device,
you can leave out the LUN, so the two above device names can also be shortened to
`0:0` and `0:1`.
For devices with LUN 0 on host adapter 0, you can leave out both parts,
so the above two can also be shortened to simply `0` and `1`.
That is, if both the first and last number in a name is zero,
you only need to use the middle number.

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

_**Note:** As of June 4, 2024, BlueSCSCI and ZuluSCSI only support changing mounted
image on CD-ROM drives. There are plans to support changing images for other device
types, but not any release dates yet._

### List shared directory on SD card

Create a directory named `shared` in the root of your SD card to use this feature.
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

You can specify the destination filename, but if you leave it out the original
filename will be used.

_**Careful:** If the filename on the SD card is not DOS compatible (too long,
or illegal characters) the transfer might fail or give unexepected results.
In that case, specify the destination filename._

_**Careful:** Make sure your destination disk has enough free space for the
file you download._

_**Careful:** The tool will overwrite any destination file with the same name,
no questions asked._

```
C:\> scsitb get 0 1 scsitb2.zip
Retrieving file list from device 0:0:0 type 0 (Disk)...
Output filename: scsitb2.zip
  Block 5/14 (35%)...
```

_**BEWARE:** If a transfer is aborted before it completes, the hardware device
firmware might not close the source file on the SD card correctly.
If this happens, it is currently unknown whether it's safe to continue using
the hardware device. Consider switching the computer off and back on to ensure
the device is properly reset and does not have unexpected open files._

## Development environment

Currently this project is developed with Open Watcom C 1.9,
and is not tested with any other compilers/toolchains.

Testing is currently primarily done with an Adaptec AIC-7870 (AHA-2940) host adapter,
using ZuluSCSI RP2040 and BlueSCSI v2 devices, running recent firmware versions.

Testing with SCSI adapters from other manufacturers has so far caused problems,
and only ASPI managers from Adaptec and Microsoft (Windows 9x) work correctly.

## License

This software is licensed under the terms of the GPL v3 license,
to be able to integrate code from the firmware projects it is designed for.
See [COPYING.md](COPYING.md) for the full license text.

Several of the header files under the `include/` directory are either verbatim
or heavily based on files from the Adaptec ASPI Software Development Kit.
Those files can be recognized by the comments at the top of them, and are
not covered by the above license terms.
