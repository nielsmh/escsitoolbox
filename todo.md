# To-do items for SCSITB

## Implement feature detection
Request the appropriate vendor pages from the SCSI device to query
whether it actually is expected to support the features.

Possibly support different feature levels/revisions of the interface.


## Build system and code quality

Split into more files, and reconsider project structure.


## Test with more ASPI drivers

Try to make it work with Adaptec EZ-SCSI v5 drivers.

Test with LSI (I-O DATA) driver.

Test under Windows 9x DOS box.

Possibly implement workarounds.


## Windows version

Investigate how much code can be shared between OS versions.

Build a user interface, probably based on dialog templates,
for Windows 3.x.

Implement ASPI calls.

Investigate shell extension UI for Windows 9x.

Investigate CLI version port for NT.


## Shared folder

Convert filenames to supported format.

Prompt before overwrites.

Upload support.


# Done

## Update documentation

Firmwares with bugfixes have been released.


## Build system and code quality

Use a handwritten makefile instead of the OpenWatcom IDE generated one.

Build on modern Windows and transfer exe to test machine.

