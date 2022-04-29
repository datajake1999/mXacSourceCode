# Microsoft Developer Studio Project File - Name="Escarpment" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Escarpment - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Escarpment.mak".
!MESSAGE 
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

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Escarpment - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "ESCARPMENT_EXPORTS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "ESCARPMENT_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 z:\jpeg\jpeg-6b\JPegLib\Release\JPegLib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /base:"0x61800000" /dll /machine:I386
# SUBTRACT LINK32 /debug
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy z:\escarpment\escarpment\escarpment.h z:\include	copy z:\escarpment\escarpment\release\escarpment.dll z:\bin	copy z:\escarpment\escarpment\release\escarpment.lib z:\lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "Escarpment - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "ESCARPMENT_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "ESCARPMENT_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 z:\jpeg\jpeg-6b\JPegLib\Debug\JPegLib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /base:"0x61800000" /dll /debug /machine:I386 /pdbtype:sept
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy z:\escarpment\escarpment\escarpment.h z:\include	copy z:\escarpment\escarpment\debug\escarpment.dll z:\bin	copy z:\escarpment\escarpment\debug\escarpment.lib z:\lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "Escarpment - Win32 Release"
# Name "Escarpment - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\BitmapCache.cpp
# End Source File
# Begin Source File

SOURCE=.\CEscControl.cpp
# End Source File
# Begin Source File

SOURCE=.\CEscPage.cpp
# End Source File
# Begin Source File

SOURCE=.\CEscSearch.cpp
# End Source File
# Begin Source File

SOURCE=.\CEscWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\Control3D.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlButton.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlChart.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlColorBlend.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlComboBox.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlDate.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlFilteredList.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlHorizontalLine.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlImage.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlLink.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlListBox.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlProgressBar.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlScrollBar.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlStatus.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlTime.cpp
# End Source File
# Begin Source File

SOURCE=.\DLLMain.cpp
# End Source File
# Begin Source File

SOURCE=.\FontCache.cpp
# End Source File
# Begin Source File

SOURCE=.\JPeg.cpp
# End Source File
# Begin Source File

SOURCE=.\MessageBox.cpp
# End Source File
# Begin Source File

SOURCE=.\MMLInterpret.cpp
# End Source File
# Begin Source File

SOURCE=.\MMLParse.cpp
# End Source File
# Begin Source File

SOURCE=.\Paint.cpp
# End Source File
# Begin Source File

SOURCE=.\Register.cpp
# End Source File
# Begin Source File

SOURCE=.\Render.cpp
# End Source File
# Begin Source File

SOURCE=.\ResLeak.cpp
# End Source File
# Begin Source File

SOURCE=.\Test.rc
# End Source File
# Begin Source File

SOURCE=.\TextWrap.cpp
# End Source File
# Begin Source File

SOURCE=.\Tools.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\Control.h
# End Source File
# Begin Source File

SOURCE=.\Escarpment.h
# End Source File
# Begin Source File

SOURCE=.\FontCache.h
# End Source File
# Begin Source File

SOURCE=.\JPeg.h
# End Source File
# Begin Source File

SOURCE=.\MMLInterpret.h
# End Source File
# Begin Source File

SOURCE=.\MMLParse.h
# End Source File
# Begin Source File

SOURCE=.\MyMalloc.h
# End Source File
# Begin Source File

SOURCE=.\Paint.h
# End Source File
# Begin Source File

SOURCE=.\Render.h
# End Source File
# Begin Source File

SOURCE=.\ResLeak.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\TextWrap.h
# End Source File
# Begin Source File

SOURCE=.\Tools.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\cursor1.cur
# End Source File
# Begin Source File

SOURCE=.\handc1.cur
# End Source File
# Begin Source File

SOURCE=.\icon1.ico
# End Source File
# End Group
# Begin Source File

SOURCE=.\CalcIndex.mml
# End Source File
# Begin Source File

SOURCE=.\MBDefault.mml
# End Source File
# End Target
# End Project
