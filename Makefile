CXXFLAGS=-w4 -e25 -zq -od -of -d0 -bt=dos -fo=.obj -mc -xs -xr

dos_objects = tbdos.obj aspiintf.obj scsiintf.obj
dos_exe = scsitb.exe

.cpp: dos\;win\

.cpp.obj: .AUTODEPEND
	wcl -c -cc++ -q $(CXXFLAGS) $[*

scsitb.exe: $(dos_objects)
	wcl -l=dos -q -lr -fe=$^. $(dos_objects)

clean: .SYMBOLIC
	del *.err
	del $(dos_exe)
	del $(dos_objects)

all: $(dos_exe)
