# Microsoft Developer Studio Generated NMAKE File, Based on OilPaint.dsp
!IF "$(CFG)" == ""
CFG=OilPaint - Win32 Debug
!MESSAGE No configuration specified. Defaulting to OilPaint - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "OilPaint - Win32 Release" && "$(CFG)" != "OilPaint - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "OilPaint.mak" CFG="OilPaint - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "OilPaint - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "OilPaint - Win32 Debug" (based on "Win32 (x86) Application")
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

!IF  "$(CFG)" == "OilPaint - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\OilPaint.exe"


CLEAN :
	-@erase "$(INTDIR)\ControlImage.obj"
	-@erase "$(INTDIR)\Image.obj"
	-@erase "$(INTDIR)\JPeg.obj"
	-@erase "$(INTDIR)\Main.obj"
	-@erase "$(INTDIR)\OilPaint.res"
	-@erase "$(INTDIR)\Register.obj"
	-@erase "$(INTDIR)\Util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\OilPaint.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\OilPaint.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\OilPaint.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\OilPaint.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=z:\jpeg\jpeg-6b\JPegLib\Release\JPegLib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib escarpment.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\OilPaint.pdb" /machine:I386 /out:"$(OUTDIR)\OilPaint.exe" 
LINK32_OBJS= \
	"$(INTDIR)\ControlImage.obj" \
	"$(INTDIR)\Image.obj" \
	"$(INTDIR)\JPeg.obj" \
	"$(INTDIR)\Main.obj" \
	"$(INTDIR)\Register.obj" \
	"$(INTDIR)\Util.obj" \
	"$(INTDIR)\OilPaint.res"

"$(OUTDIR)\OilPaint.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "OilPaint - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\OilPaint.exe"


CLEAN :
	-@erase "$(INTDIR)\ControlImage.obj"
	-@erase "$(INTDIR)\Image.obj"
	-@erase "$(INTDIR)\JPeg.obj"
	-@erase "$(INTDIR)\Main.obj"
	-@erase "$(INTDIR)\OilPaint.res"
	-@erase "$(INTDIR)\Register.obj"
	-@erase "$(INTDIR)\Util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\OilPaint.exe"
	-@erase "$(OUTDIR)\OilPaint.ilk"
	-@erase "$(OUTDIR)\OilPaint.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\OilPaint.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\OilPaint.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\OilPaint.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=z:\jpeg\jpeg-6b\JPegLib\Debug\JPegLib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib escarpment.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\OilPaint.pdb" /debug /machine:I386 /out:"$(OUTDIR)\OilPaint.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\ControlImage.obj" \
	"$(INTDIR)\Image.obj" \
	"$(INTDIR)\JPeg.obj" \
	"$(INTDIR)\Main.obj" \
	"$(INTDIR)\Register.obj" \
	"$(INTDIR)\Util.obj" \
	"$(INTDIR)\OilPaint.res"

"$(OUTDIR)\OilPaint.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("OilPaint.dep")
!INCLUDE "OilPaint.dep"
!ELSE 
!MESSAGE Warning: cannot find "OilPaint.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "OilPaint - Win32 Release" || "$(CFG)" == "OilPaint - Win32 Debug"
SOURCE=.\ControlImage.cpp

"$(INTDIR)\ControlImage.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Image.cpp

"$(INTDIR)\Image.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\JPeg.cpp

"$(INTDIR)\JPeg.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Main.cpp

"$(INTDIR)\Main.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\OilPaint.rc

"$(INTDIR)\OilPaint.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\Register.cpp

"$(INTDIR)\Register.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Util.cpp

"$(INTDIR)\Util.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

