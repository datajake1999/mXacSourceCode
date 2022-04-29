# Microsoft Developer Studio Generated NMAKE File, Based on SampleControls.dsp
!IF "$(CFG)" == ""
CFG=SampleControls - Win32 Debug
!MESSAGE No configuration specified. Defaulting to SampleControls - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "SampleControls - Win32 Release" && "$(CFG)" != "SampleControls - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SampleControls.mak" CFG="SampleControls - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SampleControls - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "SampleControls - Win32 Debug" (based on "Win32 (x86) Application")
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

!IF  "$(CFG)" == "SampleControls - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\SampleControls.exe"


CLEAN :
	-@erase "$(INTDIR)\Control3D.obj"
	-@erase "$(INTDIR)\ControlButton.obj"
	-@erase "$(INTDIR)\ControlColorBlend.obj"
	-@erase "$(INTDIR)\ControlComboBox.obj"
	-@erase "$(INTDIR)\ControlEdit.obj"
	-@erase "$(INTDIR)\ControlHorizontalLine.obj"
	-@erase "$(INTDIR)\ControlImage.obj"
	-@erase "$(INTDIR)\ControlLink.obj"
	-@erase "$(INTDIR)\ControlListBox.obj"
	-@erase "$(INTDIR)\ControlMenu.obj"
	-@erase "$(INTDIR)\ControlProgressBar.obj"
	-@erase "$(INTDIR)\ControlScrollBar.obj"
	-@erase "$(INTDIR)\ControlStatus.obj"
	-@erase "$(INTDIR)\Main.obj"
	-@erase "$(INTDIR)\ResLeak.obj"
	-@erase "$(INTDIR)\Sample.res"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\SampleControls.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\SampleControls.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\Sample.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\SampleControls.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\SampleControls.pdb" /machine:I386 /out:"$(OUTDIR)\SampleControls.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Control3D.obj" \
	"$(INTDIR)\ControlButton.obj" \
	"$(INTDIR)\ControlColorBlend.obj" \
	"$(INTDIR)\ControlComboBox.obj" \
	"$(INTDIR)\ControlEdit.obj" \
	"$(INTDIR)\ControlHorizontalLine.obj" \
	"$(INTDIR)\ControlImage.obj" \
	"$(INTDIR)\ControlLink.obj" \
	"$(INTDIR)\ControlListBox.obj" \
	"$(INTDIR)\ControlMenu.obj" \
	"$(INTDIR)\ControlProgressBar.obj" \
	"$(INTDIR)\ControlScrollBar.obj" \
	"$(INTDIR)\ControlStatus.obj" \
	"$(INTDIR)\Main.obj" \
	"$(INTDIR)\Sample.res" \
	"$(INTDIR)\ResLeak.obj"

"$(OUTDIR)\SampleControls.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
   copy z:\escarpment\escarpment\control*.cpp z:\escarpment\samplecontrols
	copy z:\escarpment\escarpment\cesccontrol.cpp z:\escarpment\samplecontrols
	copy z:\escarpment\escarpment\control.h z:\escarpment\samplecontrols
	 $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"

!ELSEIF  "$(CFG)" == "SampleControls - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\SampleControls.exe"


CLEAN :
	-@erase "$(INTDIR)\Control3D.obj"
	-@erase "$(INTDIR)\ControlButton.obj"
	-@erase "$(INTDIR)\ControlColorBlend.obj"
	-@erase "$(INTDIR)\ControlComboBox.obj"
	-@erase "$(INTDIR)\ControlEdit.obj"
	-@erase "$(INTDIR)\ControlHorizontalLine.obj"
	-@erase "$(INTDIR)\ControlImage.obj"
	-@erase "$(INTDIR)\ControlLink.obj"
	-@erase "$(INTDIR)\ControlListBox.obj"
	-@erase "$(INTDIR)\ControlMenu.obj"
	-@erase "$(INTDIR)\ControlProgressBar.obj"
	-@erase "$(INTDIR)\ControlScrollBar.obj"
	-@erase "$(INTDIR)\ControlStatus.obj"
	-@erase "$(INTDIR)\Main.obj"
	-@erase "$(INTDIR)\ResLeak.obj"
	-@erase "$(INTDIR)\Sample.res"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\SampleControls.exe"
	-@erase "$(OUTDIR)\SampleControls.ilk"
	-@erase "$(OUTDIR)\SampleControls.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\SampleControls.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\Sample.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\SampleControls.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\SampleControls.pdb" /debug /machine:I386 /out:"$(OUTDIR)\SampleControls.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\Control3D.obj" \
	"$(INTDIR)\ControlButton.obj" \
	"$(INTDIR)\ControlColorBlend.obj" \
	"$(INTDIR)\ControlComboBox.obj" \
	"$(INTDIR)\ControlEdit.obj" \
	"$(INTDIR)\ControlHorizontalLine.obj" \
	"$(INTDIR)\ControlImage.obj" \
	"$(INTDIR)\ControlLink.obj" \
	"$(INTDIR)\ControlListBox.obj" \
	"$(INTDIR)\ControlMenu.obj" \
	"$(INTDIR)\ControlProgressBar.obj" \
	"$(INTDIR)\ControlScrollBar.obj" \
	"$(INTDIR)\ControlStatus.obj" \
	"$(INTDIR)\Main.obj" \
	"$(INTDIR)\Sample.res" \
	"$(INTDIR)\ResLeak.obj"

"$(OUTDIR)\SampleControls.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
   copy z:\escarpment\escarpment\control*.cpp z:\escarpment\samplecontrols
	copy z:\escarpment\escarpment\cesccontrol.cpp z:\escarpment\samplecontrols
	copy z:\escarpment\escarpment\control.h z:\escarpment\samplecontrols
	copy z:\escarpment\escarpment\resleak.h z:\escarpment\samplecontrols
	copy z:\escarpment\escarpment\resleak.cpp z:\escarpment\samplecontrols
	 $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"

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
!IF EXISTS("SampleControls.dep")
!INCLUDE "SampleControls.dep"
!ELSE 
!MESSAGE Warning: cannot find "SampleControls.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "SampleControls - Win32 Release" || "$(CFG)" == "SampleControls - Win32 Debug"
SOURCE=.\Control3D.cpp

"$(INTDIR)\Control3D.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlButton.cpp

"$(INTDIR)\ControlButton.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlColorBlend.cpp

"$(INTDIR)\ControlColorBlend.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlComboBox.cpp

"$(INTDIR)\ControlComboBox.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlEdit.cpp

"$(INTDIR)\ControlEdit.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlHorizontalLine.cpp

"$(INTDIR)\ControlHorizontalLine.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlImage.cpp

"$(INTDIR)\ControlImage.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlLink.cpp

"$(INTDIR)\ControlLink.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlListBox.cpp

"$(INTDIR)\ControlListBox.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlMenu.cpp

"$(INTDIR)\ControlMenu.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlProgressBar.cpp

"$(INTDIR)\ControlProgressBar.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlScrollBar.cpp

"$(INTDIR)\ControlScrollBar.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlStatus.cpp

"$(INTDIR)\ControlStatus.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Main.cpp

"$(INTDIR)\Main.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ResLeak.cpp

"$(INTDIR)\ResLeak.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Sample.rc

"$(INTDIR)\Sample.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)



!ENDIF 

