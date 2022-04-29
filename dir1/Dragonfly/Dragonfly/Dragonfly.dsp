# Microsoft Developer Studio Project File - Name="Dragonfly" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=Dragonfly - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Dragonfly.mak".
!MESSAGE 
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

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Dragonfly - Win32 Release"

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
# ADD CPP /nologo /W3 /GX /O1 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib tapi32.lib /nologo /subsystem:windows /machine:I386
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "Dragonfly - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib tapi32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Dragonfly - Win32 Release"
# Name "Dragonfly - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\Archive.cpp
# End Source File
# Begin Source File

SOURCE=.\Billing.cpp
# End Source File
# Begin Source File

SOURCE=.\Calendar.cpp
# End Source File
# Begin Source File

SOURCE=.\CDatabase.cpp
# End Source File
# Begin Source File

SOURCE=.\CEncrypt.cpp
# End Source File
# Begin Source File

SOURCE=.\DeepThought.cpp
# End Source File
# Begin Source File

SOURCE=.\Dragonfly.rc
# End Source File
# Begin Source File

SOURCE=.\Event.cpp
# End Source File
# Begin Source File

SOURCE=.\Favorites.cpp
# End Source File
# Begin Source File

SOURCE=.\Journal.cpp
# End Source File
# Begin Source File

SOURCE=.\LogOn.cpp
# End Source File
# Begin Source File

SOURCE=.\Main.cpp
# End Source File
# Begin Source File

SOURCE=.\Meetings.cpp
# End Source File
# Begin Source File

SOURCE=.\Memory.cpp
# End Source File
# Begin Source File

SOURCE=.\Misc.cpp
# End Source File
# Begin Source File

SOURCE=.\Notes.cpp
# End Source File
# Begin Source File

SOURCE=.\Password.cpp
# End Source File
# Begin Source File

SOURCE=.\People.cpp
# End Source File
# Begin Source File

SOURCE=.\Phone.cpp
# End Source File
# Begin Source File

SOURCE=.\Photos.cpp
# End Source File
# Begin Source File

SOURCE=.\Planner.cpp
# End Source File
# Begin Source File

SOURCE=.\Project.cpp
# End Source File
# Begin Source File

SOURCE=.\Register.cpp
# End Source File
# Begin Source File

SOURCE=.\Reminders.cpp
# End Source File
# Begin Source File

SOURCE=.\Search.cpp
# End Source File
# Begin Source File

SOURCE=.\Strings.cpp
# End Source File
# Begin Source File

SOURCE=.\Tasks.cpp
# End Source File
# Begin Source File

SOURCE=.\Time.cpp
# End Source File
# Begin Source File

SOURCE=.\Today.cpp
# End Source File
# Begin Source File

SOURCE=.\Util.cpp
# End Source File
# Begin Source File

SOURCE=.\WrapUp.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\Dragonfly.h
# End Source File
# Begin Source File

SOURCE=..\..\Include\Escarpment.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\archive.bmp
# End Source File
# Begin Source File

SOURCE=.\Archive.jpg
# End Source File
# Begin Source File

SOURCE=.\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00001.bmp
# End Source File
# Begin Source File

SOURCE=.\Calendar.jpg
# End Source File
# Begin Source File

SOURCE=.\DeepThoughts.jpg
# End Source File
# Begin Source File

SOURCE=.\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\icon2.ico
# End Source File
# Begin Source File

SOURCE=.\Journal.jpg
# End Source File
# Begin Source File

SOURCE=.\krftpapr.jpg
# End Source File
# Begin Source File

SOURCE=.\LogIn.jpg
# End Source File
# Begin Source File

SOURCE=.\Logo150.jpg
# End Source File
# Begin Source File

SOURCE=.\Meeting.jpg
# End Source File
# Begin Source File

SOURCE=.\MemoryLane.jpg
# End Source File
# Begin Source File

SOURCE=.\Misc.jpg
# End Source File
# Begin Source File

SOURCE=.\Notes.jpg
# End Source File
# Begin Source File

SOURCE=.\People.jpg
# End Source File
# Begin Source File

SOURCE=.\Phone.jpg
# End Source File
# Begin Source File

SOURCE=.\Projects.jpg
# End Source File
# Begin Source File

SOURCE=.\Reminders.jpg
# End Source File
# Begin Source File

SOURCE=.\Search.jpg
# End Source File
# Begin Source File

SOURCE=.\spacearr.bmp
# End Source File
# Begin Source File

SOURCE=.\Tasks.jpg
# End Source File
# Begin Source File

SOURCE=.\Today.jpg
# End Source File
# Begin Source File

SOURCE=.\uparrow1.bmp
# End Source File
# Begin Source File

SOURCE=.\world1.bmp
# End Source File
# Begin Source File

SOURCE=.\WrapUp.jpg
# End Source File
# End Group
# Begin Source File

SOURCE=.\About.mml
# End Source File
# Begin Source File

SOURCE=.\Alarm.mml
# End Source File
# Begin Source File

SOURCE=.\Analysis.mml
# End Source File
# Begin Source File

SOURCE=.\Archive.mml
# End Source File
# Begin Source File

SOURCE=.\ArchiveEdit.mml
# End Source File
# Begin Source File

SOURCE=.\ArchiveInfo.mml
# End Source File
# Begin Source File

SOURCE=.\ArchiveList.mml
# End Source File
# Begin Source File

SOURCE=.\ArchiveOptions.mml
# End Source File
# Begin Source File

SOURCE=.\ArchiveView.mml
# End Source File
# Begin Source File

SOURCE=.\Backup.mml
# End Source File
# Begin Source File

SOURCE=.\Billing.mml
# End Source File
# Begin Source File

SOURCE=.\Bugs.mml
# End Source File
# Begin Source File

SOURCE=.\BusinessAddEditInclude.mml
# End Source File
# Begin Source File

SOURCE=.\BusinessEdit.mml
# End Source File
# Begin Source File

SOURCE=.\BusinessQuickAdd.mml
# End Source File
# Begin Source File

SOURCE=.\BusinessView.mml
# End Source File
# Begin Source File

SOURCE=.\calendar.mml
# End Source File
# Begin Source File

SOURCE=.\CalendarHolidays.mml
# End Source File
# Begin Source File

SOURCE=.\CalendarInfo.mml
# End Source File
# Begin Source File

SOURCE=.\CalendarList.mml
# End Source File
# Begin Source File

SOURCE=.\CalendarLog.mml
# End Source File
# Begin Source File

SOURCE=.\CalendarSummary.mml
# End Source File
# Begin Source File

SOURCE=.\CalendarYearly.mml
# End Source File
# Begin Source File

SOURCE=.\CalendarYearly2.mml
# End Source File
# Begin Source File

SOURCE=.\CalenderCombo.mml
# End Source File
# Begin Source File

SOURCE=.\ChangePassword.mml
# End Source File
# Begin Source File

SOURCE=.\DeepChange.mml
# End Source File
# Begin Source File

SOURCE=.\DeepLong.mml
# End Source File
# Begin Source File

SOURCE=.\DeepPeople.mml
# End Source File
# Begin Source File

SOURCE=.\DeepShort.mml
# End Source File
# Begin Source File

SOURCE=.\DeepThoughts.mml
# End Source File
# Begin Source File

SOURCE=.\DeepWorld.mml
# End Source File
# Begin Source File

SOURCE=.\Event.mml
# End Source File
# Begin Source File

SOURCE=.\EventAdd.mml
# End Source File
# Begin Source File

SOURCE=.\EventEdit.mml
# End Source File
# Begin Source File

SOURCE=.\Favorites.mml
# End Source File
# Begin Source File

SOURCE=.\FavoritesAdd.mml
# End Source File
# Begin Source File

SOURCE=.\FavoritesEdit.mml
# End Source File
# Begin Source File

SOURCE=.\History.mml
# End Source File
# Begin Source File

SOURCE=.\History1.mml
# End Source File
# Begin Source File

SOURCE=.\HowLong.mml
# End Source File
# Begin Source File

SOURCE=.\IfYouLike.mml
# End Source File
# Begin Source File

SOURCE=.\journal.mml
# End Source File
# Begin Source File

SOURCE=.\JournalCat.mml
# End Source File
# Begin Source File

SOURCE=.\JournalCatAdd.mml
# End Source File
# Begin Source File

SOURCE=.\JournalCatRemove.mml
# End Source File
# Begin Source File

SOURCE=.\JournalEntryEdit.mml
# End Source File
# Begin Source File

SOURCE=.\JournalEntryView.mml
# End Source File
# Begin Source File

SOURCE=.\JournalInfo.mml
# End Source File
# Begin Source File

SOURCE=.\JournalQuickAdd.mml
# End Source File
# Begin Source File

SOURCE=.\JournalTimerEnd.mml
# End Source File
# Begin Source File

SOURCE=.\LogOnAskUser.mml
# End Source File
# Begin Source File

SOURCE=.\LogOnDelete.mml
# End Source File
# Begin Source File

SOURCE=.\LogOnFirstTime.mml
# End Source File
# Begin Source File

SOURCE=.\LogOnFromBackup.mml
# End Source File
# Begin Source File

SOURCE=.\LogOnNewUser.mml
# End Source File
# Begin Source File

SOURCE=.\Macros.mml
# End Source File
# Begin Source File

SOURCE=.\MeetingActionOther.mml
# End Source File
# Begin Source File

SOURCE=.\MeetingActionSelf.mml
# End Source File
# Begin Source File

SOURCE=.\MeetingAdd.mml
# End Source File
# Begin Source File

SOURCE=.\MeetingAddReoccur.mml
# End Source File
# Begin Source File

SOURCE=.\MeetingEdit.mml
# End Source File
# Begin Source File

SOURCE=.\MeetingEditReoccur.mml
# End Source File
# Begin Source File

SOURCE=.\MeetingInfo.mml
# End Source File
# Begin Source File

SOURCE=.\MeetingNotesEdit.mml
# End Source File
# Begin Source File

SOURCE=.\MeetingNotesView.mml
# End Source File
# Begin Source File

SOURCE=.\meetings.mml
# End Source File
# Begin Source File

SOURCE=.\MemoryEdit.mml
# End Source File
# Begin Source File

SOURCE=.\MemoryInfo.mml
# End Source File
# Begin Source File

SOURCE=.\MemoryLane.mml
# End Source File
# Begin Source File

SOURCE=.\MemoryList.mml
# End Source File
# Begin Source File

SOURCE=.\MemoryQuickAdd.mml
# End Source File
# Begin Source File

SOURCE=.\MemorySuggest.mml
# End Source File
# Begin Source File

SOURCE=.\MemoryView.mml
# End Source File
# Begin Source File

SOURCE=.\Misc.mml
# End Source File
# Begin Source File

SOURCE=.\MiscUI.mml
# End Source File
# Begin Source File

SOURCE=.\NewBusiness.mml
# End Source File
# Begin Source File

SOURCE=.\NewPerson.mml
# End Source File
# Begin Source File

SOURCE=.\Notes.mml
# End Source File
# Begin Source File

SOURCE=.\NotesAddCategory.mml
# End Source File
# Begin Source File

SOURCE=.\NotesEdit.mml
# End Source File
# Begin Source File

SOURCE=.\NotesQuickAdd.mml
# End Source File
# Begin Source File

SOURCE=.\NumericalRecords.mml
# End Source File
# Begin Source File

SOURCE=.\nyi.mml
# End Source File
# Begin Source File

SOURCE=.\Overview.mml
# End Source File
# Begin Source File

SOURCE=.\pageerror.mml
# End Source File
# Begin Source File

SOURCE=.\Password.mml
# End Source File
# Begin Source File

SOURCE=.\PasswordAdd.mml
# End Source File
# Begin Source File

SOURCE=.\PasswordEdit.mml
# End Source File
# Begin Source File

SOURCE=.\PasswordSusped.mml
# End Source File
# Begin Source File

SOURCE=.\PasswordSuspendVerify.mml
# End Source File
# Begin Source File

SOURCE=.\PasswordVerify.mml
# End Source File
# Begin Source File

SOURCE=.\people.mml
# End Source File
# Begin Source File

SOURCE=.\PeopleExport.mml
# End Source File
# Begin Source File

SOURCE=.\PeopleImport.mml
# End Source File
# Begin Source File

SOURCE=.\PeopleInfo.mml
# End Source File
# Begin Source File

SOURCE=.\PeopleList.mml
# End Source File
# Begin Source File

SOURCE=.\PeopleLookUp.mml
# End Source File
# Begin Source File

SOURCE=.\PeoplePrint.mml
# End Source File
# Begin Source File

SOURCE=.\PeopleRemove.mml
# End Source File
# Begin Source File

SOURCE=.\PersonAddEditInclude.mml
# End Source File
# Begin Source File

SOURCE=.\PersonBusinessQuickAdd.mml
# End Source File
# Begin Source File

SOURCE=.\PersonEdit.mml
# End Source File
# Begin Source File

SOURCE=.\PersonQuickAdd.mml
# End Source File
# Begin Source File

SOURCE=.\PersonView.mml
# End Source File
# Begin Source File

SOURCE=.\Phone.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneCallLog.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneInfo.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneMakeCall.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneNotesEdit.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneNotesView.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneSchedAdd.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneSchedAddReoccur.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneSchedEdit.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneSchedEditReoccur.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneSpeedNew.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneSpeedRemove.mml
# End Source File
# Begin Source File

SOURCE=.\PhoneWhichNumber.mml
# End Source File
# Begin Source File

SOURCE=.\Photos.mml
# End Source File
# Begin Source File

SOURCE=.\PhotosAdd.mml
# End Source File
# Begin Source File

SOURCE=.\PhotosEntryEdit.mml
# End Source File
# Begin Source File

SOURCE=.\PhotosEntryView.mml
# End Source File
# Begin Source File

SOURCE=.\PhotosInfo.mml
# End Source File
# Begin Source File

SOURCE=.\PhotosList.mml
# End Source File
# Begin Source File

SOURCE=.\places.mml
# End Source File
# Begin Source File

SOURCE=.\Planner.mml
# End Source File
# Begin Source File

SOURCE=.\PlannerBreaks.mml
# End Source File
# Begin Source File

SOURCE=.\PlannerInfo.mml
# End Source File
# Begin Source File

SOURCE=.\PlannerMenu.mml
# End Source File
# Begin Source File

SOURCE=.\Project.mml
# End Source File
# Begin Source File

SOURCE=.\ProjectAdd.mml
# End Source File
# Begin Source File

SOURCE=.\ProjectInfo.mml
# End Source File
# Begin Source File

SOURCE=.\ProjectNewSubProject.mml
# End Source File
# Begin Source File

SOURCE=.\ProjectRemove.mml
# End Source File
# Begin Source File

SOURCE=.\ProjectTaskAdd.mml
# End Source File
# Begin Source File

SOURCE=.\ProjectTaskAddMultiple.mml
# End Source File
# Begin Source File

SOURCE=.\ProjectTaskEdit.mml
# End Source File
# Begin Source File

SOURCE=.\ProjectTaskSplit.mml
# End Source File
# Begin Source File

SOURCE=.\ProjectView.mml
# End Source File
# Begin Source File

SOURCE=.\Quotes.mml
# End Source File
# Begin Source File

SOURCE=.\Register.mml
# End Source File
# Begin Source File

SOURCE=.\ReminderAddReoccur.mml
# End Source File
# Begin Source File

SOURCE=.\ReminderInfo.mml
# End Source File
# Begin Source File

SOURCE=.\ReminderQuickAdd.mml
# End Source File
# Begin Source File

SOURCE=.\Reminders.mml
# End Source File
# Begin Source File

SOURCE=.\Reoccur.mml
# End Source File
# Begin Source File

SOURCE=.\Search.mml
# End Source File
# Begin Source File

SOURCE=.\SearchInfo.mml
# End Source File
# Begin Source File

SOURCE=.\TaskAdd.mml
# End Source File
# Begin Source File

SOURCE=.\TaskAddReoccur.mml
# End Source File
# Begin Source File

SOURCE=.\TaskEdit.mml
# End Source File
# Begin Source File

SOURCE=.\TaskEditReoccur.mml
# End Source File
# Begin Source File

SOURCE=.\TaskInfo.mml
# End Source File
# Begin Source File

SOURCE=.\Tasks.mml
# End Source File
# Begin Source File

SOURCE=.\Template.mml
# End Source File
# Begin Source File

SOURCE=.\Template2.mml
# End Source File
# Begin Source File

SOURCE=.\Template3.mml
# End Source File
# Begin Source File

SOURCE=.\templateR.mml
# End Source File
# Begin Source File

SOURCE=.\Theory.mml
# End Source File
# Begin Source File

SOURCE=.\TimeMoon.mml
# End Source File
# Begin Source File

SOURCE=.\TimeSunlight.mml
# End Source File
# Begin Source File

SOURCE=.\TimeZoneAdd.mml
# End Source File
# Begin Source File

SOURCE=.\TimeZones.mml
# End Source File
# Begin Source File

SOURCE=.\TOC.mml
# End Source File
# Begin Source File

SOURCE=.\Today.mml
# End Source File
# Begin Source File

SOURCE=.\TodayInfo.mml
# End Source File
# Begin Source File

SOURCE=.\Tomorrow.mml
# End Source File
# Begin Source File

SOURCE=.\tts.mml
# End Source File
# Begin Source File

SOURCE=.\WrapUp.mml
# End Source File
# Begin Source File

SOURCE=.\WrapUpEdit.mml
# End Source File
# Begin Source File

SOURCE=.\WrapUpView.mml
# End Source File
# End Target
# End Project
