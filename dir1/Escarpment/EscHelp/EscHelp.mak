# Microsoft Developer Studio Generated NMAKE File, Based on EscHelp.dsp
!IF "$(CFG)" == ""
CFG=EscHelp - Win32 Debug
!MESSAGE No configuration specified. Defaulting to EscHelp - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "EscHelp - Win32 Release" && "$(CFG)" != "EscHelp - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "EscHelp.mak" CFG="EscHelp - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "EscHelp - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "EscHelp - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "EscHelp - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\EscHelp.exe"


CLEAN :
	-@erase "$(INTDIR)\EscHelp.obj"
	-@erase "$(INTDIR)\EscHelp.res"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\EscHelp.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\EscHelp.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\EscHelp.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\EscHelp.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\EscHelp.pdb" /machine:I386 /out:"$(OUTDIR)\EscHelp.exe" 
LINK32_OBJS= \
	"$(INTDIR)\EscHelp.obj" \
	"$(INTDIR)\EscHelp.res"

"$(OUTDIR)\EscHelp.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "EscHelp - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\EscHelp.exe"


CLEAN :
	-@erase "$(INTDIR)\EscHelp.obj"
	-@erase "$(INTDIR)\EscHelp.res"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\EscHelp.exe"
	-@erase "$(OUTDIR)\EscHelp.ilk"
	-@erase "$(OUTDIR)\EscHelp.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\EscHelp.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\EscHelp.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\EscHelp.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\EscHelp.pdb" /debug /machine:I386 /out:"$(OUTDIR)\EscHelp.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\EscHelp.obj" \
	"$(INTDIR)\EscHelp.res"

"$(OUTDIR)\EscHelp.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("EscHelp.dep")
!INCLUDE "EscHelp.dep"
!ELSE 
!MESSAGE Warning: cannot find "EscHelp.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "EscHelp - Win32 Release" || "$(CFG)" == "EscHelp - Win32 Debug"
SOURCE=.\EscHelp.cpp

"$(INTDIR)\EscHelp.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\EscHelp.rc

"$(INTDIR)\EscHelp.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)



!ENDIF 

