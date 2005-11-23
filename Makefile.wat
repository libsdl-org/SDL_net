#=============================================================================
#          This is a Watcom makefile to build SDLNET.DLL for OS/2
#
#
#=============================================================================

dllname=SDLNET

object_files= SDLnet.obj SDLnetselect.obj SDLnetTCP.obj SDLnetUDP.obj

# Extra stuffs to pass to C compiler:
ExtraCFlags=

#
#==============================================================================
#
!include Watcom.mif

.before
    @set include=$(%os2tk)\h;$(%include);$(%sdlhome)\include;.\;

all : $(dllname).dll $(dllname).lib

$(dllname).dll : $(object_files)
    wlink @$(dllname)

$(dllname).lib : $(dllname).dll
    implib $(dllname).lib $(dllname).dll

clean : .SYMBOLIC
    @if exist $(dllname).dll del $(dllname).dll
    @if exist *.lib del *.lib
    @if exist *.obj del *.obj
    @if exist *.map del *.map
    @if exist *.res del *.res
    @if exist *.lst del *.lst
