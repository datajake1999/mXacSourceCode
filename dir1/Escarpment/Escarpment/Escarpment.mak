# Microsoft Developer Studio Generated NMAKE File, Based on Escarpment.dsp
!IF "$(CFG)" == ""
CFG=Escarpment - Win32 Debug
!MESSAGE No configuration specified. Defaulting to Escarpment - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Escarpment - Win32 Release" && "$(CFG)" != "Escarpment - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Escarpment.mak" CFG="Escarpment - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Escarpment - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Escarpment - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
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

!IF  "$(CFG)" == "Escarpment - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\Escarpment.dll"


CLEAN :
	-@erase "$(INTDIR)\BitmapCache.obj"
	-@erase "$(INTDIR)\CEscControl.obj"
	-@erase "$(INTDIR)\CEscPage.obj"
	-@erase "$(INTDIR)\CEscSearch.obj"
	-@erase "$(INTDIR)\CEscWindow.obj"
	-@erase "$(INTDIR)\Control3D.obj"
	-@erase "$(INTDIR)\ControlButton.obj"
	-@erase "$(INTDIR)\ControlChart.obj"
	-@erase "$(INTDIR)\ControlColorBlend.obj"
	-@erase "$(INTDIR)\ControlComboBox.obj"
	-@erase "$(INTDIR)\ControlDate.obj"
	-@erase "$(INTDIR)\ControlEdit.obj"
	-@erase "$(INTDIR)\ControlFilteredList.obj"
	-@erase "$(INTDIR)\ControlHorizontalLine.obj"
	-@erase "$(INTDIR)\ControlImage.obj"
	-@erase "$(INTDIR)\ControlLink.obj"
	-@erase "$(INTDIR)\ControlListBox.obj"
	-@erase "$(INTDIR)\ControlMenu.obj"
	-@erase "$(INTDIR)\ControlProgressBar.obj"
	-@erase "$(INTDIR)\ControlScrollBar.obj"
	-@erase "$(INTDIR)\ControlStatus.obj"
	-@erase "$(INTDIR)\ControlTime.obj"
	-@erase "$(INTDIR)\DLLMain.obj"
	-@erase "$(INTDIR)\FontCache.obj"
	-@erase "$(INTDIR)\JPeg.obj"
	-@erase "$(INTDIR)\MessageBox.obj"
	-@erase "$(INTDIR)\MMLInterpret.obj"
	-@erase "$(INTDIR)\MMLParse.obj"
	-@erase "$(INTDIR)\Paint.obj"
	-@erase "$(INTDIR)\Register.obj"
	-@erase "$(INTDIR)\Render.obj"
	-@erase "$(INTDIR)\ResLeak.obj"
	-@erase "$(INTDIR)\Test.res"
	-@erase "$(INTDIR)\TextWrap.obj"
	-@erase "$(INTDIR)\Tools.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\Escarpment.dll"
	-@erase "$(OUTDIR)\Escarpment.exp"
	-@erase "$(OUTDIR)\Escarpment.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "ESCARPMENT_EXPORTS" /Fp"$(INTDIR)\Escarpment.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\Test.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\Escarpment.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=z:\jpeg\jpeg-6b\JPegLib\Release\JPegLib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /base:"0x61800000" /dll /incremental:no /pdb:"$(OUTDIR)\Escarpment.pdb" /machine:I386 /out:"$(OUTDIR)\Escarpment.dll" /implib:"$(OUTDIR)\Escarpment.lib" 
LINK32_OBJS= \
	"$(INTDIR)\BitmapCache.obj" \
	"$(INTDIR)\CEscControl.obj" \
	"$(INTDIR)\CEscPage.obj" \
	"$(INTDIR)\CEscSearch.obj" \
	"$(INTDIR)\CEscWindow.obj" \
	"$(INTDIR)\Control3D.obj" \
	"$(INTDIR)\ControlButton.obj" \
	"$(INTDIR)\ControlChart.obj" \
	"$(INTDIR)\ControlColorBlend.obj" \
	"$(INTDIR)\ControlComboBox.obj" \
	"$(INTDIR)\ControlDate.obj" \
	"$(INTDIR)\ControlEdit.obj" \
	"$(INTDIR)\ControlFilteredList.obj" \
	"$(INTDIR)\ControlHorizontalLine.obj" \
	"$(INTDIR)\ControlImage.obj" \
	"$(INTDIR)\ControlLink.obj" \
	"$(INTDIR)\ControlListBox.obj" \
	"$(INTDIR)\ControlMenu.obj" \
	"$(INTDIR)\ControlProgressBar.obj" \
	"$(INTDIR)\ControlScrollBar.obj" \
	"$(INTDIR)\ControlStatus.obj" \
	"$(INTDIR)\ControlTime.obj" \
	"$(INTDIR)\DLLMain.obj" \
	"$(INTDIR)\FontCache.obj" \
	"$(INTDIR)\JPeg.obj" \
	"$(INTDIR)\MessageBox.obj" \
	"$(INTDIR)\MMLInterpret.obj" \
	"$(INTDIR)\MMLParse.obj" \
	"$(INTDIR)\Paint.obj" \
	"$(INTDIR)\Register.obj" \
	"$(INTDIR)\Render.obj" \
	"$(INTDIR)\ResLeak.obj" \
	"$(INTDIR)\TextWrap.obj" \
	"$(INTDIR)\Tools.obj" \
	"$(INTDIR)\Test.res"

"$(OUTDIR)\Escarpment.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\Escarpment.dll"
   copy z:\escarpment\escarpment\escarpment.h z:\include
	copy z:\escarpment\escarpment\release\escarpment.dll z:\bin
	copy z:\escarpment\escarpment\release\escarpment.lib z:\lib
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

!ELSEIF  "$(CFG)" == "Escarpment - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\Escarpment.dll"


CLEAN :
	-@erase "$(INTDIR)\BitmapCache.obj"
	-@erase "$(INTDIR)\CEscControl.obj"
	-@erase "$(INTDIR)\CEscPage.obj"
	-@erase "$(INTDIR)\CEscSearch.obj"
	-@erase "$(INTDIR)\CEscWindow.obj"
	-@erase "$(INTDIR)\Control3D.obj"
	-@erase "$(INTDIR)\ControlButton.obj"
	-@erase "$(INTDIR)\ControlChart.obj"
	-@erase "$(INTDIR)\ControlColorBlend.obj"
	-@erase "$(INTDIR)\ControlComboBox.obj"
	-@erase "$(INTDIR)\ControlDate.obj"
	-@erase "$(INTDIR)\ControlEdit.obj"
	-@erase "$(INTDIR)\ControlFilteredList.obj"
	-@erase "$(INTDIR)\ControlHorizontalLine.obj"
	-@erase "$(INTDIR)\ControlImage.obj"
	-@erase "$(INTDIR)\ControlLink.obj"
	-@erase "$(INTDIR)\ControlListBox.obj"
	-@erase "$(INTDIR)\ControlMenu.obj"
	-@erase "$(INTDIR)\ControlProgressBar.obj"
	-@erase "$(INTDIR)\ControlScrollBar.obj"
	-@erase "$(INTDIR)\ControlStatus.obj"
	-@erase "$(INTDIR)\ControlTime.obj"
	-@erase "$(INTDIR)\DLLMain.obj"
	-@erase "$(INTDIR)\FontCache.obj"
	-@erase "$(INTDIR)\JPeg.obj"
	-@erase "$(INTDIR)\MessageBox.obj"
	-@erase "$(INTDIR)\MMLInterpret.obj"
	-@erase "$(INTDIR)\MMLParse.obj"
	-@erase "$(INTDIR)\Paint.obj"
	-@erase "$(INTDIR)\Register.obj"
	-@erase "$(INTDIR)\Render.obj"
	-@erase "$(INTDIR)\ResLeak.obj"
	-@erase "$(INTDIR)\Test.res"
	-@erase "$(INTDIR)\TextWrap.obj"
	-@erase "$(INTDIR)\Tools.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\Escarpment.dll"
	-@erase "$(OUTDIR)\Escarpment.exp"
	-@erase "$(OUTDIR)\Escarpment.ilk"
	-@erase "$(OUTDIR)\Escarpment.lib"
	-@erase "$(OUTDIR)\Escarpment.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "ESCARPMENT_EXPORTS" /Fp"$(INTDIR)\Escarpment.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\Test.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\Escarpment.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=z:\jpeg\jpeg-6b\JPegLib\Debug\JPegLib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /base:"0x61800000" /dll /incremental:yes /pdb:"$(OUTDIR)\Escarpment.pdb" /debug /machine:I386 /out:"$(OUTDIR)\Escarpment.dll" /implib:"$(OUTDIR)\Escarpment.lib" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\BitmapCache.obj" \
	"$(INTDIR)\CEscControl.obj" \
	"$(INTDIR)\CEscPage.obj" \
	"$(INTDIR)\CEscSearch.obj" \
	"$(INTDIR)\CEscWindow.obj" \
	"$(INTDIR)\Control3D.obj" \
	"$(INTDIR)\ControlButton.obj" \
	"$(INTDIR)\ControlChart.obj" \
	"$(INTDIR)\ControlColorBlend.obj" \
	"$(INTDIR)\ControlComboBox.obj" \
	"$(INTDIR)\ControlDate.obj" \
	"$(INTDIR)\ControlEdit.obj" \
	"$(INTDIR)\ControlFilteredList.obj" \
	"$(INTDIR)\ControlHorizontalLine.obj" \
	"$(INTDIR)\ControlImage.obj" \
	"$(INTDIR)\ControlLink.obj" \
	"$(INTDIR)\ControlListBox.obj" \
	"$(INTDIR)\ControlMenu.obj" \
	"$(INTDIR)\ControlProgressBar.obj" \
	"$(INTDIR)\ControlScrollBar.obj" \
	"$(INTDIR)\ControlStatus.obj" \
	"$(INTDIR)\ControlTime.obj" \
	"$(INTDIR)\DLLMain.obj" \
	"$(INTDIR)\FontCache.obj" \
	"$(INTDIR)\JPeg.obj" \
	"$(INTDIR)\MessageBox.obj" \
	"$(INTDIR)\MMLInterpret.obj" \
	"$(INTDIR)\MMLParse.obj" \
	"$(INTDIR)\Paint.obj" \
	"$(INTDIR)\Register.obj" \
	"$(INTDIR)\Render.obj" \
	"$(INTDIR)\ResLeak.obj" \
	"$(INTDIR)\TextWrap.obj" \
	"$(INTDIR)\Tools.obj" \
	"$(INTDIR)\Test.res"

"$(OUTDIR)\Escarpment.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"
DS_POSTBUILD_DEP=$(INTDIR)\postbld.dep

ALL : $(DS_POSTBUILD_DEP)

# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

$(DS_POSTBUILD_DEP) : "$(OUTDIR)\Escarpment.dll"
   copy z:\escarpment\escarpment\escarpment.h z:\include
	copy z:\escarpment\escarpment\debug\escarpment.dll z:\bin
	copy z:\escarpment\escarpment\debug\escarpment.lib z:\lib
	echo Helper for Post-build step > "$(DS_POSTBUILD_DEP)"

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
!IF EXISTS("Escarpment.dep")
!INCLUDE "Escarpment.dep"
!ELSE 
!MESSAGE Warning: cannot find "Escarpment.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "Escarpment - Win32 Release" || "$(CFG)" == "Escarpment - Win32 Debug"
SOURCE=.\BitmapCache.cpp

"$(INTDIR)\BitmapCache.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CEscControl.cpp

"$(INTDIR)\CEscControl.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CEscPage.cpp

"$(INTDIR)\CEscPage.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CEscSearch.cpp

"$(INTDIR)\CEscSearch.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CEscWindow.cpp

"$(INTDIR)\CEscWindow.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Control3D.cpp

"$(INTDIR)\Control3D.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlButton.cpp

"$(INTDIR)\ControlButton.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlChart.cpp

"$(INTDIR)\ControlChart.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlColorBlend.cpp

"$(INTDIR)\ControlColorBlend.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlComboBox.cpp

"$(INTDIR)\ControlComboBox.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlDate.cpp

"$(INTDIR)\ControlDate.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlEdit.cpp

"$(INTDIR)\ControlEdit.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ControlFilteredList.cpp

"$(INTDIR)\ControlFilteredList.obj" : $(SOURCE) "$(INTDIR)"


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


SOURCE=.\ControlTime.cpp

"$(INTDIR)\ControlTime.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\DLLMain.cpp

"$(INTDIR)\DLLMain.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\FontCache.cpp

"$(INTDIR)\FontCache.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\JPeg.cpp

"$(INTDIR)\JPeg.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\MessageBox.cpp

"$(INTDIR)\MessageBox.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\MMLInterpret.cpp

"$(INTDIR)\MMLInterpret.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\MMLParse.cpp

"$(INTDIR)\MMLParse.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Paint.cpp

"$(INTDIR)\Paint.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Register.cpp

"$(INTDIR)\Register.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Render.cpp

"$(INTDIR)\Render.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ResLeak.cpp

"$(INTDIR)\ResLeak.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Test.rc

"$(INTDIR)\Test.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\TextWrap.cpp

"$(INTDIR)\TextWrap.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Tools.cpp

"$(INTDIR)\Tools.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

