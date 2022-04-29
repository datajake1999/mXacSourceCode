# Microsoft Developer Studio Project File - Name="EscHelp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=EscHelp - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "EscHelp.mak".
!MESSAGE 
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

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "EscHelp - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "EscHelp - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "EscHelp - Win32 Release"
# Name "EscHelp - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\EscHelp.cpp
# End Source File
# Begin Source File

SOURCE=.\EscHelp.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\Include\Escarpment.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\EscHelp.rct
# End Source File
# Begin Source File

SOURCE=.\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\Logo150.jpg
# End Source File
# Begin Source File

SOURCE=.\Waterfall.jpg
# End Source File
# End Group
# Begin Source File

SOURCE=.\AdvancedObjects.mml
# End Source File
# Begin Source File

SOURCE=.\bugs.mml
# End Source File
# Begin Source File

SOURCE=.\CEscControl.mml
# End Source File
# Begin Source File

SOURCE=.\CEscPage.mml
# End Source File
# Begin Source File

SOURCE=.\CEscSearch.mml
# End Source File
# Begin Source File

SOURCE=.\CEscWindow.mml
# End Source File
# Begin Source File

SOURCE=.\ContButton.mml
# End Source File
# Begin Source File

SOURCE=.\ContChart.mml
# End Source File
# Begin Source File

SOURCE=.\contcolorblend.mml
# End Source File
# Begin Source File

SOURCE=.\ContComboBox.mml
# End Source File
# Begin Source File

SOURCE=.\ContDate.mml
# End Source File
# Begin Source File

SOURCE=.\ContDefault.mml
# End Source File
# Begin Source File

SOURCE=.\ContEdit.mml
# End Source File
# Begin Source File

SOURCE=.\ContFilteredList.mml
# End Source File
# Begin Source File

SOURCE=.\ContHr.mml
# End Source File
# Begin Source File

SOURCE=.\ContImage.mml
# End Source File
# Begin Source File

SOURCE=.\ContListBox.mml
# End Source File
# Begin Source File

SOURCE=.\ContMenu.mml
# End Source File
# Begin Source File

SOURCE=.\ContProgressBar.mml
# End Source File
# Begin Source File

SOURCE=.\Controls.mml
# End Source File
# Begin Source File

SOURCE=.\ContScrollBar.mml
# End Source File
# Begin Source File

SOURCE=.\ContStatus.mml
# End Source File
# Begin Source File

SOURCE=.\ContThreeD.mml
# End Source File
# Begin Source File

SOURCE=.\ContTime.mml
# End Source File
# Begin Source File

SOURCE=.\Documentation.mml
# End Source File
# Begin Source File

SOURCE=".\Hello World.mml"
# End Source File
# Begin Source File

SOURCE=.\History.mml
# End Source File
# Begin Source File

SOURCE=.\Macros.mml
# End Source File
# Begin Source File

SOURCE=.\Main.mml
# End Source File
# Begin Source File

SOURCE=.\MMLAdvanced.mml
# End Source File
# Begin Source File

SOURCE=.\MMLBasics.mml
# End Source File
# Begin Source File

SOURCE=.\Overview.mml
# End Source File
# Begin Source File

SOURCE=.\OwnControls.mml
# End Source File
# Begin Source File

SOURCE=.\Registration.mml
# End Source File
# Begin Source File

SOURCE=.\Samples.mml
# End Source File
# Begin Source File

SOURCE=.\Search.mml
# End Source File
# Begin Source File

SOURCE=.\Template.mml
# End Source File
# Begin Source File

SOURCE=.\UIGuide.mml
# End Source File
# Begin Source File

SOURCE=.\UtilityFunctions.mml
# End Source File
# Begin Source File

SOURCE=.\ViewSource.mml
# End Source File
# End Target
# End Project
