# Microsoft Developer Studio Project File - Name="M3D" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=M3D - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "M3D.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "M3D.mak" CFG="M3D - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "M3D - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "M3D - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "M3D - Win32 Release"

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
# ADD CPP /nologo /G6 /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# SUBTRACT CPP /Z<none>
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /machine:I386
# SUBTRACT LINK32 /profile /debug

!ELSEIF  "$(CFG)" == "M3D - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_TIMERS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /debug /machine:I386
# SUBTRACT LINK32 /profile

!ENDIF 

# Begin Target

# Name "M3D - Win32 Release"
# Name "M3D - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\CBalustrade.cpp
# End Source File
# Begin Source File

SOURCE=.\CButtonBar.cpp
# End Source File
# Begin Source File

SOURCE=.\CColumn.cpp
# End Source File
# Begin Source File

SOURCE=.\CDoor.cpp
# End Source File
# Begin Source File

SOURCE=.\CDoorFrame.cpp
# End Source File
# Begin Source File

SOURCE=.\CDoorOpening.cpp
# End Source File
# Begin Source File

SOURCE=.\CDoorSet.cpp
# End Source File
# Begin Source File

SOURCE=.\CDoubleSurface.cpp

!IF  "$(CFG)" == "M3D - Win32 Release"

# ADD CPP /O2

!ELSEIF  "$(CFG)" == "M3D - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\CHouseView.cpp
# End Source File
# Begin Source File

SOURCE=.\CIconButton.cpp
# End Source File
# Begin Source File

SOURCE=.\CImage.cpp
# End Source File
# Begin Source File

SOURCE=.\CImageShader.cpp
# End Source File
# Begin Source File

SOURCE=.\CLibrary.cpp
# End Source File
# Begin Source File

SOURCE=.\CLight.cpp
# End Source File
# Begin Source File

SOURCE=.\CNoodle.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectBalustrade.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectBooks.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectBuildBlock.cpp

!IF  "$(CFG)" == "M3D - Win32 Release"

# ADD CPP /O2

!ELSEIF  "$(CFG)" == "M3D - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\CObjectCabinet.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectCamera.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectCeilingFan.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectColumn.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectCurtain.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectCushion.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectDoor.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectDrawer.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectEditor.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectFireplace.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectGround.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectLight.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectNoodle.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectPainting.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectPathway.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectPiers.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectPolyhedron.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectPolyMesh.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectPool.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectRevolution.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectSingleSurface.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectStairs.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectStructSurface.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectTableCloth.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectTarp.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectTeapot.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectTemplate.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectTestBox.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectTree.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectTruss.cpp
# End Source File
# Begin Source File

SOURCE=.\CObjectUniHole.cpp
# End Source File
# Begin Source File

SOURCE=.\CPiers.cpp
# End Source File
# Begin Source File

SOURCE=.\CProgress.cpp
# End Source File
# Begin Source File

SOURCE=.\CRenderClip.cpp
# End Source File
# Begin Source File

SOURCE=.\CRenderMatrix.cpp
# End Source File
# Begin Source File

SOURCE=.\CRenderSurface.cpp
# End Source File
# Begin Source File

SOURCE=.\CRenderTraditional.cpp
# End Source File
# Begin Source File

SOURCE=.\CRevolution.cpp
# End Source File
# Begin Source File

SOURCE=.\CShadowBuf.cpp
# End Source File
# Begin Source File

SOURCE=.\CSingleSurface.cpp
# End Source File
# Begin Source File

SOURCE=.\CSpline.cpp
# End Source File
# Begin Source File

SOURCE=.\CSplineSurface.cpp

!IF  "$(CFG)" == "M3D - Win32 Release"

# ADD CPP /O2

!ELSEIF  "$(CFG)" == "M3D - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\CThumbnail.cpp
# End Source File
# Begin Source File

SOURCE=.\CToolTip.cpp
# End Source File
# Begin Source File

SOURCE=.\CWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\Help.cpp
# End Source File
# Begin Source File

SOURCE=.\Intersect.cpp
# End Source File
# Begin Source File

SOURCE=.\M3D.rc
# End Source File
# Begin Source File

SOURCE=.\main.cpp
# End Source File
# Begin Source File

SOURCE=.\Measure.cpp
# End Source File
# Begin Source File

SOURCE=.\MML.cpp
# End Source File
# Begin Source File

SOURCE=.\MMLCompress.cpp
# End Source File
# Begin Source File

SOURCE=.\ObjectCF.cpp
# End Source File
# Begin Source File

SOURCE=.\ResLeak.cpp
# End Source File
# Begin Source File

SOURCE=.\Sun.cpp
# End Source File
# Begin Source File

SOURCE=.\Textures.cpp
# End Source File
# Begin Source File

SOURCE=.\Util.cpp
# End Source File
# Begin Source File

SOURCE=.\VolumeIntersect.cpp
# End Source File
# Begin Source File

SOURCE=.\VolumeSort.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\Include\Escarpment.h
# End Source File
# Begin Source File

SOURCE=.\M3D.h
# End Source File
# Begin Source File

SOURCE=.\ResLeak.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\bitmap2.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00001.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00002.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00003.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00004.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00005.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00006.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00007.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00008.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00009.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00010.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00011.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00012.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00013.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00014.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00015.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00016.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00017.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00018.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00019.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00020.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00021.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00022.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00023.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00024.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00025.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00026.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00027.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00028.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00029.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00030.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00031.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00032.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00033.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00034.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00035.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00036.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00037.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00038.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00039.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00040.bmp
# End Source File
# Begin Source File

SOURCE=.\bmp00041.bmp
# End Source File
# Begin Source File

SOURCE=.\clipleve.bmp
# End Source File
# Begin Source File

SOURCE=.\clipline.bmp
# End Source File
# Begin Source File

SOURCE=.\close1.bmp
# End Source File
# Begin Source File

SOURCE=.\cur00001.cur
# End Source File
# Begin Source File

SOURCE=.\cur00002.cur
# End Source File
# Begin Source File

SOURCE=.\cur00003.cur
# End Source File
# Begin Source File

SOURCE=.\cur00004.cur
# End Source File
# Begin Source File

SOURCE=.\cur00005.cur
# End Source File
# Begin Source File

SOURCE=.\cur00006.cur
# End Source File
# Begin Source File

SOURCE=.\cur00007.cur
# End Source File
# Begin Source File

SOURCE=.\cur00008.cur
# End Source File
# Begin Source File

SOURCE=.\cur00009.cur
# End Source File
# Begin Source File

SOURCE=.\cur00010.cur
# End Source File
# Begin Source File

SOURCE=.\cur00011.cur
# End Source File
# Begin Source File

SOURCE=.\cur00012.cur
# End Source File
# Begin Source File

SOURCE=.\cur00013.cur
# End Source File
# Begin Source File

SOURCE=.\cur00014.cur
# End Source File
# Begin Source File

SOURCE=.\cur00015.cur
# End Source File
# Begin Source File

SOURCE=.\cur00016.cur
# End Source File
# Begin Source File

SOURCE=.\cur00017.cur
# End Source File
# Begin Source File

SOURCE=.\cur00018.cur
# End Source File
# Begin Source File

SOURCE=.\cur00019.cur
# End Source File
# Begin Source File

SOURCE=.\cur00020.cur
# End Source File
# Begin Source File

SOURCE=.\cur00021.cur
# End Source File
# Begin Source File

SOURCE=.\cur00022.cur
# End Source File
# Begin Source File

SOURCE=.\cur00023.cur
# End Source File
# Begin Source File

SOURCE=.\cur00024.cur
# End Source File
# Begin Source File

SOURCE=.\cur00025.cur
# End Source File
# Begin Source File

SOURCE=.\cur00026.cur
# End Source File
# Begin Source File

SOURCE=.\cur00027.cur
# End Source File
# Begin Source File

SOURCE=.\cur00028.cur
# End Source File
# Begin Source File

SOURCE=.\cur00029.cur
# End Source File
# Begin Source File

SOURCE=.\cur00030.cur
# End Source File
# Begin Source File

SOURCE=.\cur00031.cur
# End Source File
# Begin Source File

SOURCE=.\cur00032.cur
# End Source File
# Begin Source File

SOURCE=.\cursor1.cur
# End Source File
# Begin Source File

SOURCE=.\cursorcl.cur
# End Source File
# Begin Source File

SOURCE=.\cursordi.cur
# End Source File
# Begin Source File

SOURCE=.\cursorin.cur
# End Source File
# Begin Source File

SOURCE=.\cursormo.cur
# End Source File
# Begin Source File

SOURCE=.\cursorob.cur
# End Source File
# Begin Source File

SOURCE=.\cursorpa.cur
# End Source File
# Begin Source File

SOURCE=.\cursorvi.cur
# End Source File
# Begin Source File

SOURCE=.\cursorzo.cur
# End Source File
# Begin Source File

SOURCE=.\cut1.bmp
# End Source File
# Begin Source File

SOURCE=.\cutzoomi.bmp
# End Source File
# Begin Source File

SOURCE=.\dialogbo.bmp
# End Source File
# Begin Source File

SOURCE=.\EagleEye.jpg
# End Source File
# Begin Source File

SOURCE=.\gridsele.bmp
# End Source File
# Begin Source File

SOURCE=.\ico00001.ico
# End Source File
# Begin Source File

SOURCE=.\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\icon2.ico
# End Source File
# Begin Source File

SOURCE=.\iconarro.ico
# End Source File
# Begin Source File

SOURCE=.\library.bmp
# End Source File
# Begin Source File

SOURCE=.\lightdis.ico
# End Source File
# Begin Source File

SOURCE=.\Logo150.jpg
# End Source File
# Begin Source File

SOURCE=.\minimize.bmp
# End Source File
# Begin Source File

SOURCE=.\moveembe.bmp
# End Source File
# Begin Source File

SOURCE=.\movelrfb.bmp
# End Source File
# Begin Source File

SOURCE=.\movelrud.bmp
# End Source File
# Begin Source File

SOURCE=.\movensew.bmp
# End Source File
# Begin Source File

SOURCE=.\moveud1.bmp
# End Source File
# Begin Source File

SOURCE=.\new1.bmp
# End Source File
# Begin Source File

SOURCE=.\nextmoni.bmp
# End Source File
# Begin Source File

SOURCE=.\objcontr.bmp
# End Source File
# Begin Source File

SOURCE=.\objintel.bmp
# End Source File
# Begin Source File

SOURCE=.\objnew1.bmp
# End Source File
# Begin Source File

SOURCE=.\objonoff.bmp
# End Source File
# Begin Source File

SOURCE=.\objpaint.bmp
# End Source File
# Begin Source File

SOURCE=.\PaintingKite.jpg
# End Source File
# Begin Source File

SOURCE=.\PaintingMitch.jpg
# End Source File
# Begin Source File

SOURCE=.\PaintingPuggle.jpg
# End Source File
# Begin Source File

SOURCE=.\PaintingQuoll.jpg
# End Source File
# Begin Source File

SOURCE=.\PhotoKakadu.jpg
# End Source File
# Begin Source File

SOURCE=.\PhotoLetchworth.jpg
# End Source File
# Begin Source File

SOURCE=.\PhotoMindil.jpg
# End Source File
# Begin Source File

SOURCE=.\PhotoPayaso.jpg
# End Source File
# Begin Source File

SOURCE=.\PhotoUluru.jpg
# End Source File
# Begin Source File

SOURCE=.\selall1.bmp
# End Source File
# Begin Source File

SOURCE=.\selindiv.bmp
# End Source File
# Begin Source File

SOURCE=.\Splash.jpg
# End Source File
# Begin Source File

SOURCE=.\view3d1.bmp
# End Source File
# Begin Source File

SOURCE=.\view3dlr.bmp
# End Source File
# Begin Source File

SOURCE=.\view3dro.bmp
# End Source File
# Begin Source File

SOURCE=.\viewflat.bmp
# End Source File
# Begin Source File

SOURCE=.\viewwalk.bmp
# End Source File
# Begin Source File

SOURCE=.\World.bmp
# End Source File
# Begin Source File

SOURCE=.\zoomin1.bmp
# End Source File
# End Group
# Begin Source File

SOURCE=.\BalAppearance.mml
# End Source File
# Begin Source File

SOURCE=.\BalCorners.mml
# End Source File
# Begin Source File

SOURCE=.\BalCustom.mml
# End Source File
# Begin Source File

SOURCE=.\BalDialog.mml
# End Source File
# Begin Source File

SOURCE=.\BalDisplay.mml
# End Source File
# Begin Source File

SOURCE=.\BalOpenings.mml
# End Source File
# Begin Source File

SOURCE=.\BooksDialog.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockCeiling.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockDialog.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockDisplay.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockFloors.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockFoundation.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockIntel.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockLevels.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockRoof.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockVerandah.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockVerandahAny.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockWallAngle.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockWalls.mml
# End Source File
# Begin Source File

SOURCE=.\BuildBlockWallsAny.mml
# End Source File
# Begin Source File

SOURCE=.\CabinetDialog.mml
# End Source File
# Begin Source File

SOURCE=.\ClipLevels.mml
# End Source File
# Begin Source File

SOURCE=.\ClipPoint.mml
# End Source File
# Begin Source File

SOURCE=.\ClipSet.mml
# End Source File
# Begin Source File

SOURCE=.\ClipSetFloors.mml
# End Source File
# Begin Source File

SOURCE=.\ClipSetHand.mml
# End Source File
# Begin Source File

SOURCE=.\ClipShow.mml
# End Source File
# Begin Source File

SOURCE=.\ColorSel.mml
# End Source File
# Begin Source File

SOURCE=.\Column.mml
# End Source File
# Begin Source File

SOURCE=.\CurtainDialog.mml
# End Source File
# Begin Source File

SOURCE=.\CushionDialog.mml
# End Source File
# Begin Source File

SOURCE=.\DoorCustom.mml
# End Source File
# Begin Source File

SOURCE=.\DoorDialog.mml
# End Source File
# Begin Source File

SOURCE=.\DoorFrameShape.mml
# End Source File
# Begin Source File

SOURCE=.\DoorFrameWD.mml
# End Source File
# Begin Source File

SOURCE=.\DoorKnob.mml
# End Source File
# Begin Source File

SOURCE=.\DoorOpening.mml
# End Source File
# Begin Source File

SOURCE=.\DrawerDialog.mml
# End Source File
# Begin Source File

SOURCE=.\GridFromPoint.mml
# End Source File
# Begin Source File

SOURCE=.\GridSet.mml
# End Source File
# Begin Source File

SOURCE=.\GridValues.mml
# End Source File
# Begin Source File

SOURCE=.\GroundBoundary.mml
# End Source File
# Begin Source File

SOURCE=.\GroundBoundary2.mml
# End Source File
# Begin Source File

SOURCE=.\GroundCutoutAddInter.mml
# End Source File
# Begin Source File

SOURCE=.\GroundCutoutMain.mml
# End Source File
# Begin Source File

SOURCE=.\GroundDialog.mml
# End Source File
# Begin Source File

SOURCE=.\GroundDisplay.mml
# End Source File
# Begin Source File

SOURCE=.\GroundSize.mml
# End Source File
# Begin Source File

SOURCE=.\GroundWater.mml
# End Source File
# Begin Source File

SOURCE=.\habout.mml
# End Source File
# Begin Source File

SOURCE=.\hBalCustom.mml
# End Source File
# Begin Source File

SOURCE=.\HBalInBB.mml
# End Source File
# Begin Source File

SOURCE=.\hBalInStairs.mml
# End Source File
# Begin Source File

SOURCE=.\hBalObject.mml
# End Source File
# Begin Source File

SOURCE=.\HBBBypass.mml
# End Source File
# Begin Source File

SOURCE=.\HBBCathedral.mml
# End Source File
# Begin Source File

SOURCE=.\HBBDeconstruct.mml
# End Source File
# Begin Source File

SOURCE=.\HBBElevLimit.mml
# End Source File
# Begin Source File

SOURCE=.\HBBFoundation.mml
# End Source File
# Begin Source File

SOURCE=.\HBBLevels.mml
# End Source File
# Begin Source File

SOURCE=.\HBBOverlays.mml
# End Source File
# Begin Source File

SOURCE=.\HBBRoof.mml
# End Source File
# Begin Source File

SOURCE=.\HBBVerandah.mml
# End Source File
# Begin Source File

SOURCE=.\HBBWalls.mml
# End Source File
# Begin Source File

SOURCE=.\HBooks.mml
# End Source File
# Begin Source File

SOURCE=.\hBugs.mml
# End Source File
# Begin Source File

SOURCE=.\HCabinet.mml
# End Source File
# Begin Source File

SOURCE=.\HCabinetPieces.mml
# End Source File
# Begin Source File

SOURCE=.\HColumn.mml
# End Source File
# Begin Source File

SOURCE=.\HCurtain.mml
# End Source File
# Begin Source File

SOURCE=.\HCushion.mml
# End Source File
# Begin Source File

SOURCE=.\HDoorsCustom.mml
# End Source File
# Begin Source File

SOURCE=.\HDoorsOpen.mml
# End Source File
# Begin Source File

SOURCE=.\HDoorsSize.mml
# End Source File
# Begin Source File

SOURCE=.\HDrawer.mml
# End Source File
# Begin Source File

SOURCE=.\HExtrusion.mml
# End Source File
# Begin Source File

SOURCE=.\HFencing.mml
# End Source File
# Begin Source File

SOURCE=.\HFireplace.mml
# End Source File
# Begin Source File

SOURCE=.\HFog.mml
# End Source File
# Begin Source File

SOURCE=.\HFuture.mml
# End Source File
# Begin Source File

SOURCE=.\HGridCenter.mml
# End Source File
# Begin Source File

SOURCE=.\HGridDisplay.mml
# End Source File
# Begin Source File

SOURCE=.\HGridSize.mml
# End Source File
# Begin Source File

SOURCE=.\HGroundBoundary.mml
# End Source File
# Begin Source File

SOURCE=.\HGroundCountour.mml
# End Source File
# Begin Source File

SOURCE=.\HGroundSize.mml
# End Source File
# Begin Source File

SOURCE=.\HGroundWater.mml
# End Source File
# Begin Source File

SOURCE=.\HHistory.mml
# End Source File
# Begin Source File

SOURCE=.\HIfYouLike.mml
# End Source File
# Begin Source File

SOURCE=.\HLathe.mml
# End Source File
# Begin Source File

SOURCE=.\HLightCreate.mml
# End Source File
# Begin Source File

SOURCE=.\HLightCustom.mml
# End Source File
# Begin Source File

SOURCE=.\HLightOnOff.mml
# End Source File
# Begin Source File

SOURCE=.\HLightSun.mml
# End Source File
# Begin Source File

SOURCE=.\HLocale.mml
# End Source File
# Begin Source File

SOURCE=.\HMain.mml
# End Source File
# Begin Source File

SOURCE=.\HObjReference.mml
# End Source File
# Begin Source File

SOURCE=.\HPainting.mml
# End Source File
# Begin Source File

SOURCE=.\HPathway.mml
# End Source File
# Begin Source File

SOURCE=.\HPierCustom.mml
# End Source File
# Begin Source File

SOURCE=.\HPierInBB.mml
# End Source File
# Begin Source File

SOURCE=.\HPierObject.mml
# End Source File
# Begin Source File

SOURCE=.\HPlants.mml
# End Source File
# Begin Source File

SOURCE=.\HPlantsCustom.mml
# End Source File
# Begin Source File

SOURCE=.\HPolyhedron.mml
# End Source File
# Begin Source File

SOURCE=.\HPolyMesh.mml
# End Source File
# Begin Source File

SOURCE=.\HPool.mml
# End Source File
# Begin Source File

SOURCE=.\HProdCompare.mml
# End Source File
# Begin Source File

SOURCE=.\HRegister.mml
# End Source File
# Begin Source File

SOURCE=.\HSearch.mml
# End Source File
# Begin Source File

SOURCE=.\HSplineSurface.mml
# End Source File
# Begin Source File

SOURCE=.\HStairsCustom.mml
# End Source File
# Begin Source File

SOURCE=.\HStairsObject.mml
# End Source File
# Begin Source File

SOURCE=.\HSurfaceCurve.mml
# End Source File
# Begin Source File

SOURCE=.\HSurfaceEdge.mml
# End Source File
# Begin Source File

SOURCE=.\HSurfaceOverlay.mml
# End Source File
# Begin Source File

SOURCE=.\HSurfaceThick.mml
# End Source File
# Begin Source File

SOURCE=.\hSYstem.mml
# End Source File
# Begin Source File

SOURCE=.\HTableCloth.mml
# End Source File
# Begin Source File

SOURCE=.\HTarp.mml
# End Source File
# Begin Source File

SOURCE=.\HTBBath.mml
# End Source File
# Begin Source File

SOURCE=.\HTBKitchen.mml
# End Source File
# Begin Source File

SOURCE=.\HTextureCreate.mml
# End Source File
# Begin Source File

SOURCE=.\HTextureMaterial.mml
# End Source File
# Begin Source File

SOURCE=.\HTextureModify.mml
# End Source File
# Begin Source File

SOURCE=.\HThumbnail.mml
# End Source File
# Begin Source File

SOURCE=.\HTipArchitect.mml
# End Source File
# Begin Source File

SOURCE=.\HTipBuilder.mml
# End Source File
# Begin Source File

SOURCE=.\HTipLandlord.mml
# End Source File
# Begin Source File

SOURCE=.\HTipLandscape.mml
# End Source File
# Begin Source File

SOURCE=.\HTipPeopleBuild.mml
# End Source File
# Begin Source File

SOURCE=.\HTipPeopleLanscape.mml
# End Source File
# Begin Source File

SOURCE=.\HTOC.mml
# End Source File
# Begin Source File

SOURCE=.\HTruss.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialBuildBlock1.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialBuildBlock2.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialBuildBlock3.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialBuildBlock4.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialBuildBlock5.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialBuildSite.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialButtons.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialClip.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialDoors.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialFinished.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialFinished3.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialFinishedView.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialImageQuality.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialInside.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialObjCreate.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialObjEdit.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialObjEdit2.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialObjSave.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialObjStart.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialObjTemplate.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialObjTheory.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialOpenSample.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialOutside.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialSave.mml
# End Source File
# Begin Source File

SOURCE=.\hTutorialStart.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialTextures.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialViews.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialViews2.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialViews3.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialViews4.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialWalls.mml
# End Source File
# Begin Source File

SOURCE=.\HTutorialWalls2.mml
# End Source File
# Begin Source File

SOURCE=.\HViewExposure.mml
# End Source File
# Begin Source File

SOURCE=.\HViewGlasses.mml
# End Source File
# Begin Source File

SOURCE=.\HViewLighting.mml
# End Source File
# Begin Source File

SOURCE=.\HViewMultiple.mml
# End Source File
# Begin Source File

SOURCE=.\HViewOutlines.mml
# End Source File
# Begin Source File

SOURCE=.\HViewPerspective.mml
# End Source File
# Begin Source File

SOURCE=.\HViewQuality.mml
# End Source File
# Begin Source File

SOURCE=.\HViewRotating.mml
# End Source File
# Begin Source File

SOURCE=.\HViewScreen.mml
# End Source File
# Begin Source File

SOURCE=.\HViewSmall.mml
# End Source File
# Begin Source File

SOURCE=.\Light.mml
# End Source File
# Begin Source File

SOURCE=.\LocaleInfo.mml
# End Source File
# Begin Source File

SOURCE=.\LocaleLevels.mml
# End Source File
# Begin Source File

SOURCE=.\LocaleSet.mml
# End Source File
# Begin Source File

SOURCE=.\Macros.mml
# End Source File
# Begin Source File

SOURCE=.\MaterialCustom.mml
# End Source File
# Begin Source File

SOURCE=.\MoveSet.mml
# End Source File
# Begin Source File

SOURCE=.\NoodleCurve.mml
# End Source File
# Begin Source File

SOURCE=.\NoodleDialog.mml
# End Source File
# Begin Source File

SOURCE=.\NoodleDisplay.mml
# End Source File
# Begin Source File

SOURCE=.\ObjControlPointMove.mml
# End Source File
# Begin Source File

SOURCE=.\ObjectClassEdit.mml
# End Source File
# Begin Source File

SOURCE=.\ObjectClassNew.mml
# End Source File
# Begin Source File

SOURCE=.\ObjectLibrary.mml
# End Source File
# Begin Source File

SOURCE=.\ObjectNew.mml
# End Source File
# Begin Source File

SOURCE=.\ObjectRename.mml
# End Source File
# Begin Source File

SOURCE=.\ObjEditor.mml
# End Source File
# Begin Source File

SOURCE=.\ObjPaint.mml
# End Source File
# Begin Source File

SOURCE=.\ObjPaintAddScheme.mml
# End Source File
# Begin Source File

SOURCE=.\ObjPaintHelp.mml
# End Source File
# Begin Source File

SOURCE=.\PaintingDialog.mml
# End Source File
# Begin Source File

SOURCE=.\PathwayDialog.mml
# End Source File
# Begin Source File

SOURCE=.\PiersAppearance.mml
# End Source File
# Begin Source File

SOURCE=.\PiersCorners.mml
# End Source File
# Begin Source File

SOURCE=.\PiersCustom.mml
# End Source File
# Begin Source File

SOURCE=.\PiersDialog.mml
# End Source File
# Begin Source File

SOURCE=.\PiersDisplay.mml
# End Source File
# Begin Source File

SOURCE=.\PiersOpenings.mml
# End Source File
# Begin Source File

SOURCE=.\PolyhedronDisplay.mml
# End Source File
# Begin Source File

SOURCE=.\PolyMeshDialog.mml
# End Source File
# Begin Source File

SOURCE=.\PolyMeshDisplay.mml
# End Source File
# Begin Source File

SOURCE=.\PoolDialog.mml
# End Source File
# Begin Source File

SOURCE=.\Quality.mml
# End Source File
# Begin Source File

SOURCE=.\RevCurve.mml
# End Source File
# Begin Source File

SOURCE=.\RevDialog.mml
# End Source File
# Begin Source File

SOURCE=.\RevDisplay.mml
# End Source File
# Begin Source File

SOURCE=.\Select.mml
# End Source File
# Begin Source File

SOURCE=.\SelectColor.mml
# End Source File
# Begin Source File

SOURCE=.\SelectRegion.mml
# End Source File
# Begin Source File

SOURCE=.\SingleControlPoints.mml
# End Source File
# Begin Source File

SOURCE=.\SingleCurvature.mml
# End Source File
# Begin Source File

SOURCE=.\SingleDialog.mml
# End Source File
# Begin Source File

SOURCE=.\SingleOverMain.mml
# End Source File
# Begin Source File

SOURCE=.\Splash.mml
# End Source File
# Begin Source File

SOURCE=.\StairsBal.mml
# End Source File
# Begin Source File

SOURCE=.\StairsDialog.mml
# End Source File
# Begin Source File

SOURCE=.\StairsPath.mml
# End Source File
# Begin Source File

SOURCE=.\StairsRails.mml
# End Source File
# Begin Source File

SOURCE=.\StairsTreads.mml
# End Source File
# Begin Source File

SOURCE=.\StairsWalls.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceBevelling.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceControlPoints.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceCurvature.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceCutoutAddInter.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceCutoutMain.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceDetail.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceDialog.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceDisplay.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceEdge.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceEdgeInter.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceFloors.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceOverAddCustom.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceOverAddInter.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceOverEditCustom.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceOverEditEdge.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceOverEditInter.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceOverMain.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceShowClad.mml
# End Source File
# Begin Source File

SOURCE=.\SurfaceThickness.mml
# End Source File
# Begin Source File

SOURCE=.\TableClothDialog.mml
# End Source File
# Begin Source File

SOURCE=.\TemplateBack.mml
# End Source File
# Begin Source File

SOURCE=.\TemplateHelp.mml
# End Source File
# Begin Source File

SOURCE=.\TextureBMP.mml
# End Source File
# Begin Source File

SOURCE=.\TextureBoardBatten.mml
# End Source File
# Begin Source File

SOURCE=.\TextureChip.mml
# End Source File
# Begin Source File

SOURCE=.\TextureClapboards.mml
# End Source File
# Begin Source File

SOURCE=.\TextureCorr.mml
# End Source File
# Begin Source File

SOURCE=.\TextureCrNoise.mml
# End Source File
# Begin Source File

SOURCE=.\TextureDirtPaint.mml
# End Source File
# Begin Source File

SOURCE=.\TextureFabric.mml
# End Source File
# Begin Source File

SOURCE=.\TextureGeneratePlank.mml
# End Source File
# Begin Source File

SOURCE=.\TextureGenerateTree.mml
# End Source File
# Begin Source File

SOURCE=.\TextureGrout.mml
# End Source File
# Begin Source File

SOURCE=.\TextureImageFile.mml
# End Source File
# Begin Source File

SOURCE=.\TextureLattice.mml
# End Source File
# Begin Source File

SOURCE=.\TextureLibrary.mml
# End Source File
# Begin Source File

SOURCE=.\TextureMakeStone.mml
# End Source File
# Begin Source File

SOURCE=.\TextureMakeTile.mml
# End Source File
# Begin Source File

SOURCE=.\TextureMarble.mml
# End Source File
# Begin Source File

SOURCE=.\TextureMarbling.mml
# End Source File
# Begin Source File

SOURCE=.\TextureNew.mml
# End Source File
# Begin Source File

SOURCE=.\TextureNoise.mml
# End Source File
# Begin Source File

SOURCE=.\TextureParquet.mml
# End Source File
# Begin Source File

SOURCE=.\TexturePavers.mml
# End Source File
# Begin Source File

SOURCE=.\TextureRename.mml
# End Source File
# Begin Source File

SOURCE=.\TextureSel.mml
# End Source File
# Begin Source File

SOURCE=.\TextureShingles.mml
# End Source File
# Begin Source File

SOURCE=.\TextureStones.mml
# End Source File
# Begin Source File

SOURCE=.\TextureStonesRandom.mml
# End Source File
# Begin Source File

SOURCE=.\TextureStripes.mml
# End Source File
# Begin Source File

SOURCE=.\TextureThreads.mml
# End Source File
# Begin Source File

SOURCE=.\TextureTiles.mml
# End Source File
# Begin Source File

SOURCE=.\TextureWicker.mml
# End Source File
# Begin Source File

SOURCE=.\TextureWoodPlanks.mml
# End Source File
# Begin Source File

SOURCE=.\TreeDialog.mml
# End Source File
# Begin Source File

SOURCE=.\TrussDialog.mml
# End Source File
# Begin Source File

SOURCE=.\UniHole.mml
# End Source File
# Begin Source File

SOURCE=.\View3D.mml
# End Source File
# Begin Source File

SOURCE=.\ViewFavorite.mml
# End Source File
# Begin Source File

SOURCE=.\ViewFlat.mml
# End Source File
# Begin Source File

SOURCE=.\ViewSet.mml
# End Source File
# Begin Source File

SOURCE=.\ViewSetFog.mml
# End Source File
# Begin Source File

SOURCE=.\ViewSetGrid.mml
# End Source File
# Begin Source File

SOURCE=.\ViewSetLighting.mml
# End Source File
# Begin Source File

SOURCE=.\ViewSetMeasurement.mml
# End Source File
# Begin Source File

SOURCE=.\ViewSetOutlines.mml
# End Source File
# Begin Source File

SOURCE=.\ViewSetPerspective.mml
# End Source File
# Begin Source File

SOURCE=.\ViewSetRBGlasses.mml
# End Source File
# Begin Source File

SOURCE=.\ViewWalk.mml
# End Source File
# End Target
# End Project
