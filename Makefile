CXXFLAGS=-w4 -e25 -zq -oabehikls -d0 -bt=dos -fo=.obj -mc

dos_objects = tbdos.obj aspiintf.obj scsiintf.obj toolbox.obj scsishrd.obj
win_objects = tbwin.obj
win_resources = tbwin.res
dos_exe = scsitb.exe
win_exe = scsitbw.exe

.cpp: dos/;win/;shared/

.cpp.obj: .AUTODEPEND
	wcl -c -cc++ -q $(CXXFLAGS) $[*

$(dos_exe): $(dos_objects)
	wcl -l=dos -q -lr -fe=$^. $(dos_objects)

$(win_exe) $(win_resources): $(win_objects) win\tbwin.rc $(dos_exe)
	wcl -l=windows -q -lr -fe=$^. -"option stub=$(dos_exe)" $(win_objects)
	wrc -q -bt=windows win\tbwin.rc $^.

clean: .SYMBOLIC
	rm -f *.err
	rm -f $(dos_exe)
	rm -f $(dos_objects)
	rm -f $(win_exe)
	rm -f $(win_objects) $(win_resources)

all: $(dos_exe)
