# Microsoft Developer Studio Generated NMAKE File, Based on Dragonfly.dsp
!IF "$(CFG)" == ""
CFG=Dragonfly - Win32 Debug
!MESSAGE No configuration specified. Defaulting to Dragonfly - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Dragonfly - Win32 Release" && "$(CFG)" != "Dragonfly - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Dragonfly.mak" CFG="Dragonfly - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Dragonfly - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "Dragonfly - Win32 Debug" (based on "Win32 (x86) Application")
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

!IF  "$(CFG)" == "Dragonfly - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\Dragonfly.exe"


CLEAN :
	-@erase "$(INTDIR)\Archive.obj"
	-@erase "$(INTDIR)\Billing.obj"
	-@erase "$(INTDIR)\Calendar.obj"
	-@erase "$(INTDIR)\CDatabase.obj"
	-@erase "$(INTDIR)\CEncrypt.obj"
	-@erase "$(INTDIR)\DeepThought.obj"
	-@erase "$(INTDIR)\Dragonfly.res"
	-@erase "$(INTDIR)\Event.obj"
	-@erase "$(INTDIR)\Favorites.obj"
	-@erase "$(INTDIR)\Journal.obj"
	-@erase "$(INTDIR)\LogOn.obj"
	-@erase "$(INTDIR)\Main.obj"
	-@erase "$(INTDIR)\Meetings.obj"
	-@erase "$(INTDIR)\Memory.obj"
	-@erase "$(INTDIR)\Misc.obj"
	-@erase "$(INTDIR)\Notes.obj"
	-@erase "$(INTDIR)\Password.obj"
	-@erase "$(INTDIR)\People.obj"
	-@erase "$(INTDIR)\Phone.obj"
	-@erase "$(INTDIR)\Photos.obj"
	-@erase "$(INTDIR)\Planner.obj"
	-@erase "$(INTDIR)\Project.obj"
	-@erase "$(INTDIR)\Register.obj"
	-@erase "$(INTDIR)\Reminders.obj"
	-@erase "$(INTDIR)\Search.obj"
	-@erase "$(INTDIR)\Strings.obj"
	-@erase "$(INTDIR)\Tasks.obj"
	-@erase "$(INTDIR)\Time.obj"
	-@erase "$(INTDIR)\Today.obj"
	-@erase "$(INTDIR)\Util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\WrapUp.obj"
	-@erase "$(OUTDIR)\Dragonfly.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O1 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\Dragonfly.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\Dragonfly.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\Dragonfly.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib tapi32.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\Dragonfly.pdb" /machine:I386 /out:"$(OUTDIR)\Dragonfly.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Archive.obj" \
	"$(INTDIR)\Billing.obj" \
	"$(INTDIR)\Calendar.obj" \
	"$(INTDIR)\CDatabase.obj" \
	"$(INTDIR)\CEncrypt.obj" \
	"$(INTDIR)\DeepThought.obj" \
	"$(INTDIR)\Event.obj" \
	"$(INTDIR)\Favorites.obj" \
	"$(INTDIR)\Journal.obj" \
	"$(INTDIR)\LogOn.obj" \
	"$(INTDIR)\Main.obj" \
	"$(INTDIR)\Meetings.obj" \
	"$(INTDIR)\Memory.obj" \
	"$(INTDIR)\Misc.obj" \
	"$(INTDIR)\Notes.obj" \
	"$(INTDIR)\Password.obj" \
	"$(INTDIR)\People.obj" \
	"$(INTDIR)\Phone.obj" \
	"$(INTDIR)\Photos.obj" \
	"$(INTDIR)\Planner.obj" \
	"$(INTDIR)\Project.obj" \
	"$(INTDIR)\Register.obj" \
	"$(INTDIR)\Reminders.obj" \
	"$(INTDIR)\Search.obj" \
	"$(INTDIR)\Strings.obj" \
	"$(INTDIR)\Tasks.obj" \
	"$(INTDIR)\Time.obj" \
	"$(INTDIR)\Today.obj" \
	"$(INTDIR)\Util.obj" \
	"$(INTDIR)\WrapUp.obj" \
	"$(INTDIR)\Dragonfly.res"

"$(OUTDIR)\Dragonfly.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Dragonfly - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\Dragonfly.exe"


CLEAN :
	-@erase "$(INTDIR)\Archive.obj"
	-@erase "$(INTDIR)\Billing.obj"
	-@erase "$(INTDIR)\Calendar.obj"
	-@erase "$(INTDIR)\CDatabase.obj"
	-@erase "$(INTDIR)\CEncrypt.obj"
	-@erase "$(INTDIR)\DeepThought.obj"
	-@erase "$(INTDIR)\Dragonfly.res"
	-@erase "$(INTDIR)\Event.obj"
	-@erase "$(INTDIR)\Favorites.obj"
	-@erase "$(INTDIR)\Journal.obj"
	-@erase "$(INTDIR)\LogOn.obj"
	-@erase "$(INTDIR)\Main.obj"
	-@erase "$(INTDIR)\Meetings.obj"
	-@erase "$(INTDIR)\Memory.obj"
	-@erase "$(INTDIR)\Misc.obj"
	-@erase "$(INTDIR)\Notes.obj"
	-@erase "$(INTDIR)\Password.obj"
	-@erase "$(INTDIR)\People.obj"
	-@erase "$(INTDIR)\Phone.obj"
	-@erase "$(INTDIR)\Photos.obj"
	-@erase "$(INTDIR)\Planner.obj"
	-@erase "$(INTDIR)\Project.obj"
	-@erase "$(INTDIR)\Register.obj"
	-@erase "$(INTDIR)\Reminders.obj"
	-@erase "$(INTDIR)\Search.obj"
	-@erase "$(INTDIR)\Strings.obj"
	-@erase "$(INTDIR)\Tasks.obj"
	-@erase "$(INTDIR)\Time.obj"
	-@erase "$(INTDIR)\Today.obj"
	-@erase "$(INTDIR)\Util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\WrapUp.obj"
	-@erase "$(OUTDIR)\Dragonfly.exe"
	-@erase "$(OUTDIR)\Dragonfly.ilk"
	-@erase "$(OUTDIR)\Dragonfly.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\Dragonfly.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\Dragonfly.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\Dragonfly.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib tapi32.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\Dragonfly.pdb" /debug /machine:I386 /out:"$(OUTDIR)\Dragonfly.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\Archive.obj" \
	"$(INTDIR)\Billing.obj" \
	"$(INTDIR)\Calendar.obj" \
	"$(INTDIR)\CDatabase.obj" \
	"$(INTDIR)\CEncrypt.obj" \
	"$(INTDIR)\DeepThought.obj" \
	"$(INTDIR)\Event.obj" \
	"$(INTDIR)\Favorites.obj" \
	"$(INTDIR)\Journal.obj" \
	"$(INTDIR)\LogOn.obj" \
	"$(INTDIR)\Main.obj" \
	"$(INTDIR)\Meetings.obj" \
	"$(INTDIR)\Memory.obj" \
	"$(INTDIR)\Misc.obj" \
	"$(INTDIR)\Notes.obj" \
	"$(INTDIR)\Password.obj" \
	"$(INTDIR)\People.obj" \
	"$(INTDIR)\Phone.obj" \
	"$(INTDIR)\Photos.obj" \
	"$(INTDIR)\Planner.obj" \
	"$(INTDIR)\Project.obj" \
	"$(INTDIR)\Register.obj" \
	"$(INTDIR)\Reminders.obj" \
	"$(INTDIR)\Search.obj" \
	"$(INTDIR)\Strings.obj" \
	"$(INTDIR)\Tasks.obj" \
	"$(INTDIR)\Time.obj" \
	"$(INTDIR)\Today.obj" \
	"$(INTDIR)\Util.obj" \
	"$(INTDIR)\WrapUp.obj" \
	"$(INTDIR)\Dragonfly.res"

"$(OUTDIR)\Dragonfly.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("Dragonfly.dep")
!INCLUDE "Dragonfly.dep"
!ELSE 
!MESSAGE Warning: cannot find "Dragonfly.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "Dragonfly - Win32 Release" || "$(CFG)" == "Dragonfly - Win32 Debug"
SOURCE=.\Archive.cpp

"$(INTDIR)\Archive.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Billing.cpp

"$(INTDIR)\Billing.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Calendar.cpp

"$(INTDIR)\Calendar.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CDatabase.cpp

"$(INTDIR)\CDatabase.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CEncrypt.cpp

"$(INTDIR)\CEncrypt.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\DeepThought.cpp

"$(INTDIR)\DeepThought.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Dragonfly.rc

"$(INTDIR)\Dragonfly.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\Event.cpp

"$(INTDIR)\Event.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Favorites.cpp

"$(INTDIR)\Favorites.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Journal.cpp

"$(INTDIR)\Journal.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\LogOn.cpp

"$(INTDIR)\LogOn.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Main.cpp

"$(INTDIR)\Main.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Meetings.cpp

"$(INTDIR)\Meetings.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Memory.cpp

"$(INTDIR)\Memory.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Misc.cpp

"$(INTDIR)\Misc.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Notes.cpp

"$(INTDIR)\Notes.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Password.cpp

"$(INTDIR)\Password.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\People.cpp

"$(INTDIR)\People.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Phone.cpp

"$(INTDIR)\Phone.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Photos.cpp

"$(INTDIR)\Photos.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Planner.cpp

"$(INTDIR)\Planner.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Project.cpp

"$(INTDIR)\Project.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Register.cpp

"$(INTDIR)\Register.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Reminders.cpp

"$(INTDIR)\Reminders.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Search.cpp

"$(INTDIR)\Search.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Strings.cpp

"$(INTDIR)\Strings.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Tasks.cpp

"$(INTDIR)\Tasks.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Time.cpp

"$(INTDIR)\Time.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Today.cpp

"$(INTDIR)\Today.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Util.cpp

"$(INTDIR)\Util.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\WrapUp.cpp

"$(INTDIR)\WrapUp.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

