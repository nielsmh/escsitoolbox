CXXFLAGS=-w4 -e25 -zq -od -of -d0 -bt=dos -fo=.obj -mc -xs -xr

dos_objects = tbdos.obj aspiintf.obj scsiintf.obj
win_objects = tbwin.obj
win_resources = tbwin.res
dos_exe = scsitb.exe
win_exe = scsitbw.exe

.cpp: dos\;win\

.cpp.obj: .AUTODEPEND
	wcl -c -cc++ -q $(CXXFLAGS) $[*

$(dos_exe): $(dos_objects)
	wcl -l=dos -q -lr -fe=$^. $(dos_objects)

$(win_exe) $(win_resources): $(win_objects) win\tbwin.rc $(dos_exe)
	wcl -l=windows -q -lr -fe=$^. -"option stub=$(dos_exe)" $(win_objects)
	wrc -q -bt=windows win\tbwin.rc $^.

clean: .SYMBOLIC
	del *.err
	del $(dos_exe)
	del $(dos_objects)
	del $(win_exe)
	del $(win_objects) $(win_resources)

all: $(dos_exe)
