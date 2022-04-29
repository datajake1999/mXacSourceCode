# Microsoft Developer Studio Generated NMAKE File, Based on M3D.dsp
!IF "$(CFG)" == ""
CFG=M3D - Win32 Debug
!MESSAGE No configuration specified. Defaulting to M3D - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "M3D - Win32 Release" && "$(CFG)" != "M3D - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
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

!IF  "$(CFG)" == "M3D - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\M3D.exe"


CLEAN :
	-@erase "$(INTDIR)\CBalustrade.obj"
	-@erase "$(INTDIR)\CButtonBar.obj"
	-@erase "$(INTDIR)\CColumn.obj"
	-@erase "$(INTDIR)\CDoor.obj"
	-@erase "$(INTDIR)\CDoorFrame.obj"
	-@erase "$(INTDIR)\CDoorOpening.obj"
	-@erase "$(INTDIR)\CDoorSet.obj"
	-@erase "$(INTDIR)\CDoubleSurface.obj"
	-@erase "$(INTDIR)\CHouseView.obj"
	-@erase "$(INTDIR)\CIconButton.obj"
	-@erase "$(INTDIR)\CImage.obj"
	-@erase "$(INTDIR)\CImageShader.obj"
	-@erase "$(INTDIR)\CLibrary.obj"
	-@erase "$(INTDIR)\CLight.obj"
	-@erase "$(INTDIR)\CNoodle.obj"
	-@erase "$(INTDIR)\CObjectBalustrade.obj"
	-@erase "$(INTDIR)\CObjectBooks.obj"
	-@erase "$(INTDIR)\CObjectBuildBlock.obj"
	-@erase "$(INTDIR)\CObjectCabinet.obj"
	-@erase "$(INTDIR)\CObjectCamera.obj"
	-@erase "$(INTDIR)\CObjectCeilingFan.obj"
	-@erase "$(INTDIR)\CObjectColumn.obj"
	-@erase "$(INTDIR)\CObjectCurtain.obj"
	-@erase "$(INTDIR)\CObjectCushion.obj"
	-@erase "$(INTDIR)\CObjectDoor.obj"
	-@erase "$(INTDIR)\CObjectDrawer.obj"
	-@erase "$(INTDIR)\CObjectEditor.obj"
	-@erase "$(INTDIR)\CObjectFireplace.obj"
	-@erase "$(INTDIR)\CObjectGround.obj"
	-@erase "$(INTDIR)\CObjectLight.obj"
	-@erase "$(INTDIR)\CObjectNoodle.obj"
	-@erase "$(INTDIR)\CObjectPainting.obj"
	-@erase "$(INTDIR)\CObjectPathway.obj"
	-@erase "$(INTDIR)\CObjectPiers.obj"
	-@erase "$(INTDIR)\CObjectPolyhedron.obj"
	-@erase "$(INTDIR)\CObjectPolyMesh.obj"
	-@erase "$(INTDIR)\CObjectPool.obj"
	-@erase "$(INTDIR)\CObjectRevolution.obj"
	-@erase "$(INTDIR)\CObjectSingleSurface.obj"
	-@erase "$(INTDIR)\CObjectStairs.obj"
	-@erase "$(INTDIR)\CObjectStructSurface.obj"
	-@erase "$(INTDIR)\CObjectTableCloth.obj"
	-@erase "$(INTDIR)\CObjectTarp.obj"
	-@erase "$(INTDIR)\CObjectTeapot.obj"
	-@erase "$(INTDIR)\CObjectTemplate.obj"
	-@erase "$(INTDIR)\CObjectTestBox.obj"
	-@erase "$(INTDIR)\CObjectTree.obj"
	-@erase "$(INTDIR)\CObjectTruss.obj"
	-@erase "$(INTDIR)\CObjectUniHole.obj"
	-@erase "$(INTDIR)\CPiers.obj"
	-@erase "$(INTDIR)\CProgress.obj"
	-@erase "$(INTDIR)\CRenderClip.obj"
	-@erase "$(INTDIR)\CRenderMatrix.obj"
	-@erase "$(INTDIR)\CRenderSurface.obj"
	-@erase "$(INTDIR)\CRenderTraditional.obj"
	-@erase "$(INTDIR)\CRevolution.obj"
	-@erase "$(INTDIR)\CShadowBuf.obj"
	-@erase "$(INTDIR)\CSingleSurface.obj"
	-@erase "$(INTDIR)\CSpline.obj"
	-@erase "$(INTDIR)\CSplineSurface.obj"
	-@erase "$(INTDIR)\CThumbnail.obj"
	-@erase "$(INTDIR)\CToolTip.obj"
	-@erase "$(INTDIR)\CWorld.obj"
	-@erase "$(INTDIR)\Help.obj"
	-@erase "$(INTDIR)\Intersect.obj"
	-@erase "$(INTDIR)\M3D.res"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\Measure.obj"
	-@erase "$(INTDIR)\MML.obj"
	-@erase "$(INTDIR)\MMLCompress.obj"
	-@erase "$(INTDIR)\ObjectCF.obj"
	-@erase "$(INTDIR)\ResLeak.obj"
	-@erase "$(INTDIR)\Sun.obj"
	-@erase "$(INTDIR)\Textures.obj"
	-@erase "$(INTDIR)\Util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\VolumeIntersect.obj"
	-@erase "$(INTDIR)\VolumeSort.obj"
	-@erase "$(OUTDIR)\M3D.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /G6 /ML /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\M3D.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0xc09 /fo"$(INTDIR)\M3D.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\M3D.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\M3D.pdb" /machine:I386 /out:"$(OUTDIR)\M3D.exe" 
LINK32_OBJS= \
	"$(INTDIR)\CBalustrade.obj" \
	"$(INTDIR)\CButtonBar.obj" \
	"$(INTDIR)\CColumn.obj" \
	"$(INTDIR)\CDoor.obj" \
	"$(INTDIR)\CDoorFrame.obj" \
	"$(INTDIR)\CDoorOpening.obj" \
	"$(INTDIR)\CDoorSet.obj" \
	"$(INTDIR)\CDoubleSurface.obj" \
	"$(INTDIR)\CHouseView.obj" \
	"$(INTDIR)\CIconButton.obj" \
	"$(INTDIR)\CImage.obj" \
	"$(INTDIR)\CImageShader.obj" \
	"$(INTDIR)\CLibrary.obj" \
	"$(INTDIR)\CLight.obj" \
	"$(INTDIR)\CNoodle.obj" \
	"$(INTDIR)\CObjectBalustrade.obj" \
	"$(INTDIR)\CObjectBooks.obj" \
	"$(INTDIR)\CObjectBuildBlock.obj" \
	"$(INTDIR)\CObjectCabinet.obj" \
	"$(INTDIR)\CObjectCamera.obj" \
	"$(INTDIR)\CObjectCeilingFan.obj" \
	"$(INTDIR)\CObjectColumn.obj" \
	"$(INTDIR)\CObjectCurtain.obj" \
	"$(INTDIR)\CObjectCushion.obj" \
	"$(INTDIR)\CObjectDoor.obj" \
	"$(INTDIR)\CObjectDrawer.obj" \
	"$(INTDIR)\CObjectEditor.obj" \
	"$(INTDIR)\CObjectFireplace.obj" \
	"$(INTDIR)\CObjectGround.obj" \
	"$(INTDIR)\CObjectLight.obj" \
	"$(INTDIR)\CObjectNoodle.obj" \
	"$(INTDIR)\CObjectPainting.obj" \
	"$(INTDIR)\CObjectPathway.obj" \
	"$(INTDIR)\CObjectPiers.obj" \
	"$(INTDIR)\CObjectPolyhedron.obj" \
	"$(INTDIR)\CObjectPolyMesh.obj" \
	"$(INTDIR)\CObjectPool.obj" \
	"$(INTDIR)\CObjectRevolution.obj" \
	"$(INTDIR)\CObjectSingleSurface.obj" \
	"$(INTDIR)\CObjectStairs.obj" \
	"$(INTDIR)\CObjectStructSurface.obj" \
	"$(INTDIR)\CObjectTableCloth.obj" \
	"$(INTDIR)\CObjectTarp.obj" \
	"$(INTDIR)\CObjectTeapot.obj" \
	"$(INTDIR)\CObjectTemplate.obj" \
	"$(INTDIR)\CObjectTestBox.obj" \
	"$(INTDIR)\CObjectTree.obj" \
	"$(INTDIR)\CObjectTruss.obj" \
	"$(INTDIR)\CObjectUniHole.obj" \
	"$(INTDIR)\CPiers.obj" \
	"$(INTDIR)\CProgress.obj" \
	"$(INTDIR)\CRenderClip.obj" \
	"$(INTDIR)\CRenderMatrix.obj" \
	"$(INTDIR)\CRenderSurface.obj" \
	"$(INTDIR)\CRenderTraditional.obj" \
	"$(INTDIR)\CRevolution.obj" \
	"$(INTDIR)\CShadowBuf.obj" \
	"$(INTDIR)\CSingleSurface.obj" \
	"$(INTDIR)\CSpline.obj" \
	"$(INTDIR)\CSplineSurface.obj" \
	"$(INTDIR)\CThumbnail.obj" \
	"$(INTDIR)\CToolTip.obj" \
	"$(INTDIR)\CWorld.obj" \
	"$(INTDIR)\Help.obj" \
	"$(INTDIR)\Intersect.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\Measure.obj" \
	"$(INTDIR)\MML.obj" \
	"$(INTDIR)\MMLCompress.obj" \
	"$(INTDIR)\ObjectCF.obj" \
	"$(INTDIR)\ResLeak.obj" \
	"$(INTDIR)\Sun.obj" \
	"$(INTDIR)\Textures.obj" \
	"$(INTDIR)\Util.obj" \
	"$(INTDIR)\VolumeIntersect.obj" \
	"$(INTDIR)\VolumeSort.obj" \
	"$(INTDIR)\M3D.res"

"$(OUTDIR)\M3D.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "M3D - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\M3D.exe"


CLEAN :
	-@erase "$(INTDIR)\CBalustrade.obj"
	-@erase "$(INTDIR)\CButtonBar.obj"
	-@erase "$(INTDIR)\CColumn.obj"
	-@erase "$(INTDIR)\CDoor.obj"
	-@erase "$(INTDIR)\CDoorFrame.obj"
	-@erase "$(INTDIR)\CDoorOpening.obj"
	-@erase "$(INTDIR)\CDoorSet.obj"
	-@erase "$(INTDIR)\CDoubleSurface.obj"
	-@erase "$(INTDIR)\CHouseView.obj"
	-@erase "$(INTDIR)\CIconButton.obj"
	-@erase "$(INTDIR)\CImage.obj"
	-@erase "$(INTDIR)\CImageShader.obj"
	-@erase "$(INTDIR)\CLibrary.obj"
	-@erase "$(INTDIR)\CLight.obj"
	-@erase "$(INTDIR)\CNoodle.obj"
	-@erase "$(INTDIR)\CObjectBalustrade.obj"
	-@erase "$(INTDIR)\CObjectBooks.obj"
	-@erase "$(INTDIR)\CObjectBuildBlock.obj"
	-@erase "$(INTDIR)\CObjectCabinet.obj"
	-@erase "$(INTDIR)\CObjectCamera.obj"
	-@erase "$(INTDIR)\CObjectCeilingFan.obj"
	-@erase "$(INTDIR)\CObjectColumn.obj"
	-@erase "$(INTDIR)\CObjectCurtain.obj"
	-@erase "$(INTDIR)\CObjectCushion.obj"
	-@erase "$(INTDIR)\CObjectDoor.obj"
	-@erase "$(INTDIR)\CObjectDrawer.obj"
	-@erase "$(INTDIR)\CObjectEditor.obj"
	-@erase "$(INTDIR)\CObjectFireplace.obj"
	-@erase "$(INTDIR)\CObjectGround.obj"
	-@erase "$(INTDIR)\CObjectLight.obj"
	-@erase "$(INTDIR)\CObjectNoodle.obj"
	-@erase "$(INTDIR)\CObjectPainting.obj"
	-@erase "$(INTDIR)\CObjectPathway.obj"
	-@erase "$(INTDIR)\CObjectPiers.obj"
	-@erase "$(INTDIR)\CObjectPolyhedron.obj"
	-@erase "$(INTDIR)\CObjectPolyMesh.obj"
	-@erase "$(INTDIR)\CObjectPool.obj"
	-@erase "$(INTDIR)\CObjectRevolution.obj"
	-@erase "$(INTDIR)\CObjectSingleSurface.obj"
	-@erase "$(INTDIR)\CObjectStairs.obj"
	-@erase "$(INTDIR)\CObjectStructSurface.obj"
	-@erase "$(INTDIR)\CObjectTableCloth.obj"
	-@erase "$(INTDIR)\CObjectTarp.obj"
	-@erase "$(INTDIR)\CObjectTeapot.obj"
	-@erase "$(INTDIR)\CObjectTemplate.obj"
	-@erase "$(INTDIR)\CObjectTestBox.obj"
	-@erase "$(INTDIR)\CObjectTree.obj"
	-@erase "$(INTDIR)\CObjectTruss.obj"
	-@erase "$(INTDIR)\CObjectUniHole.obj"
	-@erase "$(INTDIR)\CPiers.obj"
	-@erase "$(INTDIR)\CProgress.obj"
	-@erase "$(INTDIR)\CRenderClip.obj"
	-@erase "$(INTDIR)\CRenderMatrix.obj"
	-@erase "$(INTDIR)\CRenderSurface.obj"
	-@erase "$(INTDIR)\CRenderTraditional.obj"
	-@erase "$(INTDIR)\CRevolution.obj"
	-@erase "$(INTDIR)\CShadowBuf.obj"
	-@erase "$(INTDIR)\CSingleSurface.obj"
	-@erase "$(INTDIR)\CSpline.obj"
	-@erase "$(INTDIR)\CSplineSurface.obj"
	-@erase "$(INTDIR)\CThumbnail.obj"
	-@erase "$(INTDIR)\CToolTip.obj"
	-@erase "$(INTDIR)\CWorld.obj"
	-@erase "$(INTDIR)\Help.obj"
	-@erase "$(INTDIR)\Intersect.obj"
	-@erase "$(INTDIR)\M3D.res"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\Measure.obj"
	-@erase "$(INTDIR)\MML.obj"
	-@erase "$(INTDIR)\MMLCompress.obj"
	-@erase "$(INTDIR)\ObjectCF.obj"
	-@erase "$(INTDIR)\ResLeak.obj"
	-@erase "$(INTDIR)\Sun.obj"
	-@erase "$(INTDIR)\Textures.obj"
	-@erase "$(INTDIR)\Util.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\VolumeIntersect.obj"
	-@erase "$(INTDIR)\VolumeSort.obj"
	-@erase "$(OUTDIR)\M3D.exe"
	-@erase "$(OUTDIR)\M3D.ilk"
	-@erase "$(OUTDIR)\M3D.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_TIMERS" /Fp"$(INTDIR)\M3D.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0xc09 /fo"$(INTDIR)\M3D.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\M3D.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib escarpment.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\M3D.pdb" /debug /machine:I386 /out:"$(OUTDIR)\M3D.exe" 
LINK32_OBJS= \
	"$(INTDIR)\CBalustrade.obj" \
	"$(INTDIR)\CButtonBar.obj" \
	"$(INTDIR)\CColumn.obj" \
	"$(INTDIR)\CDoor.obj" \
	"$(INTDIR)\CDoorFrame.obj" \
	"$(INTDIR)\CDoorOpening.obj" \
	"$(INTDIR)\CDoorSet.obj" \
	"$(INTDIR)\CDoubleSurface.obj" \
	"$(INTDIR)\CHouseView.obj" \
	"$(INTDIR)\CIconButton.obj" \
	"$(INTDIR)\CImage.obj" \
	"$(INTDIR)\CImageShader.obj" \
	"$(INTDIR)\CLibrary.obj" \
	"$(INTDIR)\CLight.obj" \
	"$(INTDIR)\CNoodle.obj" \
	"$(INTDIR)\CObjectBalustrade.obj" \
	"$(INTDIR)\CObjectBooks.obj" \
	"$(INTDIR)\CObjectBuildBlock.obj" \
	"$(INTDIR)\CObjectCabinet.obj" \
	"$(INTDIR)\CObjectCamera.obj" \
	"$(INTDIR)\CObjectCeilingFan.obj" \
	"$(INTDIR)\CObjectColumn.obj" \
	"$(INTDIR)\CObjectCurtain.obj" \
	"$(INTDIR)\CObjectCushion.obj" \
	"$(INTDIR)\CObjectDoor.obj" \
	"$(INTDIR)\CObjectDrawer.obj" \
	"$(INTDIR)\CObjectEditor.obj" \
	"$(INTDIR)\CObjectFireplace.obj" \
	"$(INTDIR)\CObjectGround.obj" \
	"$(INTDIR)\CObjectLight.obj" \
	"$(INTDIR)\CObjectNoodle.obj" \
	"$(INTDIR)\CObjectPainting.obj" \
	"$(INTDIR)\CObjectPathway.obj" \
	"$(INTDIR)\CObjectPiers.obj" \
	"$(INTDIR)\CObjectPolyhedron.obj" \
	"$(INTDIR)\CObjectPolyMesh.obj" \
	"$(INTDIR)\CObjectPool.obj" \
	"$(INTDIR)\CObjectRevolution.obj" \
	"$(INTDIR)\CObjectSingleSurface.obj" \
	"$(INTDIR)\CObjectStairs.obj" \
	"$(INTDIR)\CObjectStructSurface.obj" \
	"$(INTDIR)\CObjectTableCloth.obj" \
	"$(INTDIR)\CObjectTarp.obj" \
	"$(INTDIR)\CObjectTeapot.obj" \
	"$(INTDIR)\CObjectTemplate.obj" \
	"$(INTDIR)\CObjectTestBox.obj" \
	"$(INTDIR)\CObjectTree.obj" \
	"$(INTDIR)\CObjectTruss.obj" \
	"$(INTDIR)\CObjectUniHole.obj" \
	"$(INTDIR)\CPiers.obj" \
	"$(INTDIR)\CProgress.obj" \
	"$(INTDIR)\CRenderClip.obj" \
	"$(INTDIR)\CRenderMatrix.obj" \
	"$(INTDIR)\CRenderSurface.obj" \
	"$(INTDIR)\CRenderTraditional.obj" \
	"$(INTDIR)\CRevolution.obj" \
	"$(INTDIR)\CShadowBuf.obj" \
	"$(INTDIR)\CSingleSurface.obj" \
	"$(INTDIR)\CSpline.obj" \
	"$(INTDIR)\CSplineSurface.obj" \
	"$(INTDIR)\CThumbnail.obj" \
	"$(INTDIR)\CToolTip.obj" \
	"$(INTDIR)\CWorld.obj" \
	"$(INTDIR)\Help.obj" \
	"$(INTDIR)\Intersect.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\Measure.obj" \
	"$(INTDIR)\MML.obj" \
	"$(INTDIR)\MMLCompress.obj" \
	"$(INTDIR)\ObjectCF.obj" \
	"$(INTDIR)\ResLeak.obj" \
	"$(INTDIR)\Sun.obj" \
	"$(INTDIR)\Textures.obj" \
	"$(INTDIR)\Util.obj" \
	"$(INTDIR)\VolumeIntersect.obj" \
	"$(INTDIR)\VolumeSort.obj" \
	"$(INTDIR)\M3D.res"

"$(OUTDIR)\M3D.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("M3D.dep")
!INCLUDE "M3D.dep"
!ELSE 
!MESSAGE Warning: cannot find "M3D.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "M3D - Win32 Release" || "$(CFG)" == "M3D - Win32 Debug"
SOURCE=.\CBalustrade.cpp

"$(INTDIR)\CBalustrade.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CButtonBar.cpp

"$(INTDIR)\CButtonBar.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CColumn.cpp

"$(INTDIR)\CColumn.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CDoor.cpp

"$(INTDIR)\CDoor.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CDoorFrame.cpp

"$(INTDIR)\CDoorFrame.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CDoorOpening.cpp

"$(INTDIR)\CDoorOpening.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CDoorSet.cpp

"$(INTDIR)\CDoorSet.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CDoubleSurface.cpp

!IF  "$(CFG)" == "M3D - Win32 Release"

CPP_SWITCHES=/nologo /G6 /ML /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\M3D.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\CDoubleSurface.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "M3D - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_TIMERS" /Fp"$(INTDIR)\M3D.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\CDoubleSurface.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\CHouseView.cpp

"$(INTDIR)\CHouseView.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CIconButton.cpp

"$(INTDIR)\CIconButton.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CImage.cpp

"$(INTDIR)\CImage.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CImageShader.cpp

"$(INTDIR)\CImageShader.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CLibrary.cpp

"$(INTDIR)\CLibrary.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CLight.cpp

"$(INTDIR)\CLight.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CNoodle.cpp

"$(INTDIR)\CNoodle.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectBalustrade.cpp

"$(INTDIR)\CObjectBalustrade.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectBooks.cpp

"$(INTDIR)\CObjectBooks.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectBuildBlock.cpp

!IF  "$(CFG)" == "M3D - Win32 Release"

CPP_SWITCHES=/nologo /G6 /ML /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\M3D.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\CObjectBuildBlock.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "M3D - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_TIMERS" /Fp"$(INTDIR)\M3D.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\CObjectBuildBlock.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\CObjectCabinet.cpp

"$(INTDIR)\CObjectCabinet.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectCamera.cpp

"$(INTDIR)\CObjectCamera.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectCeilingFan.cpp

"$(INTDIR)\CObjectCeilingFan.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectColumn.cpp

"$(INTDIR)\CObjectColumn.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectCurtain.cpp

"$(INTDIR)\CObjectCurtain.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectCushion.cpp

"$(INTDIR)\CObjectCushion.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectDoor.cpp

"$(INTDIR)\CObjectDoor.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectDrawer.cpp

"$(INTDIR)\CObjectDrawer.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectEditor.cpp

"$(INTDIR)\CObjectEditor.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectFireplace.cpp

"$(INTDIR)\CObjectFireplace.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectGround.cpp

"$(INTDIR)\CObjectGround.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectLight.cpp

"$(INTDIR)\CObjectLight.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectNoodle.cpp

"$(INTDIR)\CObjectNoodle.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectPainting.cpp

"$(INTDIR)\CObjectPainting.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectPathway.cpp

"$(INTDIR)\CObjectPathway.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectPiers.cpp

"$(INTDIR)\CObjectPiers.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectPolyhedron.cpp

"$(INTDIR)\CObjectPolyhedron.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectPolyMesh.cpp

"$(INTDIR)\CObjectPolyMesh.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectPool.cpp

"$(INTDIR)\CObjectPool.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectRevolution.cpp

"$(INTDIR)\CObjectRevolution.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectSingleSurface.cpp

"$(INTDIR)\CObjectSingleSurface.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectStairs.cpp

"$(INTDIR)\CObjectStairs.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectStructSurface.cpp

"$(INTDIR)\CObjectStructSurface.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectTableCloth.cpp

"$(INTDIR)\CObjectTableCloth.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectTarp.cpp

"$(INTDIR)\CObjectTarp.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectTeapot.cpp

"$(INTDIR)\CObjectTeapot.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectTemplate.cpp

"$(INTDIR)\CObjectTemplate.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectTestBox.cpp

"$(INTDIR)\CObjectTestBox.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectTree.cpp

"$(INTDIR)\CObjectTree.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectTruss.cpp

"$(INTDIR)\CObjectTruss.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CObjectUniHole.cpp

"$(INTDIR)\CObjectUniHole.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CPiers.cpp

"$(INTDIR)\CPiers.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CProgress.cpp

"$(INTDIR)\CProgress.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CRenderClip.cpp

"$(INTDIR)\CRenderClip.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CRenderMatrix.cpp

"$(INTDIR)\CRenderMatrix.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CRenderSurface.cpp

"$(INTDIR)\CRenderSurface.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CRenderTraditional.cpp

"$(INTDIR)\CRenderTraditional.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CRevolution.cpp

"$(INTDIR)\CRevolution.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CShadowBuf.cpp

"$(INTDIR)\CShadowBuf.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CSingleSurface.cpp

"$(INTDIR)\CSingleSurface.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CSpline.cpp

"$(INTDIR)\CSpline.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CSplineSurface.cpp

!IF  "$(CFG)" == "M3D - Win32 Release"

CPP_SWITCHES=/nologo /G6 /ML /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\M3D.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\CSplineSurface.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "M3D - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_TIMERS" /Fp"$(INTDIR)\M3D.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\CSplineSurface.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\CThumbnail.cpp

"$(INTDIR)\CThumbnail.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CToolTip.cpp

"$(INTDIR)\CToolTip.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CWorld.cpp

"$(INTDIR)\CWorld.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Help.cpp

"$(INTDIR)\Help.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Intersect.cpp

"$(INTDIR)\Intersect.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\M3D.rc

"$(INTDIR)\M3D.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\main.cpp

"$(INTDIR)\main.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Measure.cpp

"$(INTDIR)\Measure.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\MML.cpp

"$(INTDIR)\MML.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\MMLCompress.cpp

"$(INTDIR)\MMLCompress.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ObjectCF.cpp

"$(INTDIR)\ObjectCF.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ResLeak.cpp

"$(INTDIR)\ResLeak.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Sun.cpp

"$(INTDIR)\Sun.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Textures.cpp

"$(INTDIR)\Textures.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Util.cpp

"$(INTDIR)\Util.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\VolumeIntersect.cpp

"$(INTDIR)\VolumeIntersect.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\VolumeSort.cpp

"$(INTDIR)\VolumeSort.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

