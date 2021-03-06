# Microsoft Developer Studio Generated NMAKE File, Based on MMLEdit.dsp
!IF "$(CFG)" == ""
CFG=MMLEdit - Win32 Debug
!MESSAGE No configuration specified. Defaulting to MMLEdit - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "MMLEdit - Win32 Release" && "$(CFG)" != "MMLEdit - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "MMLEdit.mak" CFG="MMLEdit - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "MMLEdit - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "MMLEdit - Win32 Debug" (based on "Win32 (x86) Application")
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

!IF  "$(CFG)" == "MMLEdit - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\MMLEdit.exe"


CLEAN :
	-@erase "$(INTDIR)\MMLEdit.obj"
	-@erase "$(INTDIR)\MMLEdit.res"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\MMLEdit.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\MMLEdit.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\MMLEdit.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\MMLEdit.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\MMLEdit.pdb" /machine:I386 /out:"$(OUTDIR)\MMLEdit.exe" 
LINK32_OBJS= \
	"$(INTDIR)\MMLEdit.obj" \
	"$(INTDIR)\MMLEdit.res"

"$(OUTDIR)\MMLEdit.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "MMLEdit - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\MMLEdit.exe"


CLEAN :
	-@erase "$(INTDIR)\MMLEdit.obj"
	-@erase "$(INTDIR)\MMLEdit.res"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\MMLEdit.exe"
	-@erase "$(OUTDIR)\MMLEdit.ilk"
	-@erase "$(OUTDIR)\MMLEdit.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\MMLEdit.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\MMLEdit.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\MMLEdit.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\MMLEdit.pdb" /debug /machine:I386 /out:"$(OUTDIR)\MMLEdit.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\MMLEdit.obj" \
	"$(INTDIR)\MMLEdit.res"

"$(OUTDIR)\MMLEdit.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("MMLEdit.dep")
!INCLUDE "MMLEdit.dep"
!ELSE 
!MESSAGE Warning: cannot find "MMLEdit.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "MMLEdit - Win32 Release" || "$(CFG)" == "MMLEdit - Win32 Debug"
SOURCE=.\MMLEdit.cpp

"$(INTDIR)\MMLEdit.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\MMLEdit.rc

"$(INTDIR)\MMLEdit.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)



!ENDIF 

