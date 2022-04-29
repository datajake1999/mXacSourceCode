/************************************************************************
CPolyMesh.cpp - Draws a PolyMesh.

begun 2/7/03 by Mike Rozak
Copyright 2003 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <float.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"




#define VMIRROROFFSET                0
#define VSIDEOFFSET                  (VMIRROROFFSET + m_wNumMirror * sizeof(PMMIRROR))
#define VEDGEOFFSET                  (VSIDEOFFSET + m_wNumSide * sizeof(DWORD))
#define VSIDETEXTOFFSET              (VEDGEOFFSET + m_wNumEdge * sizeof(DWORD))
#define VVERTDEFORMOFFSET            (VSIDETEXTOFFSET + m_wNumSideText * sizeof(SIDETEXT))
#define VBONEWEIGHTOFFSET            (VVERTDEFORMOFFSET + m_wNumVertDeform * sizeof(VERTDEFORM))
#define VMAXPMOFFSET                 (VBONEWEIGHTOFFSET + m_wNumBoneWeight * sizeof(BONEWEIGHT))

#define SMIRROROFFSET                0
#define SVERTOFFSET                  (SMIRROROFFSET + m_wNumMirror * sizeof(PMMIRROR))
#define SMAXPMOFFSET                 (SVERTOFFSET + m_wNumVert * sizeof(DWORD))

#define EMIRROROFFSET                0
#define EMAXPMOFFSET                 (EMIRROROFFSET + m_wNumMirror * sizeof(PMMIRROR))



/**********************************************************************************
OECALCompare - Used for sorting attributes in m_lPCOEAttrib
*/
static int _cdecl OECALCompare (const void *elem1, const void *elem2)
{
   PCOEAttrib *pdw1, *pdw2;
   pdw1 = (PCOEAttrib*) elem1;
   pdw2 = (PCOEAttrib*) elem2;

   return _wcsicmp(pdw1[0]->m_szName, pdw2[0]->m_szName);
}



/*************************************************************************************
CPMVert::CPMVert - Constructor and destructor*/
CPMVert::CPMVert (void)
{
   Clear();
}

/*************************************************************************************
CPMVert::Clear - Clers out the current point
*/
void CPMVert::Clear (void)
{
   m_pLoc.Zero();
   m_pLocSubdivide.Zero(); // BUGFIX
   m_tText.h = m_tText.v = 0;
   m_pNorm.Zero();

   m_wNumMirror = m_wNumSide = m_wNumEdge = 0;
   m_wNumSideText = m_wNumVertDeform = m_wNumBoneWeight = 0;
}


/*************************************************************************************
CPMVert::HasAMirror - Checks to see if the point has a mirror. SymmetryRecalc()
must have been called before for this to work

inputs
   DWORD       dwSymmetry - From m_dwSymmetry

*/
BOOL CPMVert::HasAMirror (DWORD dwSymmetry)
{
   if (m_wNumMirror)
      return TRUE;

   // see if it's ok because of symmetry
   DWORD i;
   for (i = 0; i < 3; i++)
      if ( (dwSymmetry & (1 << i)) && (fabs(m_pLoc.p[i]) < CLOSE))
         return TRUE;

   return FALSE;
}


/*************************************************************************************
CPMVert::XXXGet - Returns a pointer to the beginning of m_wNumXXX elems of the
type specified. While individual elements can be changed, to add/remove elements use
one of the XXXAdd or XXXRemove functions
*/
__inline PMMIRROR *CPMVert::MirrorGet (void)
{
   return (PMMIRROR*)((PBYTE) m_memMisc.p + VMIRROROFFSET);
}

__inline DWORD *CPMVert::SideGet (void)
{
   return (DWORD*)((PBYTE) m_memMisc.p + VSIDEOFFSET);
}

__inline DWORD *CPMVert::EdgeGet (void)
{
   return (DWORD*)((PBYTE) m_memMisc.p + VEDGEOFFSET);
}

__inline PSIDETEXT CPMVert::SideTextGet (void)
{
   return (PSIDETEXT)((PBYTE) m_memMisc.p + VSIDETEXTOFFSET);
}

__inline PVERTDEFORM CPMVert::VertDeformGet (void)
{
   return (PVERTDEFORM)((PBYTE) m_memMisc.p + VVERTDEFORMOFFSET);
}

__inline PBONEWEIGHT CPMVert::BoneWeightGet (void)
{
   return (PBONEWEIGHT)((PBYTE) m_memMisc.p + VBONEWEIGHTOFFSET);
}

__inline PVOID CPMVert::MaxPMGet (void)
{
   return (PVOID)((PBYTE) m_memMisc.p + VMAXPMOFFSET);
}

/*************************************************************************************
CPMVert::XXXAdd - Adds the given number of elements on to the list. It does NOT check
for duplicates. Returns TRUE if success, FALSE if fail.

NOTE: Calling this may invalidate previous pointers from XXXGet().
*/
BOOL CPMVert::MirrorAdd (PMMIRROR *pMirror, WORD wNum)
{
   DWORD dwHave, dwNeed, dwExtra;
   PBYTE pMove;
   dwExtra = wNum * sizeof(*pMirror);
   dwHave = VMAXPMOFFSET;
   dwNeed = dwHave + dwExtra;
   if (!m_memMisc.Required (dwNeed))
      return FALSE;
   pMove = (PBYTE)SideGet(); // changewhencopy

   DWORD dwCopy;
   dwCopy = (DWORD)(((PBYTE)m_memMisc.p + dwHave) - (PBYTE)pMove);
   memmove (pMove + dwExtra, pMove, dwCopy);
   memcpy (pMove, pMirror, dwExtra);
   m_wNumMirror += wNum; // changewhencopy
   return TRUE;
}

BOOL CPMVert::SideAdd (DWORD *padwSide, WORD wNum)
{
   DWORD dwHave, dwNeed, dwExtra;
   PBYTE pMove;
   dwExtra = wNum * sizeof(*padwSide);
   dwHave = VMAXPMOFFSET;
   dwNeed = dwHave + dwExtra;
   if (!m_memMisc.Required (dwNeed))
      return FALSE;
   pMove = (PBYTE)EdgeGet(); // changewhencopy

   DWORD dwCopy;
   dwCopy = (DWORD)(((PBYTE)m_memMisc.p + dwHave) - (PBYTE)pMove);
   memmove (pMove + dwExtra, pMove, dwCopy);
   memcpy (pMove, padwSide, dwExtra);
   m_wNumSide += wNum; // changewhencopy
   return TRUE;
}

BOOL CPMVert::EdgeAdd (DWORD *padwEdge, WORD wNum)
{
   DWORD dwHave, dwNeed, dwExtra;
   PBYTE pMove;
   dwExtra = wNum * sizeof(*padwEdge);
   dwHave = VMAXPMOFFSET;
   dwNeed = dwHave + dwExtra;
   if (!m_memMisc.Required (dwNeed))
      return FALSE;
   pMove = (PBYTE)SideTextGet(); // changewhencopy

   DWORD dwCopy;
   dwCopy = (DWORD)(((PBYTE)m_memMisc.p + dwHave) - (PBYTE)pMove);
   memmove (pMove + dwExtra, pMove, dwCopy);
   memcpy (pMove, padwEdge, dwExtra);
   m_wNumEdge += wNum; // changewhencopy
   return TRUE;
}

BOOL CPMVert::SideTextAdd (PSIDETEXT pSIDETEXT, WORD wNum)
{
   DWORD dwHave, dwNeed, dwExtra;
   PBYTE pMove;
   dwExtra = wNum * sizeof(*pSIDETEXT);
   dwHave = VMAXPMOFFSET;
   dwNeed = dwHave + dwExtra;
   if (!m_memMisc.Required (dwNeed))
      return FALSE;
   pMove = (PBYTE)VertDeformGet(); // changewhencopy

   DWORD dwCopy;
   dwCopy = (DWORD)(((PBYTE)m_memMisc.p + dwHave) - (PBYTE)pMove);
   memmove (pMove + dwExtra, pMove, dwCopy);
   memcpy (pMove, pSIDETEXT, dwExtra);
   m_wNumSideText += wNum; // changewhencopy
   return TRUE;
}

BOOL CPMVert::VertDeformAdd (PVERTDEFORM pVERTDEFORM, WORD wNum)
{
   // make sure no matches
   PVERTDEFORM pv = VertDeformGet();
   DWORD i;
   for (i = 0; i < m_wNumVertDeform; i++) if (pv[i].dwDeform == pVERTDEFORM->dwDeform) {
      pv[i].pDeform.Add (&pVERTDEFORM->pDeform);
      return TRUE;
   }

   DWORD dwHave, dwNeed, dwExtra;
   PBYTE pMove;
   dwExtra = wNum * sizeof(*pVERTDEFORM);
   dwHave = VMAXPMOFFSET;
   dwNeed = dwHave + dwExtra;
   if (!m_memMisc.Required (dwNeed))
      return FALSE;
   pMove = (PBYTE)BoneWeightGet(); // changewhencopy

   DWORD dwCopy;
   dwCopy = (DWORD)(((PBYTE)m_memMisc.p + dwHave) - (PBYTE)pMove);
   memmove (pMove + dwExtra, pMove, dwCopy);
   memcpy (pMove, pVERTDEFORM, dwExtra);
   m_wNumVertDeform += wNum; // changewhencopy
   return TRUE;
}

BOOL CPMVert::BoneWeightAdd (PBONEWEIGHT pBONEWEIGHT, WORD wNum)
{
   DWORD dwHave, dwNeed, dwExtra;
   PBYTE pMove;
   dwExtra = wNum * sizeof(*pBONEWEIGHT);
   dwHave = VMAXPMOFFSET;
   dwNeed = dwHave + dwExtra;
   if (!m_memMisc.Required (dwNeed))
      return FALSE;
   pMove = (PBYTE)MaxPMGet(); // changewhencopy

   DWORD dwCopy;
   dwCopy = (DWORD)(((PBYTE)m_memMisc.p + dwHave) - (PBYTE)pMove);
   memmove (pMove + dwExtra, pMove, dwCopy);
   memcpy (pMove, pBONEWEIGHT, dwExtra);
   m_wNumBoneWeight += wNum; // changewhencopy
   return TRUE;
}


/*************************************************************************************
CPMVert::XXXClear - Removes all the elements from the vertex
*/
void CPMVert::MirrorClear (void)
{
   if (m_wNumMirror) {
      PMMIRROR *pCur = MirrorGet();
      PBYTE pFrom = (PBYTE)(pCur + m_wNumMirror);
      memmove (pCur, pFrom, (PBYTE) (MaxPMGet()) - pFrom);
      m_wNumMirror = 0;
   }
}

void CPMVert::SideClear (void)
{
   if (m_wNumSide) {
      DWORD *pCur = SideGet();
      PBYTE pFrom = (PBYTE)(pCur + m_wNumSide);
      memmove (pCur, pFrom, (PBYTE) (MaxPMGet()) - pFrom);
      m_wNumSide = 0;
   }
}

void CPMVert::EdgeClear (void)
{
   if (m_wNumEdge) {
      DWORD *pCur = EdgeGet();
      PBYTE pFrom = (PBYTE)(pCur + m_wNumEdge);
      memmove (pCur, pFrom, (PBYTE) (MaxPMGet()) - pFrom);
      m_wNumEdge = 0;
   }
}

void CPMVert::SideTextClear (void)
{
   if (m_wNumSideText) {
      SIDETEXT *pCur = SideTextGet();
      PBYTE pFrom = (PBYTE)(pCur + m_wNumSideText);
      memmove (pCur, pFrom, (PBYTE) (MaxPMGet()) - pFrom);
      m_wNumSideText = 0;
   }
}

void CPMVert::VertDeformClear (void)
{
   if (m_wNumVertDeform) {
      VERTDEFORM *pCur = VertDeformGet();
      PBYTE pFrom = (PBYTE)(pCur + m_wNumVertDeform);
      memmove (pCur, pFrom, (PBYTE) (MaxPMGet()) - pFrom);
      m_wNumVertDeform = 0;
   }
}

void CPMVert::BoneWeightClear (void)
{
   if (m_wNumBoneWeight) {
      BONEWEIGHT *pCur = BoneWeightGet();
      PBYTE pFrom = (PBYTE)(pCur + m_wNumBoneWeight);
      memmove (pCur, pFrom, (PBYTE) (MaxPMGet()) - pFrom);
      m_wNumBoneWeight = 0;
   }
}


/*************************************************************************************
CPMVert::XXXRemove - Loops through all the elements for the type XXX. If it finds
any that match dwRemove they're deleted. If the value is greater than dwRemove the
value is decreased by 1.
*/
void CPMVert::MirrorRemove (DWORD dwRemove)
{
   WORD i;
   PMMIRROR *pCur = MirrorGet(); // changeline
   for (i = 0; i < m_wNumMirror; ) {   // changeline
      if (pCur[i].dwObject < dwRemove) {
         i++;
         continue;   // no change
      }
      if (pCur[i].dwObject > dwRemove) {
         pCur[i].dwObject--;
         i++;
         continue;
      }
      
      // else, match
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumMirror--;   // changeline
      // dont increase i
   }
}

void CPMVert::SideRemove (DWORD dwRemove)
{
   WORD i;
   DWORD *pCur = SideGet(); // changeline
   for (i = 0; i < m_wNumSide; ) {   // changeline
      if (pCur[i] < dwRemove) {
         i++;
         continue;   // no change
      }
      if (pCur[i] > dwRemove) {
         pCur[i]--;
         i++;
         continue;
      }
      
      // else, match
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumSide--;   // changeline
      // dont increase i
   }
}

void CPMVert::EdgeRemove (DWORD dwRemove)
{
   WORD i;
   DWORD *pCur = EdgeGet(); // changeline
   for (i = 0; i < m_wNumEdge; ) {   // changeline
      if (pCur[i] < dwRemove) {
         i++;
         continue;   // no change
      }
      if (pCur[i] > dwRemove) {
         pCur[i]--;
         i++;
         continue;
      }
      
      // else, match
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumEdge--;   // changeline
      // dont increase i
   }
}

void CPMVert::SideTextRemove (DWORD dwRemove)
{
   WORD i;
   SIDETEXT *pCur = SideTextGet(); // changeline
   for (i = 0; i < m_wNumSideText; ) {   // changeline
      if (pCur[i].dwSide < dwRemove) {
         i++;
         continue;   // no change
      }
      if (pCur[i].dwSide > dwRemove) {
         pCur[i].dwSide--;
         i++;
         continue;
      }
      
      // else, match
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumSideText--;   // changeline
      // dont increase i
   }
}

void CPMVert::VertDeformRemove (DWORD dwRemove)
{
   WORD i;
   VERTDEFORM *pCur = VertDeformGet(); // changeline
   for (i = 0; i < m_wNumVertDeform; ) {   // changeline
      if (pCur[i].dwDeform < dwRemove) {
         i++;
         continue;   // no change
      }
      if (pCur[i].dwDeform > dwRemove) {
         pCur[i].dwDeform--;
         i++;
         continue;
      }
      
      // else, match
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumVertDeform--;   // changeline
      // dont increase i
   }
}

void CPMVert::BoneWeightRemove (DWORD dwRemove)
{
   WORD i;
   BONEWEIGHT *pCur = BoneWeightGet(); // changeline
   for (i = 0; i < m_wNumBoneWeight; ) {   // changeline
      if (pCur[i].dwBone < dwRemove) {
         i++;
         continue;   // no change
      }
      if (pCur[i].dwBone > dwRemove) {
         pCur[i].dwBone--;
         i++;
         continue;
      }
      
      // else, match
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumBoneWeight--;   // changeline
      // dont increase i
   }
}

/*************************************************************************************
DWORDSearch - Given a pointer to a CListFixed containing DWORDs, sorted in
ascending order, this finds the index of the matching DWORD. Or, -1 if cant find

inputs
   DWORD       dwFind - Item to find
   DWORD       dwNum - Number of items in the list
   DWORD       *padwElem - Pointer to list of elements. MUST be sorted.
returns
   DWORD - Index of found, -1 if can't find
*/
static int _cdecl BDWORDCompare (const void *elem1, const void *elem2)
{
   DWORD *pdw1, *pdw2;
   pdw1 = (DWORD*) elem1;
   pdw2 = (DWORD*) elem2;

   if (*pdw1 > *pdw2)
      return 1;
   else if (*pdw1 < *pdw2)
      return -1;
   else
      return 0;
   // BUGFIX - Was return (int) (*pdw1) - (int)(*pdw2);
}

static int _cdecl B2DWORDCompare (const void *elem1, const void *elem2)
{
   DWORD *pdw1, *pdw2;
   pdw1 = (DWORD*) elem1;
   pdw2 = (DWORD*) elem2;

   if (pdw1[0] > pdw2[0])
      return 1;
   else if (pdw1[0] < pdw2[0])
      return -1;
   else if (pdw1[1] > pdw2[1])
      return 1;
   else if (pdw1[1] < pdw2[1])
      return -1;
   else
      return 0;
}

DWORD DWORDSearch (DWORD dwFind, DWORD dwNum, DWORD *padwElem)
{
   DWORD *pdw;
   pdw = (DWORD*) bsearch (&dwFind, padwElem, dwNum, sizeof(DWORD), BDWORDCompare);
   if (!pdw)
      return -1;
   return (DWORD) ((PBYTE) pdw - (PBYTE) padwElem) / sizeof(DWORD);
}


DWORD DWORD2Search (DWORD *pdwFind, DWORD dwNum, DWORD *padwElem)
{
   DWORD *pdw;
   pdw = (DWORD*) bsearch (pdwFind, padwElem, dwNum, 2*sizeof(DWORD), B2DWORDCompare);
   if (!pdw)
      return -1;
   return (DWORD) ((PBYTE) pdw - (PBYTE) padwElem) / (2*sizeof(DWORD));
}


/*************************************************************************************
CPMVert::XXXRename - Looks through all the elements. Anythign with an ID in the padwOrig
list (dwNum = number of elements) is renamed to padwNew's corresponding value.
If padwNew is -1 then the element is deleted.
*/
void CPMVert::MirrorRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   WORD i;
   PMMIRROR *pCur = MirrorGet(); // changeline
   for (i = 0; i < m_wNumMirror; ) {   // changeline
      DWORD dwVal = DWORDSearch (pCur[i].dwObject, dwNum, padwOrig);
      if (dwVal == -1) {
         // not found
         i++;
         continue;
      }

      // else if padwNew[dwVal] != -1 then just change
      if (padwNew[dwVal] != -1) {
         pCur[i].dwObject = padwNew[dwVal];
         i++;
         continue;
      }

      // else, delete
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumMirror--;   // changeline
      // dont increase i
   }
}


void CPMVert::SideRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   WORD i;
   DWORD *pCur = SideGet(); // changeline
   for (i = 0; i < m_wNumSide; ) {   // changeline
      DWORD dwVal = DWORDSearch (pCur[i], dwNum, padwOrig);
      if (dwVal == -1) {
         // not found
         i++;
         continue;
      }

      // else if padwNew[dwVal] != -1 then just change
      if (padwNew[dwVal] != -1) {
         pCur[i] = padwNew[dwVal];
         i++;
         continue;
      }

      // else, delete
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumSide--;   // changeline
      // dont increase i
   }
}

void CPMVert::EdgeRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   WORD i;
   DWORD *pCur = EdgeGet(); // changeline
   for (i = 0; i < m_wNumEdge; ) {   // changeline
      DWORD dwVal = DWORDSearch (pCur[i], dwNum, padwOrig);
      if (dwVal == -1) {
         // not found
         i++;
         continue;
      }

      // else if padwNew[dwVal] != -1 then just change
      if (padwNew[dwVal] != -1) {
         pCur[i] = padwNew[dwVal];
         i++;
         continue;
      }

      // else, delete
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumEdge--;   // changeline
      // dont increase i
   }
}

void CPMVert::SideTextRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew, BOOL fCanCreateDup)
{
   WORD i, j;
   SIDETEXT *pCur = SideTextGet(); // changeline
   for (i = 0; i < m_wNumSideText; ) {   // changeline
      DWORD dwVal = DWORDSearch (pCur[i].dwSide, dwNum, padwOrig);
      if (dwVal == -1) {
         // not found
         i++;
         continue;
      }

      // else if padwNew[dwVal] != -1 then just change
      if (padwNew[dwVal] != -1) {
         pCur[i].dwSide = padwNew[dwVal];
         i++;
         continue;
      }

      // else, delete
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumSideText--;   // changeline
      // dont increase i
   }

   // BUGFIX - When do a rename might end up renaming two differnt sides to
   // the same side number... This could cause problems because then have
   // two entries for the same side. To take care of this, go through and look
   // for duplicates
   DWORD dwVal;
   if (fCanCreateDup) for (i = 0; i < m_wNumSideText; i++) {
      dwVal = pCur[i].dwSide;

      for (j = m_wNumSideText-1; j > i; j--) {
         if (dwVal == pCur[j].dwSide) {
            // delete
            memmove (pCur + j, pCur + (j+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (j+1)));
            m_wNumSideText--;   // changeline
         }
      } // j
   } // i

}

void CPMVert::VertDeformRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   WORD i;
   VERTDEFORM *pCur = VertDeformGet(); // changeline
   for (i = 0; i < m_wNumVertDeform; ) {   // changeline
      DWORD dwVal = DWORDSearch (pCur[i].dwDeform, dwNum, padwOrig);
      if (dwVal == -1) {
         // not found
         i++;
         continue;
      }

      // else if padwNew[dwVal] != -1 then just change
      if (padwNew[dwVal] != -1) {
         pCur[i].dwDeform = padwNew[dwVal];
         i++;
         continue;
      }

      // else, delete
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumVertDeform--;   // changeline
      // dont increase i
   }
}

void CPMVert::BoneWeightRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   WORD i;
   BONEWEIGHT *pCur = BoneWeightGet(); // changeline
   for (i = 0; i < m_wNumBoneWeight; ) {   // changeline
      DWORD dwVal = DWORDSearch (pCur[i].dwBone, dwNum, padwOrig);
      if (dwVal == -1) {
         // not found
         i++;
         continue;
      }

      // else if padwNew[dwVal] != -1 then just change
      if (padwNew[dwVal] != -1) {
         pCur[i].dwBone = padwNew[dwVal];
         i++;
         continue;
      }

      // else, delete
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumBoneWeight--;   // changeline
      // dont increase i
   }
}


/**********************************************************************************
CPMVert::SideTextClean - Loops through the side text and looks for refernces to
sides to which the vertex is no longer connected. If it finds any then they are
removed. NOTE: This requires that the list of sides in the vertex is valid.
*/
void CPMVert::SideTextClean (void)
{
   WORD i, j;
   SIDETEXT *pCur = SideTextGet(); // changeline
   DWORD *padwSide = SideGet();
   for (i = 0; i < m_wNumSideText; ) {   // changeline
      // see if this matches a side
      for (j = 0; j < m_wNumSide; j++)
         if (padwSide[j] == pCur[i].dwSide)
            break;
      if (j < m_wNumSide) {
         i++;
         continue;   // still valid
      }


      // else, delete
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumSideText--;   // changeline
      // dont increase i
   }

}

/**********************************************************************************
CPMVert::TextureGet - Given a side (or -1 if side unknown), this returns a pointer
to the texture point. This pointer is only valid until textureset is called.

inputs
   DWORD       dwSide - Which side. If the side can't be found in the remap list returns the default text
returns
   PTEXTUREPOINT - Pointer to the texture
*/
PTEXTUREPOINT CPMVert::TextureGet (DWORD dwSide)
{
   if (!m_wNumSideText || (dwSide == -1))
      return &m_tText;  // only one setting so use

   WORD i;
   PSIDETEXT ps = SideTextGet ();
   for (i = 0; i < m_wNumSideText; i++) {
      if (dwSide == ps[i].dwSide)
         return &ps[i].tp;
   }

   // else not found
   return &m_tText;
}

/**********************************************************************************
CPMVert:TextureSet - Sets the texture for the specific side. NOTE: If the side is
new to the vertex it normally adds an entry. However, if the texture point is the
same as what exists nothing is added.

inputs
   DWORD          dwSide - Side to set it for. If the side is not already in the remap
                  list this will add it. If this is -1 then the default side will be set.
   PTEXTUREPOINT  pt - Set to
   BOOL           fAlwaysAdd - If TRUE then when find a dwSide that's not on the list
                  then always add an entry for it, even if it's close to the default setting
returns
   none
*/
BOOL CPMVert::TextureSet (DWORD dwSide, PTEXTUREPOINT pt, BOOL fAlwaysAdd)
{
   if (dwSide == -1) {
      m_tText = *pt;
      return TRUE;
   }

   // see if already in list
   if (m_wNumSideText) {
      WORD i;
      PSIDETEXT ps = SideTextGet();
      for (i = 0; i < m_wNumSideText; i++)
         if (dwSide == ps[i].dwSide) {
            ps[i].tp = *pt;
            return TRUE;
         }
   }

   // if it's the same then add nothing
   if (!fAlwaysAdd && AreClose (pt, &m_tText))
      return TRUE;  // no change

   // else, add
   SIDETEXT st;
   st.dwSide = dwSide;
   st.tp = *pt;
   return SideTextAdd (&st);
}


/**********************************************************************************
CPMVert::DeformChanged - Incremennts the vertex's location by pDelta. If dwDeform
==-1 then m_pLoc is changed. Else, the given deformation is changed. Automatically
adds a deformation entry if none exists, or removes it if it's set to 0.

inputs
   DWORD       dwDeform - Deformation number, or -1 if m_pLoc
   CPoint      pDelta - Amount to change by.
returns
   none
*/
BOOL CPMVert::DeformChanged (DWORD dwDeform, PCPoint pDelta)
{
   if (dwDeform == -1) {
      m_pLoc.Add (pDelta);
      return TRUE;
   }

   CPoint pZero;
   pZero.Zero();
   if (pDelta->AreClose (&pZero))
      return TRUE; // no change

   // see if can find it
   if (m_wNumVertDeform) {
      WORD i;
      PVERTDEFORM pv = VertDeformGet();
      for (i = 0; i < m_wNumVertDeform; i++, pv++) {  // BUGGIC - Wasnt increasing PV
         if (pv->dwDeform != dwDeform)
            continue;   // no match

         // match
         pv->pDeform.Add (pDelta);
         if (pv->pDeform.AreClose (&pZero))  // if ends up being 0 then just get rid of
            VertDeformRemove (dwDeform);
         return TRUE;
      } // i
   } // if verdeform

   // else, cant find, so add
   VERTDEFORM vd;
   vd.dwDeform = dwDeform;
   vd.pDeform.Copy (pDelta);
   return VertDeformAdd (&vd);
}



/**********************************************************************************
CPMVertOld::DeformCalc - Filled in the pLoc based on the current deformation.

inputs
   DWORD       dwNum - Number of active deformations
   DWORD       *padwDeform - Pointer to an array of deformations. This MUST be sorted in ascending order
   fp          *pafDeform - Pointer to an array of deformation amounts (from 0 to 1)
   PCPoint     pLoc - Filled in with new location
returns
   none
*/
void CPMVert::DeformCalc (DWORD dwNum, DWORD *padwDeform, fp *pafDeform, PCPoint pLoc)
{
   // start out with current location
   pLoc->Copy (&m_pLoc);

   // go throgh all the pnt deformations
   WORD i;
   DWORD dwFind;
   CPoint p;
   PVERTDEFORM pv = VertDeformGet();
   for (i = 0; i < m_wNumVertDeform; i++) {
      dwFind = DWORDSearch (pv[i].dwDeform, dwNum, padwDeform);
      if (dwFind == -1)
         continue;   // assume 0

      // else value
      p.Copy (&pv[i].pDeform);
      p.Scale (pafDeform[dwFind]);
      pLoc->Add (&p);
   }

}



/**********************************************************************************
CPMVert::Scale - Scales the vertex, including all normals.

inputs
   PCMatrix       pScale - Does scaling on the points
   PCMatrix       pScaleInvTrans - Inverse and thentransposed version of pScale. Used on vectors.
*/
void CPMVert::Scale (PCMatrix pScale, PCMatrix pScaleInvTrans)
{
   m_pLoc.p[3] = 1;
   m_pLoc.MultiplyLeft (pScale);

   // NOTE: Not scaling the normal
   //m_pNorm.p[3] = 1;
   //m_pNorm.MultiplyLeft (pScaleInvTrans);

   // transform all the deformations
   if (m_wNumVertDeform) {
      WORD i;
      PVERTDEFORM pv = VertDeformGet();
      for (i = 0; i < m_wNumVertDeform; i++) {
         pv[i].pDeform.p[3] =1;
         pv[i].pDeform.MultiplyLeft (pScaleInvTrans);
      }
   }

}


/**********************************************************************************
CPMVert::Clone -Standard clone
*/
CPMVert *CPMVert::Clone (BOOL fKeepDeform)
{
   PCPMVert pNew = new CPMVert;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew, fKeepDeform)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}


/**********************************************************************************
CPMVert::CloneTo - Copies all the data from the current vertex to the new one.
It ASSUMES the new one has NO memory allocated, since everything is copied over.

inputs
   PCPMVertOld    *pTo - Copies the info to
   BOOL        fCloneDeform - If TRUE (defualt) then clone the deformation info also.
               If false, only clone the texture info.
*/
BOOL CPMVert::CloneTo (CPMVert *pTo, BOOL fKeepDeform)
{
   // copy over points
   pTo->m_pLoc.Copy (&m_pLoc);
   pTo->m_tText = m_tText;
   pTo->m_pNorm.Copy (&m_pNorm);
   pTo->m_pLocSubdivide.Copy (&m_pLocSubdivide);   // BUGFIX - Didnt have before
   pTo->m_pNormSubdivide.Copy(&m_pNormSubdivide);

   // transfer other stuff over
   DWORD dwNeed = fKeepDeform ? (VMAXPMOFFSET) :
      (sizeof(SIDETEXT)*m_wNumSideText + sizeof(DWORD)*m_wNumSide);
   if (!pTo->m_memMisc.Required (dwNeed))
      return FALSE;
   pTo->m_wNumSide = m_wNumSide;
   pTo->m_wNumSideText = m_wNumSideText;
   if (fKeepDeform) {
      pTo->m_wNumEdge = m_wNumEdge;
      pTo->m_wNumMirror = m_wNumMirror;
      pTo->m_wNumVertDeform = m_wNumVertDeform;
      pTo->m_wNumBoneWeight = m_wNumBoneWeight;

      memcpy (pTo->m_memMisc.p, m_memMisc.p, dwNeed);
   }
   else {
      // BUGFIX - Do like this so faster to clone for deformations
      pTo->m_wNumEdge = pTo->m_wNumMirror = pTo->m_wNumVertDeform = pTo->m_wNumBoneWeight = 0;

      memcpy (pTo->SideGet(), SideGet(), sizeof(DWORD)*m_wNumSide);
      memcpy (pTo->SideTextGet(), SideTextGet(), sizeof(SIDETEXT)*m_wNumSideText);
      // NOTE: This discards the bone weight too, which may not be desired
   }


   // clear deformations?
   // not needed because of stuff from above
   //if (!fKeepDeform && m_wNumVertDeform) {
   //   pTo->VertDeformClear();
   //}

   return TRUE;
}



/**********************************************************************************
CPMVert::CalcNorm - Calculates and fills in the normal for the point. Note, the
polygon mesh's sides must have had CalcNorm() called first.
*/
void CPMVert::CalcNorm (PCPolyMesh pMesh)
{
   m_pNorm.Zero();

   // get normals from all the sides
   WORD i;
   DWORD *padwSide = SideGet ();
   for (i = 0; i < m_wNumSide; i++) {
      PCPMSide ps = pMesh->SideGet (padwSide[i]);
      m_pNorm.Add (&ps->m_pNorm);
   } // i

   // renormalize
   if (m_wNumSide > 1)
      m_pNorm.Normalize();

}


/**********************************************************************************
CPMVert::Average - Averages two or more points together and fills in this point.

NOTE:
   The mirror list is not filled in becuase mirrors cannot be determined at this time.
   Same with the edge list.

inputs
   PCPMVert       *papVert - Pointer to an array of PCPMVert that are to be averaged together
   fp             *pafWeight - Weights, summing up to 1.0, for each of the vertices.
                     If NULL then weights are all 1.0 / dwNum
   DWORD          dwNum - Number of elements in papVert and pafWeight
   DWORD          dwSide - Side to use when determining the texture, or -1 for all included sides
   BOOL           fKeepDeform - If TRUE keep the deformation information when interpolate.
*/
BOOL CPMVert::Average (CPMVert **papVert, fp *pafWeight, DWORD dwNum, DWORD dwSide, BOOL fKeepDeform)
{
   // wipe out what currently have
   Clear();
   fp fWeight;
   fWeight = 1.0 / (fp)dwNum;

   // do location
   DWORD i;
   WORD j;
   CPoint pTemp;
   for (i = 0; i < dwNum; i++) {
      pTemp.Copy (&papVert[i]->m_pLoc);
      pTemp.Scale (pafWeight ? pafWeight[i] : fWeight);
      m_pLoc.Add (&pTemp);

      // also do subdivide, since may be used for volumetric textures or
      // or displaying bone/morph coloration
      pTemp.Copy (&papVert[i]->m_pLocSubdivide);
      pTemp.Scale (pafWeight ? pafWeight[i] : fWeight);
      m_pLocSubdivide.Add (&pTemp);
   }

   // do the texture interpolation, if side != -1 then get textures from each one
   // and add in, else, need to be more complex
   if (dwSide != -1) {
      PTEXTUREPOINT ptp;
      for (i = 0; i < dwNum; i++) {
         ptp = papVert[i]->TextureGet (dwSide);
         m_tText.h += ptp->h * (pafWeight ? pafWeight[i] : fWeight);
         m_tText.v += ptp->v * (pafWeight ? pafWeight[i] : fWeight);
      }
   }
   else {   // side==-1
      // go through all the other points and figure out what sides affect their textures
      PSIDETEXT pt;
      TEXTUREPOINT tp;
      memset (&tp, 0, sizeof(tp));
      for (i = 0; i < dwNum; i++) {
         pt = papVert[i]->SideTextGet();
         for (j = 0; j < papVert[i]->m_wNumSideText; j++)
            TextureSet (pt[j].dwSide, &tp, TRUE);   // so have entries for all textures
      } // i

      // now go through all texture indecies and average
      PTEXTUREPOINT ptp;
      DWORD dwSide;
      pt = SideTextGet();
      for (j = 0; j <= m_wNumSideText; j++) {
         if (j < m_wNumSideText) {
            ptp = &pt[j].tp;
            dwSide = pt[j].dwSide;
         }
         else {
            ptp = &m_tText;
            dwSide = -1;
         }
         ptp->h = ptp->v = 0;

         // loop and average texture values
         fp fCount;
         fCount = 0;
         for (i = 0; i < dwNum; i++) {
            PTEXTUREPOINT ptp2;
            ptp2 = papVert[i]->TextureGet (dwSide);

            // BUGFIX - If textureget of a valid side is returning the default
            // texture then ignore
            if ((dwSide != -1) && (papVert[i]->TextureGet(-1) == ptp2))
               continue;

            fCount += (pafWeight ? pafWeight[i] : fWeight);
            ptp->h += ptp2->h * (pafWeight ? pafWeight[i] : fWeight);
            ptp->v += ptp2->v * (pafWeight ? pafWeight[i] : fWeight);
         }
         if (fCount) {
            ptp->h /= fCount;
            ptp->v /= fCount;
         }

      } // j
   } // if include all sides

   // if want to interpolate deformations then calculate these
   if (fKeepDeform) {
      // loop through all the point deformations and average these
      PVERTDEFORM pvdp1, pvdp2;
      WORD k;
      pvdp2 = VertDeformGet();
      for (i = 0; i < dwNum; i++) {
         pvdp1 = papVert[i]->VertDeformGet();
         for (j = 0; j < papVert[i]->m_wNumVertDeform; j++) {
            for (k = 0; k < m_wNumVertDeform; k++)
               if (pvdp1[j].dwDeform == pvdp2[k].dwDeform)
                  break;
            if (k < m_wNumVertDeform) {
               // already on list, so incorporate
               pTemp.Copy (&pvdp1[j].pDeform);
               pTemp.Scale (pafWeight ? pafWeight[i] : fWeight);
               pvdp2[k].pDeform.Add (&pTemp);
            }
            else {
               // not on list, so add new entry
               VERTDEFORM vd;
               vd.dwDeform = pvdp1[j].dwDeform;
               vd.pDeform.Copy (&pvdp1[j].pDeform);
               vd.pDeform.Scale (pafWeight ? pafWeight[i] : fWeight);
               VertDeformAdd (&vd);

               // reget the value
               pvdp2 = VertDeformGet();
            }
         } // j
      } // i


      // loop through all the bone weights and average these
      PBONEWEIGHT pvbw1, pvbw2;
      pvbw2 = BoneWeightGet();
      for (i = 0; i < dwNum; i++) {
         pvbw1 = papVert[i]->BoneWeightGet();
         for (j = 0; j < papVert[i]->m_wNumBoneWeight; j++) {
            for (k = 0; k < m_wNumBoneWeight; k++)
               if (pvbw1[j].dwBone == pvbw2[k].dwBone)
                  break;
            if (k < m_wNumBoneWeight) {
               // already on list, so incorporate
               pvbw2[k].fWeight += (pafWeight ? pafWeight[i] : fWeight) * pvbw1[j].fWeight;
            }
            else {
               // not on list, so add new entry
               BONEWEIGHT vd;
               vd.dwBone = pvbw1[j].dwBone;
               vd.fWeight = pvbw1[j].fWeight * (pafWeight ? pafWeight[i] : fWeight);
               BoneWeightAdd (&vd);

               // reget the value
               pvbw2 = BoneWeightGet();
            }
         } // j
      } // i
   } // if fKeepDeform

   return TRUE;
}



static PWSTR gpszVert = L"Vert";
static PWSTR gpszLoc = L"Loc";
static PWSTR gpszText = L"Text";
static PWSTR gpszMirror = L"Mirror";
static PWSTR gpszObject = L"Object";
static PWSTR gpszType = L"Type";
static PWSTR gpszSideText = L"SideText";
static PWSTR gpszVertDeform = L"VertDeform";
static PWSTR gpszBoneWeight = L"VertBoneWeight";
static PWSTR gpszDeform = L"Deform";



/**********************************************************************************
CPMVert::MMLToBinaryCalcMinMax - Used to calculate the minimum and maximum values
that are used.

inputs
   PCPoint     pLocMax - Filled with the maximum location. Will already contain valid values.
   PCPoint     pLocMin - Filled with the minimum location. Will already contain valid values.
   PTEXTUREPOINT ptTextMax - Filled with the maximum texture. Will already contain valid values.
   PTEXTUREPOINT ptTextMin - Filled with the minimum texture. Will already contain valid values.
returns
   none
*/
void CPMVert::MMLToBinaryCalcMinMax (PCPoint pLocMax, PCPoint pLocMin, PTEXTUREPOINT ptTextMax,
                                     PTEXTUREPOINT ptTextMin)
{
   pLocMax->Max (&m_pLoc);
   pLocMin->Min (&m_pLoc);

   ptTextMax->h = max(ptTextMax->h, m_tText.h);
   ptTextMax->v = max(ptTextMax->v, m_tText.v);
   ptTextMin->h = min(ptTextMin->h, m_tText.h);
   ptTextMin->v = min(ptTextMin->v, m_tText.v);

   // side text info
   PSIDETEXT pSideText;
   pSideText = SideTextGet();
   DWORD i;
   for (i = 0; i < m_wNumSideText; i++, pSideText++) {
      ptTextMax->h = max(ptTextMax->h, pSideText->tp.h);
      ptTextMax->v = max(ptTextMax->v, pSideText->tp.v);
      ptTextMin->h = min(ptTextMin->h, pSideText->tp.h);
      ptTextMin->v = min(ptTextMin->v, pSideText->tp.v);
   }

   // deformation around point
   PVERTDEFORM pVertDeform;
   pVertDeform = VertDeformGet();
   for (i = 0; i < m_wNumVertDeform; i++, pVertDeform++) {
      pLocMax->Max (&pVertDeform->pDeform);
      pLocMin->Min (&pVertDeform->pDeform);
   }

}


// VERTBIN - basic vertex binary structure
typedef struct {
   WORD           wSize;         // size of the entire vertex information, in bytes
   WORD           wSymmetry;     // indicates which sign bits are flipped. 0x01 for x, 0x02 for y, 0x04 for z
   short          aiLoc[3];      // location, -32767 to 32767. Takes into account symmetry settings
   short          aiText[2];     // texture, -326767 to 32767
   WORD           wSides;        // number of sides
   WORD           wDeform;       // number of deformations
   WORD           wWeight;       // number of bone weights
   // followed by an array of VERTBINSIDE
   // followed by an array of VERTBINDEFORM
   // followed by an array of VERTBINWEIGHT
} VERTBIN, *PVERTBIN;

// VERTBINSIDE - For storing sides
typedef struct {
   WORD           wNum;          // side number
   short          aiText[2];     // texture, -32767 to 32767
} VERTBINSIDE, *PVERTBINSIDE;

// VERTBINDEFORM - For storing deformations
typedef struct {
   WORD           wNum;          // deformation number
   short          aiDeform[3];   // deformation, -32676 to 32767. Takes into account symmetry settings
} VERTBINDEFORM, *PVERTBINDEFORM;

// VERTBINWEIGHT - For storing weights
typedef struct {
   WORD           wNum;          // bone number
   short          iWeight;       // 0 for 0.0, 0x100 for 1.0
} VERTBINWEIGHT, *PVERTBINWEIGHT;

/**********************************************************************************
CPMVert::MMLToBinary - Saves the information as binary.

inputs
   PCMem       pMem - Memory to save to. New memory is appeneded to m_dwCurPosn
   PCPoint     pScale - Scaling factor
   PTEXTUREPOINT ptScale - Scaling factor for textures
   fp          fRound - How much to add for rounding off
   DWORD       dwSymmetry - Bit fields. 0x01 if symmetrical around X, 0x02 around Y, 0x04 around Z
returns
   BOOL - TRUE if success
*/
BOOL CPMVert::MMLToBinary (PCMem pMem, PCPoint pScale, PTEXTUREPOINT ptScale, fp fRound, DWORD dwSymmetry)
{
   // how much size do we need in total
   DWORD dwNeed = sizeof(VERTBIN) +
      sizeof(VERTBINSIDE) * (DWORD)m_wNumSideText +
      sizeof(VERTBINDEFORM) * (DWORD)m_wNumVertDeform +
      sizeof(VERTBINWEIGHT) * (DWORD)m_wNumBoneWeight;
   if (!pMem->Required (pMem->m_dwCurPosn + dwNeed))
      return FALSE;
   if (dwNeed >= 0x10000)
      return FALSE;  // shouldnt happen, but just in case

   PVERTBIN pvb = (PVERTBIN) ((PBYTE)pMem->p + pMem->m_dwCurPosn);
   pvb->wSize = (WORD) dwNeed;
   pvb->wSides = m_wNumSideText;
   pvb->wDeform = m_wNumVertDeform;
   pvb->wWeight = m_wNumBoneWeight;

   // fill in symmetry
   DWORD i, j;
   pvb->wSymmetry = 0;
   for (j = 0; j < 3; j++)
      if ((dwSymmetry & (1<<j)) && (m_pLoc.p[j] < 0))
         pvb->wSymmetry |= (WORD)(1 << j);

   fp f;
   short s;
   for (j = 0; j < 3; j++) {
      // convert units
      f = m_pLoc.p[j] * pScale->p[j] + fRound;

      // BUGFIX - make sure don't excede values, since sometimes getting wrap around
      f = max(f, -32767);
      f = min(f, 32767);

      s = (short) floor(f);
      if (pvb->wSymmetry & (WORD)(1 << j))
         s *= -1;

      // weite
      pvb->aiLoc[j] = s;
   }
   for (j = 0; j < 2; j++) {
      // convert units
      f = (j ? m_tText.v * ptScale->v : m_tText.h * ptScale->h) + fRound;
      s = (short) floor(f);

      // write
      pvb->aiText[j] = s;
   } // j


   // sides
   PVERTBINSIDE pSide = (PVERTBINSIDE) (pvb + 1);
   PSIDETEXT pSideText;
   pSideText = SideTextGet();
   for (i = 0; i < m_wNumSideText; i++, pSideText++, pSide++) {
      pSide->wNum = (WORD)pSideText->dwSide;

      for (j = 0; j < 2; j++) {
         // convert units
         f = (j ? pSideText->tp.v * ptScale->v : pSideText->tp.h * ptScale->h) + fRound;
         s = (short) floor(f);

         // write
         pSide->aiText[j] = s;
      } // j
   }

   // deformations
   PVERTBINDEFORM pDeform = (PVERTBINDEFORM) pSide;
   PVERTDEFORM pVertDeform;
   pVertDeform = VertDeformGet();
   for (i = 0; i < m_wNumVertDeform; i++, pVertDeform++, pDeform++) {
      pDeform->wNum = (WORD)pVertDeform->dwDeform;

      for (j = 0; j < 3; j++) {
         // convert units
         f = pVertDeform->pDeform.p[j] * pScale->p[j] + fRound;
         s = (short) floor(f);
         if (pvb->wSymmetry & (WORD)(1 << j))
            s *= -1;

         // weite
         pDeform->aiDeform[j] = s;
      }
   } // i


   // weights
   PVERTBINWEIGHT pWeight = (PVERTBINWEIGHT) pDeform;
   PBONEWEIGHT pBoneWeight;
   pBoneWeight = BoneWeightGet();
   for (i = 0; i < m_wNumBoneWeight; i++, pBoneWeight++, pWeight++) {
      pWeight->wNum = (WORD)pBoneWeight->dwBone;

      f = pBoneWeight->fWeight * (fp)0x100 + 0.5;
      f = min(f, 32767.0);
      f = max(f, -32767.0);
      pWeight->iWeight = (short)f;
   } // i

   // update current position and done
   pMem->m_dwCurPosn += dwNeed;
   return TRUE;
}


/**********************************************************************************
CPMVert::MMLTo - Standard MMLTo call.

NOTE: This does NOT write out the edge or side information since that can be recalculated.
*/
PCMMLNode2 CPMVert::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszVert);
   
   // variables
   MMLValueSet (pNode, gpszLoc, &m_pLoc);
   MMLValueSet (pNode, gpszText, &m_tText);

   // mirror info
   WORD i;
   PCMMLNode2 pSub;
#if 0 // dont save mirror info
   PPMMIRROR pMirror;
   pMirror = MirrorGet();
   for (i = 0; i < m_wNumMirror; i++, pMirror++) {
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMirror);
      MMLValueSet (pSub, gpszObject, (int)pMirror->dwObject);
      MMLValueSet (pSub, gpszType, (int)pMirror->dwType);
   }
#endif

   // side text info
   PSIDETEXT pSideText;
   pSideText = SideTextGet();
   for (i = 0; i < m_wNumSideText; i++, pSideText++) {
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszSideText);
      MMLValueSet (pSub, gpszObject, (int)pSideText->dwSide);
      MMLValueSet (pSub, gpszText, &pSideText->tp);
   }

   // deformation around point
   PVERTDEFORM pVertDeform;
   pVertDeform = VertDeformGet();
   for (i = 0; i < m_wNumVertDeform; i++, pVertDeform++) {
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszVertDeform);
      MMLValueSet (pSub, gpszObject, (int)pVertDeform->dwDeform);
      MMLValueSet (pSub, gpszDeform, &pVertDeform->pDeform);
   }

   // boneweight
   PBONEWEIGHT pBoneWeight;
   pBoneWeight = BoneWeightGet();
   for (i = 0; i < m_wNumBoneWeight; i++, pBoneWeight++) {
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszBoneWeight);
      MMLValueSet (pSub, gpszObject, (int)pBoneWeight->dwBone);
      MMLValueSet (pSub, gpszDeform, pBoneWeight->fWeight);
   }

   return pNode;
}



/**********************************************************************************
CPMVert::MMLFrom - Standard MMLFrom call.

NOTE: This does NOT fill in the edge or side information since that can be recalculated.
*/
BOOL CPMVert::MMLFrom (PCMMLNode2 pNode)
{
   // clrear current info
   Clear();


   // basic info
   MMLValueGetPoint (pNode, gpszLoc, &m_pLoc);
   MMLValueGetTEXTUREPOINT (pNode, gpszText, &m_tText);

   // loop though elems
   DWORD i;
   PCMMLNode2 pSub;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszSideText)) {
         SIDETEXT st;
         memset (&st, 0, sizeof(st));

         st.dwSide = (DWORD) MMLValueGetInt (pSub, gpszObject, 0);
         MMLValueGetTEXTUREPOINT (pSub, gpszText, &st.tp);
         SideTextAdd (&st);
      }
#if 0 // dont save symmetry info
      else if (!_wcsicmp(psz, gpszMirror)) {
         PMMIRROR m;
         memset (&m, 0 ,sizeof(m));
         m.dwObject = (DWORD) MMLValueGetInt (pSub, gpszObject, (int)0);
         m.dwType = (DWORD) MMLValueGetInt (pSub, gpszType, (int)0);
         MirrorAdd (&m);
      }
#endif 0
      else if (!_wcsicmp(psz, gpszVertDeform)) {
         VERTDEFORM vd;
         memset (&vd, 0, sizeof(vd));

         vd.dwDeform = (DWORD) MMLValueGetInt (pSub, gpszObject, 0);
         MMLValueGetPoint (pSub, gpszDeform, &vd.pDeform);
         VertDeformAdd (&vd);
      }
      else if (!_wcsicmp(psz, gpszBoneWeight)) {
         BONEWEIGHT vd;
         memset (&vd, 0, sizeof(vd));

         vd.dwBone = (DWORD) MMLValueGetInt (pSub, gpszObject, 0);
         vd.fWeight = MMLValueGetDouble (pSub, gpszDeform, 0);
         BoneWeightAdd (&vd);
      }
   } // i

   return TRUE;
}




/**********************************************************************************
CPMVert::MMLFromBinary - Standard MMLFrom call.

NOTE: This does NOT fill in the edge or side information since that can be recalculated.

inputs
   PBYTE          pab - Where to get data from
   DWORD          dwSize - Amount of data available
   PCPoint        pScale - Scale
   PTEXTUREPOINT  ptScale - Scale
returns
   DWORD - Size actually used
*/
DWORD CPMVert::MMLFromBinary (PBYTE pab, DWORD dwSize, PCPoint pScale, PTEXTUREPOINT ptScale)
{
   // clrear current info
   Clear();

   if (dwSize < sizeof(VERTBIN))
      return 0;
   PVERTBIN pvb = (PVERTBIN) pab;

   // how much size do we need in total
   DWORD dwNeed = sizeof(VERTBIN) +
      sizeof(VERTBINSIDE) * (DWORD)pvb->wSides +
      sizeof(VERTBINDEFORM) * (DWORD)pvb->wDeform +
      sizeof(VERTBINWEIGHT) * (DWORD)pvb->wWeight;
   if ((dwSize < dwNeed) || ((DWORD)pvb->wSize != dwNeed))
      return 0;

   // convert the points
   DWORD i, j;
   fp f;
   m_pLoc.Zero();
   for (j = 0; j < 3; j++) {
      // convert units
      f = (fp)pvb->aiLoc[j] * pScale->p[j];
      if (pvb->wSymmetry & (WORD)(1 << j))
         f *= -1;

      // write
      m_pLoc.p[j] = f;
   }
   for (j = 0; j < 2; j++) {
      // convert units
      f = (fp)pvb->aiText[j] * (j ? ptScale->v : ptScale->h);

      // write
      if (j)
         m_tText.v = f;
      else
         m_tText.h = f;
   } // j

   // sides
   PVERTBINSIDE pSide = (PVERTBINSIDE) (pvb + 1);
   SIDETEXT st;
   memset (&st, 0, sizeof(st));
   for (i = 0; i < pvb->wSides; i++, pSide++) {
      st.dwSide = pSide->wNum;

      for (j = 0; j < 2; j++) {
         // convert units
         f = (fp)pSide->aiText[j] * (j ? ptScale->v : ptScale->h);

         // write
         if (j)
            st.tp.v = f;
         else
            st.tp.h = f;
      } // j

      SideTextAdd (&st);
   }

   // deformations
   PVERTBINDEFORM pDeform = (PVERTBINDEFORM) pSide;
   VERTDEFORM vd;
   memset (&vd, 0, sizeof(vd));
   for (i = 0; i < pvb->wDeform; i++, pDeform++) {
      vd.dwDeform = pDeform->wNum;
      for (j = 0; j < 3; j++) {
         // convert units
         f = (fp)pDeform->aiDeform[j] * pScale->p[j];
         if (pvb->wSymmetry & (WORD)(1 << j))
            f *= -1;

         // write
         vd.pDeform.p[j] = f;
      }

      VertDeformAdd (&vd);
   } // i


   // weights
   PVERTBINWEIGHT pWeight = (PVERTBINWEIGHT) pDeform;
   BONEWEIGHT bw;
   memset (&vd, 0, sizeof(vd));
   for (i = 0; i < pvb->wWeight; i++, pWeight++) {
      bw.dwBone = pWeight->wNum;
      bw.fWeight = (fp)pWeight->iWeight / (fp)0x100;

      BoneWeightAdd (&bw);
   } // i

   // update current position and done
   return dwNeed;
}



/**********************************************************************************
CPMSide::CPMSide - Construcor and destructor
*/
CPMSide::CPMSide (void)
{
   Clear();
}
// destructor not needed

/**********************************************************************************
CPMSide::Clear - Clears out all the settings
*/
void CPMSide::Clear (void)
{
   m_dwSurfaceText = m_dwOrigSide = 0;
   m_wNumMirror = m_wNumVert =0;
   m_pNorm.Zero();
}


/**********************************************************************************
CPMSide::Clone -Standard clone

inputs
   BOOL           fKeepDeform - If TRUE keeps deform info, FALSE then keep only
                  information necessary for subdivision
*/
CPMSide *CPMSide::Clone (BOOL fKeepDeform)
{
   PCPMSide pNew = new CPMSide;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew, fKeepDeform)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}

/**********************************************************************************
CPMSide::CloneTo - Copies all the data from the current vertex to the new one.
It ASSUMES the new one has NO memory allocated, since everything is copied over.

inputs
   PCPMVertOld    *pTo - Copies the info to
   BOOL           fKeepDeform - If TRUE keeps deform info, FALSE then keep only
                  information necessary for subdivision
*/
BOOL CPMSide::CloneTo (CPMSide *pTo, BOOL fKeepDeform)
{
   pTo->m_dwSurfaceText = m_dwSurfaceText;
   pTo->m_dwOrigSide = m_dwOrigSide;
   pTo->m_pNorm.Copy (&m_pNorm);

   // transfer other stuff over
   DWORD dwNeed = fKeepDeform ? (SMAXPMOFFSET) : (sizeof(DWORD)*m_wNumVert);
   if (!pTo->m_memMisc.Required (dwNeed))
      return FALSE;
   pTo->m_wNumVert = m_wNumVert;
   if (fKeepDeform) {
      memcpy (pTo->m_memMisc.p, m_memMisc.p, dwNeed);
      pTo->m_wNumMirror = m_wNumMirror;
   }
   else {
      pTo->m_wNumMirror = 0;
      memcpy (pTo->VertGet(), VertGet(), sizeof(DWORD)*m_wNumVert);
   }

   return TRUE;
}

static PWSTR gpszSide = L"Side";

/**********************************************************************************
CPMSide::MMLTo - Standard MMLTo call.

NOTE: This does NOT write out the edge information, or m_dwOrigSide, or m_pNorm
since that can be recalculated.
*/
PCMMLNode2 CPMSide::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;

   pNode->NameSet (gpszSide);
   
   // variables
   MMLValueSet (pNode, gpszText, (int)m_dwSurfaceText);

   // mirror info
   WORD i;
   PCMMLNode2 pSub;
#if 0 // dont save symmetry info
   PPMMIRROR pMirror;
   pMirror = MirrorGet();
   for (i = 0; i < m_wNumMirror; i++, pMirror++) {
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszMirror);
      MMLValueSet (pSub, gpszObject, (int)pMirror->dwObject);
      MMLValueSet (pSub, gpszType, (int)pMirror->dwType);
   }
#endif // 0

   // Vertex text info
   DWORD *padwVert;
   padwVert = VertGet();
   for (i = 0; i < m_wNumVert; i++, padwVert++) {
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszVert);
      MMLValueSet (pSub, gpszObject, (int)padwVert[0]);
   }

   return pNode;
}


// SIDEBIN - Binary for storing side information
typedef struct {
   WORD           wSize;      // total size, in bytes
   WORD           wText;      // texture number
   WORD           wNumVert;   // number of vertecies stored
   // array of WORDs for vertex
} SIDEBIN, *PSIDEBIN;

/**********************************************************************************
CPMSide::MMLToBianry - Standard MMLTo call.

NOTE: This does NOT write out the edge information, or m_dwOrigSide, or m_pNorm
since that can be recalculated.
*/
BOOL CPMSide::MMLToBinary (PCMem pMem)
{
   DWORD dwNeed = sizeof(SIDEBIN) + sizeof(WORD) * (DWORD)m_wNumVert;
   if (!pMem->Required(pMem->m_dwCurPosn + dwNeed))
      return FALSE;
   if (dwNeed >= 0x10000)
      return FALSE;

   PSIDEBIN psb = (PSIDEBIN)((PBYTE)pMem->p + pMem->m_dwCurPosn);
   psb->wSize = (WORD)dwNeed;
   psb->wText = (WORD)m_dwSurfaceText;
   psb->wNumVert = m_wNumVert;

   WORD *paw = (WORD*)(psb+1);
   DWORD i;
   DWORD *padwVert;
   padwVert = VertGet();
   for (i = 0; i < m_wNumVert; i++, padwVert++, paw++)
      paw[0] = (WORD)padwVert[0];

   // done
   pMem->m_dwCurPosn += dwNeed;
   return TRUE;
}


/**********************************************************************************
CPMSide::MMLFrom - Standard MMLFrom call.

NOTE: This does NOT fill in the edge or side information since that can be recalculated.
*/
BOOL CPMSide::MMLFrom (PCMMLNode2 pNode)
{
   // clrear current info
   Clear();


   // basic info
   m_dwSurfaceText = (DWORD)MMLValueGetInt (pNode, gpszText, (int)0);

   // loop though elems
   DWORD i;
   PCMMLNode2 pSub;
   PWSTR psz;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;

      if (!_wcsicmp(psz, gpszVert)) {
         DWORD dwVert;
         dwVert = (DWORD) MMLValueGetInt (pSub, gpszObject, 0);
         VertAdd (&dwVert);
      }
#if 0 // dont save symmetry info
      else if (!_wcsicmp(psz, gpszMirror)) {
         PMMIRROR m;
         memset (&m, 0 ,sizeof(m));
         m.dwObject = (DWORD) MMLValueGetInt (pSub, gpszObject, (int)0);
         m.dwType = (DWORD) MMLValueGetInt (pSub, gpszType, (int)0);
         MirrorAdd (&m);
      }
#endif
   } // i

   return TRUE;
}


/**********************************************************************************
CPMSide::MMLFromBinary - Standard MMLFrom call.

NOTE: This does NOT fill in the edge or side information since that can be recalculated.

inputs
   PBYTE          pab - Where to get data from
   DWORD          dwSize - Amount of data available
returns
   DWORD - Size actually used
*/
DWORD CPMSide::MMLFromBinary (PBYTE pab, DWORD dwSize)
{
   // clrear current info
   Clear();

   if (dwSize < sizeof(SIDEBIN))
      return 0;
   PSIDEBIN psb = (PSIDEBIN) pab;

   // how much size do we need in total
   DWORD dwNeed = sizeof(SIDEBIN) + sizeof(WORD) * (DWORD)psb->wNumVert;
   if ((dwSize < dwNeed) || ((DWORD)psb->wSize != dwNeed))
      return 0;

   // basic info
   m_dwSurfaceText = psb->wText;

   WORD *paw = (WORD*)(psb+1);
   DWORD i;
   DWORD dwVert;
   for (i = 0; i < psb->wNumVert; i++, paw++) {
      dwVert = paw[0];
      VertAdd (&dwVert);
   }

   return dwNeed;
}

/*************************************************************************************
CPMSide::HasAMirror - Checks to see if the side has a mirror. SymmetryRecalc()
must have been called before for this to work

inputs
   DWORD       dwSymmetry - From m_dwSymmetry
   PCPMVert    *ppv - List of vertices

*/
BOOL CPMSide::HasAMirror (DWORD dwSymmetry, PCPMVert *ppv)
{
   // NOTE: Not a foolproof test for has-a-mirror
   DWORD i;
   DWORD *padw = VertGet();
   for (i = 0; i < m_wNumVert; i++)
      if (!ppv[padw[i]]->HasAMirror(dwSymmetry))
         return FALSE;

   return TRUE;
}


/*************************************************************************************
CPMSide::XXXGet - Returns a pointer to the beginning of m_wNumXXX elems of the
type specified. While individual elements can be changed, to add/remove elements use
one of the XXXAdd or XXXRemove functions
*/
__inline PMMIRROR *CPMSide::MirrorGet (void)
{
   return (PMMIRROR*)((PBYTE) m_memMisc.p + SMIRROROFFSET);
}

__inline DWORD *CPMSide::VertGet (void)
{
   return (DWORD*)((PBYTE) m_memMisc.p + SVERTOFFSET);
}

__inline PVOID CPMSide::MaxPMGet (void)
{
   return (PVOID)((PBYTE) m_memMisc.p + SMAXPMOFFSET);
}



/**********************************************************************************
CPMSide::RotateVert - Rotates the vertices to the right (or left) one elem. Used
to fix triangulation problems.

inputs
   BOOL        fRight - If TRUE rotate to the right
*/
void CPMSide::RotateVert (BOOL fRight)
{
   if (m_wNumVert < 2)
      return;  // nothing to do
   DWORD *padwVert = VertGet();
   DWORD dw;

   if (fRight) {
      dw = padwVert[m_wNumVert-1];
      memmove (padwVert+1, padwVert, sizeof(DWORD)*(m_wNumVert-1));
      padwVert[0] = dw;
   }
   else {
      dw = padwVert[0];
      memmove (padwVert, padwVert+1, sizeof(DWORD)*(m_wNumVert-1));
      padwVert[m_wNumVert-1] = dw;
   }
}


/*************************************************************************************
CPMSide::XXXAdd - Adds the given number of elements on to the list. It does NOT check
for duplicates. Returns TRUE if success, FALSE if fail.

NOTE: Calling this may invalidate previous pointers from XXXGet().
*/
BOOL CPMSide::MirrorAdd (PMMIRROR *pMirror, WORD wNum)
{
   DWORD dwHave, dwNeed, dwExtra;
   PBYTE pMove;
   dwExtra = wNum * sizeof(*pMirror);
   dwHave = SMAXPMOFFSET;
   dwNeed = dwHave + dwExtra;
   if (!m_memMisc.Required (dwNeed))
      return FALSE;
   pMove = (PBYTE)VertGet(); // changewhencopy

   DWORD dwCopy;
   dwCopy = (DWORD)(((PBYTE)m_memMisc.p + dwHave) - (PBYTE)pMove);
   memmove (pMove + dwExtra, pMove, dwCopy);
   memcpy (pMove, pMirror, dwExtra);
   m_wNumMirror += wNum; // changewhencopy
   return TRUE;
}

BOOL CPMSide::VertAdd (DWORD *padwSide, WORD wNum)
{
   DWORD dwHave, dwNeed, dwExtra;
   PBYTE pMove;
   dwExtra = wNum * sizeof(*padwSide);
   dwHave = SMAXPMOFFSET;
   dwNeed = dwHave + dwExtra;
   if (!m_memMisc.Required (dwNeed))
      return FALSE;
   pMove = (PBYTE)MaxPMGet(); // changewhencopy

   DWORD dwCopy;
   dwCopy = (DWORD)(((PBYTE)m_memMisc.p + dwHave) - (PBYTE)pMove);
   memmove (pMove + dwExtra, pMove, dwCopy);
   memcpy (pMove, padwSide, dwExtra);
   m_wNumVert += wNum; // changewhencopy
   return TRUE;
}



/*************************************************************************************
CPMSide::VertInsert - Inserts the given vertex before the index number (into the
list of vertices

inputs
   DWORD          dwVert - Vertex number to insert
   DWORD          dwIndex - Index to insert before
returns
   BOOL - TRUE if success
*/
BOOL CPMSide::VertInsert (DWORD dwVert, DWORD dwIndex)
{
   DWORD dwOldNum = m_wNumVert;
   DWORD *padwVert;

   if (!VertAdd (&dwVert))
      return FALSE;

   padwVert = VertGet();
   memmove (padwVert + (dwIndex+1), padwVert + dwIndex,
      (dwOldNum - dwIndex) * sizeof(DWORD));
   padwVert[dwIndex] = dwVert;
   return TRUE;
}

/*************************************************************************************
CPMSide::VertFind - Given a vertex, search through the list of vertices in the side
and returns the index into that vertex. Or -1 if cant find.

inputs
   DWORD          dwVert - looking for
returns
   DWORD - Index where found, or -1 if cant find
*/
DWORD CPMSide::VertFind (DWORD dwVert)
{
   DWORD *padwVert = VertGet();
   DWORD i;
   for (i = 0; i < m_wNumVert; i++)
      if (padwVert[i] == dwVert)
         return i;

   return -1;
}


/*************************************************************************************
CPMSide::XXXRemove - Loops through all the elements for the type XXX. If it finds
any that match dwRemove they're deleted. If the value is greater than dwRemove the
value is decreased by 1.
*/
void CPMSide::MirrorRemove (DWORD dwRemove)
{
   WORD i;
   PMMIRROR *pCur = MirrorGet(); // changeline
   for (i = 0; i < m_wNumMirror; ) {   // changeline
      if (pCur[i].dwObject < dwRemove) {
         i++;
         continue;   // no change
      }
      if (pCur[i].dwObject > dwRemove) {
         pCur[i].dwObject--;
         i++;
         continue;
      }
      
      // else, match
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumMirror--;   // changeline
      // dont increase i
   }
}

void CPMSide::VertRemove (DWORD dwRemove)
{
   WORD i;
   DWORD *pCur = VertGet(); // changeline
   for (i = 0; i < m_wNumVert; ) {   // changeline
      if (pCur[i] < dwRemove) {
         i++;
         continue;   // no change
      }
      if (pCur[i] > dwRemove) {
         pCur[i]--;
         i++;
         continue;
      }
      
      // else, match
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumVert--;   // changeline
      // dont increase i
   }
}


/********************************************************************************
CPMSide::VertRemoveByIndex - Remove a vertex based on the index into the side

inputs
   DWORD          dwInex - From 0..m_wNumVert-1
*/
void CPMSide::VertRemoveByIndex (DWORD dwIndex)
{
   DWORD *pCur = VertGet(); // changeline
   memmove (pCur + dwIndex, pCur + (dwIndex+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (dwIndex+1)));
   m_wNumVert--;   // changeline
}



/*************************************************************************************
CPMSide::XXXClear - Removes all the elements from the vertex
*/
void CPMSide::MirrorClear (void)
{
   if (m_wNumMirror) {
      PMMIRROR *pCur = MirrorGet();
      PBYTE pFrom = (PBYTE)(pCur + m_wNumMirror);
      memmove (pCur, pFrom, (PBYTE) (MaxPMGet()) - pFrom);
      m_wNumMirror = 0;
   }
}

void CPMSide::VertClear (void)
{
   if (m_wNumVert) {
      DWORD *pCur = VertGet();
      PBYTE pFrom = (PBYTE)(pCur + m_wNumVert);
      memmove (pCur, pFrom, (PBYTE) (MaxPMGet()) - pFrom);
      m_wNumVert = 0;
   }
}



/*************************************************************************************
CPMSide::XXXRename - Looks through all the elements. Anythign with an ID in the padwOrig
list (dwNum = number of elements) is renamed to padwNew's corresponding value.
If padwNew is -1 then the element is deleted.
*/
void CPMSide::MirrorRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   WORD i;
   PMMIRROR *pCur = MirrorGet(); // changeline
   for (i = 0; i < m_wNumMirror; ) {   // changeline
      DWORD dwVal = DWORDSearch (pCur[i].dwObject, dwNum, padwOrig);
      if (dwVal == -1) {
         // not found
         i++;
         continue;
      }

      // else if padwNew[dwVal] != -1 then just change
      if (padwNew[dwVal] != -1) {
         pCur[i].dwObject = padwNew[dwVal];
         i++;
         continue;
      }

      // else, delete
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumMirror--;   // changeline
      // dont increase i
   }
}


void CPMSide::VertRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   WORD i;
   DWORD *pCur = VertGet(); // changeline
   for (i = 0; i < m_wNumVert; ) {   // changeline
      DWORD dwVal = DWORDSearch (pCur[i], dwNum, padwOrig);
      if (dwVal == -1) {
         // not found
         i++;
         continue;
      }

      // else if padwNew[dwVal] != -1 then just change
      if (padwNew[dwVal] != -1) {
         pCur[i] = padwNew[dwVal];
         i++;
         continue;
      }

      // else, delete
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumVert--;   // changeline
      // dont increase i
   }
}




/*************************************************************************************
CPMSide::CalcNorm - Calculates the normal for the side.

inputs
   PCPolyMesh        pMesh - Polygon mesh that can get the vertex information from
*/
void CPMSide::CalcNorm (PCPolyMesh pMesh)
{
   // calculates the normal for the side...
   m_pNorm.Zero();

   // triangulate
   WORD i;
   PCPoint pA, pB, pC;
   CPoint p1, p2, pN;
   fp fLen;
   DWORD *padwVert = VertGet ();
   for (i = 0; i < m_wNumVert; i++) {
      pA = &pMesh->VertGet(padwVert[i])->m_pLoc;   // BUGFIX - Was always using 0, but this is better to use i
      pB = &pMesh->VertGet(padwVert[(i+1)%m_wNumVert])->m_pLoc;
      pC = &pMesh->VertGet(padwVert[(i+2)%m_wNumVert])->m_pLoc;

      // BUGFIX - Sometimes get normals messed up when p1 is equal to p2
      // so fix by ignoring if very close
      if (i) {
         // use old p2
         p1.Copy (&p2);
         p1.Scale (-1);
      }
      else {
         // calculate
         p1.Subtract (pA, pB);
         p1.Normalize();
      }
      p2.Subtract (pC, pB);
      p2.Normalize();

      pN.CrossProd (&p1, &p2);
      fLen = pN.Length();
      if (fLen < .01)
         continue;
      pN.Scale (1.0 / fLen);
      m_pNorm.Add (&pN);

      // if only 3 vertices might as well break here
      if (m_wNumVert <= 3)
         break;
   } // i

   if (m_wNumVert > 3)
      m_pNorm.Normalize();
}


/*************************************************************************************
CPMSide::Reverse - Reverses the order of the points in the side
*/
void CPMSide::Reverse (void)
{
   WORD i;
   DWORD dw;
   DWORD *padwVert = VertGet();
   for (i = 0; i < m_wNumVert/2; i++) {
      dw = padwVert[i];
      padwVert[i] = padwVert[m_wNumVert - i - 1];
      padwVert[m_wNumVert - i - 1] = dw;
   }

   // BUGFIX - Rotate the vertices so the original first point is still the first point
   RotateVert (TRUE);
}







/*************************************************************************************
CPMEdge::Constructor and destructor
*/
CPMEdge::CPMEdge (void)
{
   Clear();
}
// no destructor for now

/*************************************************************************************
CPMEdge::Clear - Clears out all the params
*/
void CPMEdge::Clear (void)
{
   m_adwSide[0] = m_adwSide[1] = -1;
   m_adwVert[0] = m_adwVert[1] = 0;
   m_apTree[0] = m_apTree[1] = NULL;
#ifdef MIRROREDGE // Dont store mirror info in edges
   m_wNumMirror = 0;
#endif
}


/**********************************************************************************
CPMEdge::Clone -Standard clone
*/
CPMEdge *CPMEdge::Clone (void)
{
   PCPMEdge pNew = new CPMEdge;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}


/**********************************************************************************
CPMEdge::CloneTo - Copies all the data from the current vertex to the new one.
It ASSUMES the new one has NO memory allocated, since everything is copied over.

inputs
   CPMEdge    *pTo - Copies the info to
*/
BOOL CPMEdge::CloneTo (CPMEdge *pTo)
{
   pTo->m_adwSide[0] = m_adwSide[0];
   pTo->m_adwSide[1] = m_adwSide[1];
   pTo->m_adwVert[0] = m_adwVert[0];
   pTo->m_adwVert[1] = m_adwVert[1];
   pTo->m_apTree[0] = pTo->m_apTree[1] = NULL;  // cant really clone tree links

#ifdef MIRROREDGE // Dont store mirror info in edges

   // transfer other stuff over
   DWORD dwNeed = EMAXPMOFFSET;
   if (!pTo->m_memMisc.Required (dwNeed))
      return FALSE;
   memcpy (pTo->m_memMisc.p, m_memMisc.p, dwNeed);
   pTo->m_wNumMirror = m_wNumMirror;
#endif

   return TRUE;
}

/*************************************************************************************
CPMEdge::XXXGet - Returns a pointer to the beginning of m_wNumXXX elems of the
type specified. While individual elements can be changed, to add/remove elements use
one of the XXXAdd or XXXRemove functions
*/
#ifdef MIRROREDGE // Dont store mirror info in edges
__inline PMMIRROR *CPMEdge::MirrorGet (void)
{
   return (PMMIRROR*)((PBYTE) m_memMisc.p + EMIRROROFFSET);
}

__inline PVOID CPMEdge::MaxPMGet (void)
{
   return (PVOID)((PBYTE) m_memMisc.p + EMAXPMOFFSET);
}
#endif

/*************************************************************************************
CPMEdge::HasAMirror - Checks to see if the edge has a mirror. SymmetryRecalc()
must have been called before for this to work

inputs
   DWORD       dwSymmetry - From m_dwSymmetry
   PCPMVert    *ppv - List of vertices

*/
BOOL CPMEdge::HasAMirror (DWORD dwSymmetry, PCPMVert *ppv)
{
   // NOTE: Not a foolproof test for has-a-mirror
   DWORD i;
   for (i = 0; i < 2; i++)
      if (!ppv[m_adwVert[i]]->HasAMirror(dwSymmetry))
         return FALSE;

   return TRUE;
}



/*************************************************************************************
CPMEdge::XXXAdd - Adds the given number of elements on to the list. It does NOT check
for duplicates. Returns TRUE if success, FALSE if fail.

NOTE: Calling this may invalidate previous pointers from XXXGet().
*/
#ifdef MIRROREDGE // Dont store mirror info in edges

BOOL CPMEdge::MirrorAdd (PMMIRROR *pMirror, WORD wNum)
{
   DWORD dwHave, dwNeed, dwExtra;
   PBYTE pMove;
   dwExtra = wNum * sizeof(*pMirror);
   dwHave = EMAXPMOFFSET;
   dwNeed = dwHave + dwExtra;
   if (!m_memMisc.Required (dwNeed))
      return FALSE;
   pMove = (PBYTE)MaxPMGet(); // changewhencopy

   DWORD dwCopy;
   dwCopy = (DWORD)(((PBYTE)m_memMisc.p + dwHave) - (PBYTE)pMove);
   memmove (pMove + dwExtra, pMove, dwCopy);
   memcpy (pMove, pMirror, dwExtra);
   m_wNumMirror += wNum; // changewhencopy
   return TRUE;
}
#endif


/*************************************************************************************
CPMEdge::XXXRemove - Loops through all the elements for the type XXX. If it finds
any that match dwRemove they're deleted. If the value is greater than dwRemove the
value is decreased by 1.
*/
#ifdef MIRROREDGE // Dont store mirror info in edges
void CPMEdge::MirrorRemove (DWORD dwRemove)
{
   WORD i;
   PMMIRROR *pCur = MirrorGet(); // changeline
   for (i = 0; i < m_wNumMirror; ) {   // changeline
      if (pCur[i].dwObject < dwRemove) {
         i++;
         continue;   // no change
      }
      if (pCur[i].dwObject > dwRemove) {
         pCur[i].dwObject--;
         i++;
         continue;
      }
      
      // else, match
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumMirror--;   // changeline
      // dont increase i
   }
}
#endif



/*************************************************************************************
CPMEdge::XXXClear - Removes all the elements from the vertex
*/
#ifdef MIRROREDGE // Dont store mirror info in edges
void CPMEdge::MirrorClear (void)
{
   if (m_wNumMirror) {
      PMMIRROR *pCur = MirrorGet();
      PBYTE pFrom = (PBYTE)(pCur + m_wNumMirror);
      memmove (pCur, pFrom, (PBYTE) (MaxPMGet()) - pFrom);
      m_wNumMirror = 0;
   }
}
#endif



/*************************************************************************************
CPMEdge::XXXRename - Looks through all the elements. Anythign with an ID in the padwOrig
list (dwNum = number of elements) is renamed to padwNew's corresponding value.
If padwNew is -1 then the element is deleted.
*/
#ifdef MIRROREDGE // Dont store mirror info in edges
void CPMEdge::MirrorRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   WORD i;
   PMMIRROR *pCur = MirrorGet(); // changeline
   for (i = 0; i < m_wNumMirror; ) {   // changeline
      DWORD dwVal = DWORDSearch (pCur[i].dwObject, dwNum, padwOrig);
      if (dwVal == -1) {
         // not found
         i++;
         continue;
      }

      // else if padwNew[dwVal] != -1 then just change
      if (padwNew[dwVal] != -1) {
         pCur[i].dwObject = padwNew[dwVal];
         i++;
         continue;
      }

      // else, delete
      memmove (pCur + i, pCur + (i+1), (PBYTE)MaxPMGet() - (PBYTE)(pCur + (i+1)));
      m_wNumMirror--;   // changeline
      // dont increase i
   }
}
#endif

void CPMEdge::VertRename (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   WORD i;
   WORD m_wNumVert = 2;
   DWORD *pCur = m_adwVert; // changeline
   for (i = 0; i < m_wNumVert; ) {   // changeline
      DWORD dwVal = DWORDSearch (pCur[i], dwNum, padwOrig);
      if (dwVal == -1) {
         // not found
         i++;
         continue;
      }

      // else if padwNew[dwVal] != -1 then just change
      if (padwNew[dwVal] != -1) {
         pCur[i] = padwNew[dwVal];
         i++;
         continue;
      }

      // else, delete, but cant really do that
      pCur[i] = 0;
      i++;
   }

   // need to resort list
   VertSet (m_adwVert[0], m_adwVert[1]);
}


/*************************************************************************************
CPMEdge::VertSet - Sets the current vertex indecies for the edges. This should be
called instead of setting the variables directly because it makes sure the lowest
vertex appears first.
*/
void CPMEdge::VertSet (DWORD dwVert1, DWORD dwVert2)
{
   if (dwVert1 < dwVert2) {
      m_adwVert[0] = dwVert1;
      m_adwVert[1] = dwVert2;
   }
   else {
      m_adwVert[0] = dwVert2;
      m_adwVert[1] = dwVert1;
   }
}





/**************************************************************************************
CPolyMesh::Constructor and destructor
*/
CPolyMesh::CPolyMesh (void)
{
   m_lVert.Init (sizeof(PCPMVert));
   m_lSide.Init (sizeof(PCPMSide));
   m_lEdge.Init (sizeof(PCPMEdge));
   m_lRenderSurface.Init (sizeof(DWORD));
   m_lPCOEAttrib.Init (sizeof(PCOEAttrib));
   m_lMorphRemapID.Init (sizeof(DWORD));
   m_lSurfaceNotInMeters.Init (sizeof(DWORD));
   m_lMorphRemapValue.Init (sizeof(fp));
   m_lMorphCOM.Init (sizeof(CPoint));
   m_dwSubdivide = 1;
   m_fBonesAlreadyApplied = FALSE;
   Clear();
}

CPolyMesh::~CPolyMesh (void)
{
   Clear();
}

/**************************************************************************************
CPolyMesh::Clear - Wipes out the entire contents of the polymesh
*/
void CPolyMesh::Clear (void)
{
   m_fCanBackface = TRUE;
   m_dwSymmetry = 0;
   m_fDirtyRender = TRUE;
   m_fDirtyMorphCOM = TRUE;
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtySymmetry = FALSE;  // not dirty since nothing here
   m_pBoundMin.Zero();
   m_pBoundMax.Zero();
   m_dwSpecialLast = ORSPECIAL_NONE;
   m_dwSelMode = 0;
   m_lSel.Init (sizeof(DWORD));
   m_lSel.Clear();
   m_fSelSorted = TRUE;
   memset (&m_gBone, 0, sizeof(m_gBone));
   m_fBoneMirrorValid = FALSE;
   m_dwBoneDisplay = -1;
   m_fBoneChangeShape = FALSE;
   m_lPMBONEINFO.Init (sizeof(PMBONEINFO));
   m_mBoneLocLastRender.Identity();

   // wipe out existing
   DWORD i;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   PCPMEdge *ppe = (PCPMEdge*) m_lEdge.Get(0);
   for (i = 0; i < m_lVert.Num(); i++)
      delete ppv[i];
   for (i = 0; i < m_lSide.Num(); i++)
      delete pps[i];
   for (i = 0; i < m_lEdge.Num(); i++)
      delete ppe[i];
   m_lVert.Clear();
   m_lSide.Clear();
   m_lEdge.Clear();

   // attributes
   ObjEditClearPCOEAttribList (&m_lPCOEAttrib);
   m_lMorphRemapID.Clear();
   m_lSurfaceNotInMeters.Clear();
   m_lMorphRemapValue.Clear();
   m_lMorphCOM.Clear();
}



/**********************************************************************************
CPolyMesh::Clone -Standard clone

inputs
   BOOL              fKeepDeform - If TRUE then keep the deformation, else get rid
                     of them to make cloning as fast as possible
*/
CPolyMesh *CPolyMesh::Clone (BOOL fKeepDeform)
{
   PCPolyMesh pNew = new CPolyMesh;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew, fKeepDeform)) {
      delete pNew;
      return NULL;
   }
   return pNew;
}




/**************************************************************************************
CPolyMesh::CloneTo - Standard clone call

inputs
   PCPolyMesh        *pTo - Where to clone to
   BOOL              fKeepDeform - If TRUE then keep the deformation, else get rid
                     of them to make cloning as fast as possible
*/
BOOL CPolyMesh::CloneTo (CPolyMesh *pTo, BOOL fKeepDeform)
{
   // clear whatever is there
   pTo->Clear();

   pTo->m_fCanBackface = m_fCanBackface;
   pTo->m_dwSymmetry = m_dwSymmetry;
   pTo->m_dwSubdivide = m_dwSubdivide;
   pTo->m_dwSpecialLast =m_dwSpecialLast;
   pTo->m_fDirtyRender = TRUE;   // BUGFIX - Set this to always be true since not copying over renderin info
   pTo->m_fDirtyMorphCOM = TRUE; // since not copying over com
   pTo->m_fDirtyEdge = fKeepDeform ? m_fDirtyEdge : TRUE;
   pTo->m_fDirtyNorm = m_fDirtyNorm;
   pTo->m_fDirtySymmetry = fKeepDeform ? m_fDirtySymmetry : TRUE;
   pTo->m_fDirtyScale = m_fDirtyScale;
   pTo->m_pBoundMin.Copy (&m_pBoundMin);
   pTo->m_pBoundMax.Copy (&m_pBoundMax);
   pTo->m_dwSelMode = m_dwSelMode;
   pTo->m_fSelSorted = m_fSelSorted;
   pTo->m_lSel.Init (((m_dwSelMode == 1) ? 2 : 1) * sizeof(DWORD), m_lSel.Get(0), m_lSel.Num());

   pTo->m_gBone = m_gBone;
   pTo->m_fBoneMirrorValid =m_fBoneMirrorValid;
   pTo->m_dwBoneDisplay = m_dwBoneDisplay;
   pTo->m_fBoneChangeShape = m_fBoneChangeShape;
   pTo->m_lPMBONEINFO.Init (sizeof(PMBONEINFO), m_lPMBONEINFO.Get(0), m_lPMBONEINFO.Num());
   pTo->m_mBoneLocLastRender.Copy (&m_mBoneLocLastRender);

   // transfer over vertex info
   DWORD i;
   pTo->m_lVert.Init (sizeof(PCPMVert), m_lVert.Get(0), m_lVert.Num());
   pTo->m_lSide.Init (sizeof(PCPMSide), m_lSide.Get(0), m_lSide.Num());
   // BUGFIX - Dont clone pTo->m_lEdge.Init (sizeof(PCPMEdge), m_lEdge.Get(0), m_lEdge.Num());
   PCPMVert *ppv = (PCPMVert*) pTo->m_lVert.Get(0);
   PCPMSide *pps = (PCPMSide*) pTo->m_lSide.Get(0);
   // BUGFIX - Dont clone PCPMEdge *ppe = (PCPMEdge*) pTo->m_lEdge.Get(0);
   for (i = 0; i < pTo->m_lVert.Num(); i++)
      ppv[i] = ppv[i]->Clone(fKeepDeform);
   for (i = 0; i < pTo->m_lSide.Num(); i++)
      pps[i] = pps[i]->Clone(fKeepDeform);
   // BUGFIX - Dont clone for (i = 0; i < pTo->m_lEdge.Num(); i++)
   // BUGFIX - Dont clone    ppe[i] = ppe[i]->Clone();

   if (fKeepDeform) {
      pTo->m_lPCOEAttrib.Required (m_lPCOEAttrib.Num());
      for (i = 0; i < m_lPCOEAttrib.Num(); i++) {
         PCOEAttrib pa = *((PCOEAttrib*) m_lPCOEAttrib.Get(i));
         PCOEAttrib pNewA = new COEAttrib;
         if (!pNewA)
            continue;
         pa->CloneTo (pNewA);

         pTo->m_lPCOEAttrib.Add (&pNewA);
      }
      pTo->m_lMorphRemapID.Init (sizeof(DWORD), m_lMorphRemapID.Get(0), m_lMorphRemapID.Num());
      pTo->m_lMorphRemapValue.Init (sizeof(fp), m_lMorphRemapValue.Get(0), m_lMorphRemapValue.Num());
      // NOTE: Not transferring m_lMorphCOM
   }

   // transfer over in meters
   pTo->m_lSurfaceNotInMeters.Init (sizeof(DWORD), m_lSurfaceNotInMeters.Get(0), m_lSurfaceNotInMeters.Num());

   // BUGFIX - Because store edges as a tree (for fast access) need to
   // mark edges as invlaid when clone
   pTo->m_fDirtyEdge = TRUE;
   return TRUE;
}

static PWSTR gpszPolyMesh = L"PolyMesh";
static PWSTR gpszCanBackface = L"CanBackface";
static PWSTR gpszSymmetry = L"Symmetry";
static PWSTR gpszSubdivide = L"Subdivide";
static PWSTR gpszBone = L"Bone";
static PWSTR gpszBoneGUID = L"BoneGUID";
static PWSTR gpszName = L"Name";
static PWSTR gpszLocScale = L"LocScale";
static PWSTR gpszTextScale = L"TextScale";
static PWSTR gpszBinary = L"Binary";

// POLYMESHBIN - Header for the polymesh binary
typedef struct {
   WORD           wNumVert;      // number of verticies
   WORD           wNumSide;      // number of sides
   // followed by MMLToBinary for each vertex
   // followed by MMLToBinary for each side
} POLYMESHBIN, *PPOLYMESHBIN;

/**************************************************************************************
CPolyMesh::MMLTo - Standard MMLTo
*/
PCMMLNode2 CPolyMesh::MMLTo (void)
{
   // clean the sidetext entries
   VertSideTextClean ();

   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszPolyMesh);

   // data
   MMLValueSet (pNode, gpszCanBackface, (int)m_fCanBackface);
   MMLValueSet (pNode, gpszSymmetry, (int)m_dwSymmetry);
   MMLValueSet (pNode, gpszSubdivide, (int)m_dwSubdivide);
   // NOTE: Not writing an selection information

   DWORD i;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);


   if ((m_lVert.Num() < 0x10000) && (m_lSide.Num() < 0x10000)) {
      // determine min and max
      CPoint pLocMin, pLocMax;
      TEXTUREPOINT tpTextMin, tpTextMax;
      for (i = 0; i < 3; i++) {
         pLocMin.p[i] = FLT_MAX;
         pLocMax.p[i] = -FLT_MAX;
      }
      tpTextMax.h = tpTextMax.v = -FLT_MAX;
      tpTextMin.h = tpTextMin.v = FLT_MAX;
      for (i = 0; i < m_lVert.Num(); i++)
         ppv[i]->MMLToBinaryCalcMinMax (&pLocMax, &pLocMin, &tpTextMax, &tpTextMin);

      // convert this to a scaling factor
      CPoint pMax;
      TEXTUREPOINT tpMax;
      pMax.Zero();
      for (i = 0; i < 3; i++) {
         if (pLocMin.p[i] == FLT_MAX) {
            pMax.p[i] = CLOSE;
            continue;   // nothing
         }
         pMax.p[i] = max(fabs(pLocMin.p[i]), fabs(pLocMax.p[i]));
         pMax.p[i] = max(pMax.p[i], CLOSE);
      } // i
      if (tpTextMin.h == FLT_MAX)
         tpMax.h = tpMax.v = CLOSE;
      else {
         tpMax.h = max(fabs(tpTextMax.h), fabs(tpTextMin.h));
         tpMax.v = max(fabs(tpTextMax.v), fabs(tpTextMin.v));
         tpMax.h = max(tpMax.h, CLOSE);
         tpMax.v = max(tpMax.v, CLOSE);
      }

      // store the scaling factor away
      MMLValueSet (pNode, gpszLocScale, &pMax);
      MMLValueSet (pNode, gpszTextScale, &tpMax);

      // determine the inverse as well as the round off
      CPoint pScale;
      TEXTUREPOINT tpScale;
      fp fRound = 1.0 / 32767.0 / 2.0; // so values go from -32767 to 32767
      for (i = 0; i < 3; i++)
         pScale.p[i] = 32767.0 / pMax.p[i];
      tpScale.h = 32767.0 / tpMax.h;
      tpScale.v = 32767.0 / tpMax.v;

      // write all the binary information in one chunk
      CMem mem;
      if (!mem.Required (sizeof(POLYMESHBIN))) {
         delete pNode;
         return FALSE;
      }
      mem.m_dwCurPosn = sizeof(POLYMESHBIN);
      PPOLYMESHBIN pbm = (PPOLYMESHBIN)mem.p;
      pbm->wNumVert = (WORD)m_lVert.Num();
      pbm->wNumSide = (WORD)m_lSide.Num();

      // all the verticies
      for (i = 0; i < m_lVert.Num(); i++)
         ppv[i]->MMLToBinary (&mem, &pScale, &tpScale, fRound, m_dwSymmetry);

      // all the sides
      for (i = 0; i < m_lSide.Num(); i++)
         pps[i]->MMLToBinary (&mem);

      // NOTE: Dont do any compression since seem to make longer...
      //CMem memRLE;
      //memRLE.m_dwCurPosn = 0;
      //BOOL fCompress = MMLCompressMaxGet();
      //MMLCompressMaxSet (TRUE);
      //PatternEncode ((PBYTE) mem.p, mem.m_dwCurPosn / sizeof(WORD), sizeof(WORD), &memRLE, NULL);
      //MMLCompressMaxSet (fCompress);
      //MMLValueSet (pNode, gpszBinary, (PBYTE) memRLE.p, memRLE.m_dwCurPosn);
      MMLValueSet (pNode, gpszBinary, (PBYTE) mem.p, mem.m_dwCurPosn);
   }
   else {
      // too large, so write out old fashioned way
      // NOTE: Not writing out edges since can be recalced
      for (i = 0; i < m_lVert.Num(); i++) {
         PCMMLNode2 pSub = ppv[i]->MMLTo ();
         if (!pSub)
            continue;
         pNode->ContentAdd (pSub);
      }
      for (i = 0; i < m_lSide.Num(); i++) {
         PCMMLNode2 pSub = pps[i]->MMLTo ();
         if (!pSub)
            continue;
         pNode->ContentAdd (pSub);
      }
   }

   // morphs
   PCOEAttrib *pap;
   pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   for (i = 0; i < m_lPCOEAttrib.Num(); i++) {
      pap[i]->m_fDefValue = pap[i]->m_fCurValue;   // so writes it out
      PCMMLNode2 pSub = pap[i]->MMLTo ();
      if (pSub)
         pNode->ContentAdd (pSub);
      // NOTE: Dont need to do name set because already have one set by MMLTo()
   }

   // surface in meters
   DWORD *padw;
   padw = (DWORD*) m_lSurfaceNotInMeters.Get(0);
   for (i = 0; i < m_lSurfaceNotInMeters.Num(); i++) {
      WCHAR szTemp[64];
      swprintf (szTemp, L"SurfInMet%d", (int)i);
      MMLValueSet (pNode, szTemp, (int) padw[i]);
   }

   // bones
   MMLValueSet (pNode, gpszBoneGUID, (PBYTE) &m_gBone, sizeof(m_gBone));
   PPMBONEINFO ppb;
   ppb = (PPMBONEINFO) m_lPMBONEINFO.Get(0);
   for (i = 0; i < m_lPMBONEINFO.Num(); i++) {
      PCMMLNode2 pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszBone);
      MMLValueSet (pSub, gpszName, ppb[i].szName); // BUGFIX - Was pNode

      DWORD j;
      WCHAR szTemp[64];
      for (j = 0; j < NUMBONEMORPH; j++) {
         if (!ppb[i].aBoneMorph[j].szName[0])
            continue;

         swprintf (szTemp, L"MorphName%d", (int)j);
         MMLValueSet (pSub, szTemp, ppb[i].aBoneMorph[j].szName);

         swprintf (szTemp, L"Dim%d", (int)j);
         MMLValueSet (pSub, szTemp, (int) ppb[i].aBoneMorph[j].dwDim);

         swprintf (szTemp, L"BoneAngle%d", (int)j);
         MMLValueSet (pSub, szTemp, &ppb[i].aBoneMorph[j].tpBoneAngle);

         swprintf (szTemp, L"MorphValue%d", (int)j);
         MMLValueSet (pSub, szTemp, &ppb[i].aBoneMorph[j].tpMorphValue);
      } // j
   }

   return pNode;
}

/**************************************************************************************
CPolyMesh::MMLFrom - Standard MMLFrom
*/
BOOL CPolyMesh::MMLFrom (PCMMLNode2 pNode)
{
   // wipe out
   m_dwSubdivide = 2;
   Clear();
   m_fDirtyRender = TRUE;
   m_fDirtyMorphCOM = TRUE;
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtySymmetry = TRUE;

   // read in
   // data
   m_fCanBackface = (BOOL) MMLValueGetInt (pNode, gpszCanBackface, (int)0);
   m_dwSymmetry = (DWORD) MMLValueGetInt (pNode, gpszSymmetry, (int)0);
   m_dwSubdivide = (DWORD) MMLValueGetInt (pNode, gpszSubdivide, (int)0);

   // surface in meters
   DWORD i;
   for (i = 0; ; i++) {
      WCHAR szTemp[64];
      DWORD dw;
      swprintf (szTemp, L"SurfInMet%d", (int)i);
      dw = (DWORD) MMLValueGetInt (pNode, szTemp, -1);
      if (dw == -1)
         break;
      m_lSurfaceNotInMeters.Add (&dw);
   } // i
   qsort (m_lSurfaceNotInMeters.Get(0), m_lSurfaceNotInMeters.Num(), sizeof(DWORD), BDWORDCompare);

   MMLValueGetBinary (pNode, gpszBoneGUID, (PBYTE) &m_gBone, sizeof(m_gBone));

   // get the scaling factor away
   CPoint pMax;
   TEXTUREPOINT tpMax;
   pMax.Zero();
   tpMax.h = tpMax.v = 0;
   MMLValueGetPoint (pNode, gpszLocScale, &pMax);
   MMLValueGetTEXTUREPOINT (pNode, gpszTextScale, &tpMax);
   pMax.Scale (1.0 / 32767.0);   // so turn into a scale
   tpMax.h /= 32767.0;
   tpMax.v /= 32767.0;
   
   // see if have binary, and load that
   CMem mem;
   MMLValueGetBinary (pNode, gpszBinary, &mem);
   if (mem.m_dwCurPosn) {
      // NOTE: Not doing any decompression since only seems to make larger
      DWORD dwUsed;
      //CMem mem;
      //PatternDecode ((PBYTE) memRLE.p, memRLE.m_dwCurPosn, sizeof(WORD), &mem, &dwUsed);
      //if (mem.m_dwCurPosn < sizeof(PPOLYMESHBIN))
      //   return FALSE;

      PPOLYMESHBIN pmb = (PPOLYMESHBIN) mem.p;
      DWORD dwLeft =(DWORD) mem.m_dwCurPosn - sizeof(POLYMESHBIN);   // BUGFIX - Was unintentionally sizeof(PPOLYSMESHBIN)
      PBYTE pab = (PBYTE)(pmb+1);

      // get all the vertecies
      for (i = 0; i < pmb->wNumVert; i++) {
         PCPMVert pv = new CPMVert;
         if (!pv)
            return FALSE;

         dwUsed = pv->MMLFromBinary (pab, dwLeft, &pMax, &tpMax);
         if (!dwUsed) {
            delete pv;
            return FALSE;
         }
         m_lVert.Add (&pv);

         dwLeft -= dwUsed;
         pab += dwUsed;
      } // i

      // get all the sides
      for (i = 0; i < pmb->wNumSide; i++) {
         PCPMSide pv = new CPMSide;
         if (!pv)
            return FALSE;

         dwUsed = pv->MMLFromBinary (pab, dwLeft);
         if (!dwUsed) {
            delete pv;
            return FALSE;
         }
         m_lSide.Add (&pv);

         dwLeft -= dwUsed;
         pab += dwUsed;
      } // i
   } // if RLE


   PWSTR psz;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz)
         continue;
      if (!_wcsicmp(psz, gpszVert)) {
         PCPMVert pv = new CPMVert;
         if (!pv)
            return FALSE;
         if (!pv->MMLFrom (pSub)) {
            delete pv;
            return FALSE;
         }
         m_lVert.Add (&pv);
      }
      else if (!_wcsicmp(psz, gpszSide)) {
         PCPMSide pv = new CPMSide;
         if (!pv)
            return FALSE;
         if (!pv->MMLFrom (pSub)) {
            delete pv;
            return FALSE;
         }
         m_lSide.Add (&pv);
      }
      else if (!wcscmp(psz, Attrib())) {
         PCOEAttrib pa = new COEAttrib;
         if (!pa)
            continue;
         if (!pa->MMLFrom (pSub)) {
            delete pa;
            continue;
         }
         m_lPCOEAttrib.Add (&pa);
      }
      else if (!_wcsicmp(psz, gpszBone)) {
         PMBONEINFO bi;
         PWSTR psz;
         memset (&bi, 0, sizeof(bi));
         psz = MMLValueGet (pSub, gpszName);
         if (psz)
            wcscpy (bi.szName, psz);

         DWORD j;
         WCHAR szTemp[64];
         for (j = 0; j < NUMBONEMORPH; j++) {
            swprintf (szTemp, L"MorphName%d", (int)j);
            psz = MMLValueGet (pSub, szTemp);
            if (!psz || !psz[0])
               continue;   // no morph
            
            wcscpy (bi.aBoneMorph[j].szName, psz);

            swprintf (szTemp, L"Dim%d", (int)j);
            bi.aBoneMorph[j].dwDim = (DWORD) MMLValueGetInt (pSub, szTemp, 0);

            swprintf (szTemp, L"BoneAngle%d", (int)j);
            MMLValueGetTEXTUREPOINT (pSub, szTemp, &bi.aBoneMorph[j].tpBoneAngle);

            swprintf (szTemp, L"MorphValue%d", (int)j);
            MMLValueGetTEXTUREPOINT (pSub, szTemp, &bi.aBoneMorph[j].tpMorphValue);
         } // j

         m_lPMBONEINFO.Add (&bi);
      } // if bone morph
   }
   // Sort m_lPCOEAttrib list
   qsort (m_lPCOEAttrib.Get(0), m_lPCOEAttrib.Num(), sizeof(PCOEAttrib), OECALCompare);
   m_fDirtyMorphCOM = TRUE;

   // need to keep track of which points link to which sides
   VertSideLinkRebuild();

   // so know which morphs affected
   CalcMorphRemap ();

   return TRUE;
}

/**************************************************************************************
CPolyMesh::VertSideTextClean - Loops through all the vertices, and makes sure that
the side textures are clean by calling SideTextClean(). NOTE: This should only
be called if the sides list in the vertices is valid.
*/
void CPolyMesh::VertSideTextClean (void)
{
   // clear out all the verex side numbers
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   DWORD i;
   for (i = 0; i < m_lVert.Num(); i++)
      ppv[i]->SideTextClean();
}

/**************************************************************************************
CPolyMesh::VertSideLinkRebuild - Vertices contain links to what sides they're part of.
This rebuilds that list.
*/
void CPolyMesh::VertSideLinkRebuild (void)
{
   // clear out all the verex side numbers
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   DWORD i;
   for (i = 0; i < m_lVert.Num(); i++)
      ppv[i]->SideClear();

   PCPMSide *pss;
   PCPMVert pv;
   DWORD *padwVert;
   WORD j;
   pss = (PCPMSide*) m_lSide.Get(0);
   for (i = 0; i < m_lSide.Num(); i++) {
      padwVert = pss[i]->VertGet();
      for (j = 0; j < pss[i]->m_wNumVert; j++) {
         pv = VertGet(padwVert[j]);
         if (pv)
            pv->SideAdd (&i);
      } // j
   } // i

   // clear the sidetext entries
   VertSideTextClean();
}

/**************************************************************************************
CPolyMesh::SymmetrySet - Sets the current editing symmetry mode for the object.

inputs
   DWORD    dwSymmetry - Bifield. 0x01 = x symmetry, 0x02 = y, 0x04 = z
returns
   none
*/
void CPolyMesh::SymmetrySet (DWORD dwSymmetry)
{
   if (m_dwSymmetry == dwSymmetry)
      return;  // no change
   m_dwSymmetry = dwSymmetry;

   m_fDirtySymmetry = TRUE;
   SymmetryRecalc ();
}

/**************************************************************************************
CPolyMesh::SymmetryGet - Returns the current symmetry flags, as passed into SymmetrySet()
*/
DWORD CPolyMesh::SymmetryGet (void)
{
   return m_dwSymmetry;
}

/**************************************************************************************
CPolyMesh::SubdivideSet - Sets the subdivision level.

inputs
   DWORD       dwSubDivide - use -1 for faceted, 0 for no subdivide but smooth, 1 for one pass, etc.
returns
   none
*/
void CPolyMesh::SubdivideSet (DWORD dwSubDivide)
{
   // if no change then do nothing
   if (m_dwSubdivide == dwSubDivide)
      return;

   m_dwSubdivide = dwSubDivide;
   m_fDirtyRender = TRUE;
   m_fDirtyNorm = TRUE;
}

/**************************************************************************************
CPolyMesh::SubdivideGet - Returns the current subdivision value
*/
DWORD CPolyMesh::SubdivideGet (void)
{
   return m_dwSubdivide;
}

/**************************************************************************************
CPolyMesh::SymmetryRecalc - Fills in the symmetry settings for the vertices and
sides based on the new m_dwSymmetry value. This figures out which bits are symmetrical
and sets flags indicating so
*/

#ifdef _DEBUG
#define SHOWSYMMETRYRECALC  // BUGBUG
#endif

void CPolyMesh::SymmetryRecalc (void)
{
   // if symmetry not dirty then dont bother
   if (!m_fDirtySymmetry)
      return;
   m_fDirtySymmetry = FALSE;

#ifdef SHOWSYMMETRYRECALC
   WCHAR szTemp[128];
#endif

   // recalc the normals
   CalcNorm ();

   // create a list of the centers of all the polygons
   CListFixed lCenter;
   lCenter.Init (sizeof(CPoint));
   DWORD i;
   DWORD j;
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   PCPMSide ps;
   PCPMVert pv;
   DWORD *padw;
   PCPoint papSideCenter;
   CPoint pSum;
   lCenter.Required (m_lSide.Num());
   for (i = 0; i < m_lSide.Num(); i++) {
      ps = pps[i];
      pSum.Zero();
      padw = ps->VertGet();
      for (j = 0; j < ps->m_wNumVert; j++) {
         pv = ppv[padw[j]];
         pSum.Add (&pv->m_pLoc);
      } // j
      pSum.Scale (1.0 / (fp)ps->m_wNumVert);
      lCenter.Add (&pSum);

#ifdef SHOWSYMMETRYRECALC
      swprintf (szTemp, L"\r\nSide %d = Loc = (%g,%g,%g), norm= (%g, %g, %g)",
         (int)i, (double)pSum.p[0], (double)pSum.p[1], (double)pSum.p[2],
         (double)ps->m_pNorm.p[0], (double)ps->m_pNorm.p[1], (double)ps->m_pNorm.p[2]);
      OutputDebugStringW (szTemp);
#endif

      // while at it clear existing mirrors
      ps->MirrorClear();
   } // i
   papSideCenter = (PCPoint) lCenter.Get(0);

#ifdef SHOWSYMMETRYRECALC
   fp fDistance = 0.001;
   for (i = 0; i < lCenter.Num(); i++) {
      fp fBest = 1000000;
      DWORD dwBest = (DWORD)-1;
      CPoint pTemp, pDist;
      pTemp.Copy (&papSideCenter[i]);
      pTemp.p[0] *= -1; // XY mirror
      for (j = 0; j < lCenter.Num(); j++) {
         pDist.Subtract (&pTemp, &papSideCenter[j]);
         fDistance = pDist.Length();
         if (fDistance < fBest) {
            fBest = fDistance;
            dwBest = j;
         }
      } // j
      swprintf (szTemp, L"\r\nSide %d XY mirror to side %d, dist=%g",
         (int)i, (int)dwBest, (double)fBest);
      OutputDebugStringW (szTemp);

      if ((fBest > 3e-5) && (dwBest != (DWORD)-1)) {
         swprintf (szTemp, L"\r\n\tSide %d: ", (int)i);
         OutputDebugStringW (szTemp);
         ps = pps[i];
         padw = ps->VertGet();
         for (j = 0; j < ps->m_wNumVert; j++) {
            pv = ppv[padw[j]];
            swprintf (szTemp, L"(%g,%g,%g) ", (double)pv->m_pLoc.p[0], (double)pv->m_pLoc.p[1], (double)pv->m_pLoc.p[2]);
            OutputDebugStringW (szTemp);
         } // j

         swprintf (szTemp, L"\r\n\tSide %d: ", (int)dwBest);
         OutputDebugStringW (szTemp);
         ps = pps[dwBest];
         padw = ps->VertGet();
         for (j = 0; j < ps->m_wNumVert; j++) {
            pv = ppv[padw[j]];
            swprintf (szTemp, L"(%g,%g,%g) ", (double)pv->m_pLoc.p[0], (double)pv->m_pLoc.p[1], (double)pv->m_pLoc.p[2]);
            OutputDebugStringW (szTemp);
         } // j

      }
   } // i
#endif

   // loop through all the vertices and clear their mirrors
   for (i = 0; i < m_lVert.Num(); i++)
      ppv[i]->MirrorClear();

   // edges will need to be recalced
   m_fDirtyEdge = TRUE;

   // if not supposed to be any mirroring then done now
   if (!m_dwSymmetry)
      return;

   // loop through all the vertices and try to figure out which ones are mirrors
   // of each other
   CListFixed lIndex, lMirror, lSides, lSidesMirror, lSidesTest, lPMMIRROR;
   lIndex.Init (sizeof(DWORD));
   lMirror.Init (sizeof(DWORD));
   lSides.Init (sizeof(CPoint));
   lSidesMirror.Init (sizeof(CPoint));
   lSidesTest.Init (sizeof(CPoint));
   lPMMIRROR.Init (sizeof(PMMIRROR));
   for (i = 0; i < m_lVert.Num(); i++) {
      // if already has mirrors then already accounted for this so skip
      pv = ppv[i];
      if (pv->m_wNumMirror)
         continue;

      // figure out on what axis should look for mirrors
      DWORD dwLook;
      dwLook = m_dwSymmetry;
      // BUGFIX - Taking this out because if disconnect two triangles that are symmetrical
      // and touch at 0,y,z then this symmetry check will break and it wont find a
      // point symmetryical to itself
      //if (pv->m_pLoc.p[0] == 0)
      //   dwLook &= (~0x01);
      //if (pv->m_pLoc.p[1] == 0)
      //   dwLook &= (~0x02);
      //if (pv->m_pLoc.p[2] == 0)
      //   dwLook &= (~0x04);
      //if (!dwLook)
      //   continue;   // wont be any mirrors

      // look through all the sides and keep track of their center points
      lSides.Clear();
      padw = pv->SideGet();
      lSides.Required ((DWORD)pv->m_wNumSide);
      for (j = 0; j < (DWORD)pv->m_wNumSide; j++) {
         lSides.Add (&papSideCenter[padw[j]]);
      } // j

      // keep track of all the mirror sets that find
      DWORD x,y,z,xMax,yMax,zMax;
      lIndex.Clear();
      lMirror.Clear();
      lIndex.Add (&i);
      x = 0;
      lMirror.Add (&x);

      xMax = (dwLook & 0x01) ? 2 : 1;
      yMax = (dwLook & 0x02) ? 2 : 1;
      zMax = (dwLook & 0x04) ? 2 : 1;
      PCPMVert pvj;
      CPoint pWant, pWantNorm, pMirror;
      PCPoint papSidesMirror;
      DWORD k, q;
      for (x = 0; x < xMax; x++) for (y = 0; y < yMax; y++) for (z = 0; z < zMax; z++) {
         // dont do self
         if (!x && !y && !z)
            continue;

         //what looking for
         pMirror.Zero();
         pMirror.p[0] = x ? -1 : 1;
         pMirror.p[1] = y ? -1 : 1;
         pMirror.p[2] = z ? -1 : 1;

         pWant.Copy (&pv->m_pLoc);
         pWantNorm.Copy (&pv->m_pNorm);
         lSidesMirror.Init (sizeof(CPoint), lSides.Get(0), lSides.Num());
         papSidesMirror = (PCPoint) lSidesMirror.Get(0);
         for (j = 0; j < 3; j++) {
            pWant.p[j] *= pMirror.p[j];
            pWantNorm.p[j] *= pMirror.p[j];
            for (k = 0; k < lSidesMirror.Num(); k++)
               papSidesMirror[k].p[j] *= pMirror.p[j];
         }

         // BUGFIX - Make sure not to mirror the same point over again
         DWORD *padwCurMirror = (DWORD*) lIndex.Get(0);

#if 0 // def _DEBUG
         int ai[3];
         for (j = 0; j < 3; j++)
            ai[j] = floor(pv->m_pLoc.p[j] * 1000.0 + 0.5);
         if ((ai[0] == -46) && (ai[1] == -65) && (ai[2] == 8))
            ai[0] = -10111;
#endif // _DEBUG

         // find all the points which could possibly be mirrors
         for (j = i+1; j < m_lVert.Num(); j++) {
            pvj = ppv[j];

#if 0 //def _DEBUG
            if (ai[0] == -10111) {
               char szTemp[64];
               CPoint pDist;
               pDist.Subtract (&pWant, &pvj->m_pLoc);
               sprintf (szTemp, "Dist=%g\r\n", (double)pDist.Length());
               OutputDebugString (szTemp);

               if (pDist.Length() < 0.003)
                  ai[1] = 101;
            }
#endif

            if (pvj->m_wNumMirror)
               continue;   // already mirrored elsewhere so ignore
            // BUGFIX - Make sure doesn't mirror something already on the list
            for (k = 0; k < lIndex.Num(); k++)
               if (padwCurMirror[k] == j)
                  break;
            if (k < lIndex.Num())
               continue;

            // both the desired point and the normal must be close
            // BUGFIX - Allow more freedom in close test
#define POINTCLOSE      (0.0005)    // slightly larger than close
            if ((fabs(pWant.p[0] - pvj->m_pLoc.p[0]) > POINTCLOSE) || (fabs(pWant.p[1] - pvj->m_pLoc.p[1]) > POINTCLOSE) ||
               (fabs(pWant.p[2] - pvj->m_pLoc.p[2]) > POINTCLOSE))
            // if (!pWant.AreClose (&pvj->m_pLoc))
               continue;

#ifdef SHOWSYMMETRYRECALC
            swprintf (szTemp, L"\r\nVert %d - Might be mirror to vert %d",
               (int)i, (int)j);
            OutputDebugStringW (szTemp);
            if ((i == 152) && (j == 153))
               j = 153;
            if ((i == 153) && (j == 154))
               i = 153;
            if ((i == 153) && (j == 160))
               i = 153;
            if ((i == 153) && (j == 161))
               i = 153;
            if ((i == 144) && (j == 145))
               i = 144;
#endif
            // BUGFIX - a lot more error as far as normals
#define NORMCLOSE       (0.1)
            if ((fabs(pWantNorm.p[0] - pvj->m_pNorm.p[0]) > NORMCLOSE) || (fabs(pWantNorm.p[1] - pvj->m_pNorm.p[1]) > NORMCLOSE) ||
               (fabs(pWantNorm.p[2] - pvj->m_pNorm.p[2]) > NORMCLOSE))
               continue;

#ifdef SHOWSYMMETRYRECALC
            swprintf (szTemp, L"\r\nVert %d normal matches with %d",
               (int)i, (int)j);
            OutputDebugStringW (szTemp);
#endif
            // just make sure that 0 is 0
            if ((pv->m_pLoc.p[0] == 0) && (pvj->m_pLoc.p[0] != 0))
               continue;
            if ((pv->m_pLoc.p[1] == 0) && (pvj->m_pLoc.p[1] != 0))
               continue;
            if ((pv->m_pLoc.p[2] == 0) && (pvj->m_pLoc.p[2] != 0))
               continue;

            // make sure that all the sides match up
            if (pvj->m_wNumSide != (WORD) lSidesMirror.Num())
               continue;
            lSidesTest.Clear();
            padw = pvj->SideGet();
            lSidesTest.Required ((DWORD)pvj->m_wNumSide);
            for (k = 0; k < (DWORD)pvj->m_wNumSide; k++) {
               lSidesTest.Add (&papSideCenter[padw[k]]);
            } // j
            PCPoint pTest;
            pTest = (PCPoint)lSidesTest.Get(0);
            for (k = 0; k < lSidesMirror.Num(); k++) {
               for (q = 0; q < lSidesTest.Num(); q++)
                  // BUGFIX - Modify this one too, so more forgiving
                  // if (pTest[q].AreClose (&papSidesMirror[k]))  // BUGFIX - Changed [q] to [k]
                  if ((fabs(pTest[q].p[0] - papSidesMirror[k].p[0]) < POINTCLOSE) && (fabs(pTest[q].p[1] - papSidesMirror[k].p[1]) < POINTCLOSE) &&
                     (fabs(pTest[q].p[2] - papSidesMirror[k].p[2]) < POINTCLOSE))
                     break; // found
               if (q >= lSidesTest.Num())
                  break;   // not ffound - same point but no symmetry
            } // k
            if (k < lSidesMirror.Num())
               continue;   // stopped because sides not found - same point but no symmetry

            // if get to here found a match
            break;
         } // j

         if (j < m_lVert.Num()) {
#ifdef SHOWSYMMETRYRECALC
            swprintf (szTemp, L"\r\nzVert %d mirrors %d",
               (int)i, (int)j);
            OutputDebugStringW (szTemp);
#endif

            // found a mirror
            lIndex.Add (&j);
            q = (x ? 1 : 0) | (y ? 2 : 0) | (z ? 4 : 0);
            lMirror.Add (&q);
         }

      } //xyz

      // when get here have a list of all the mirrors of the point in lIndex and lMirror
      // use this to set all the mirror params
      DWORD *padwIndex, *padwMirror;
      PMMIRROR pmm;
      memset (&pmm, 0, sizeof(pmm));
      padwIndex = (DWORD*) lIndex.Get(0);
      padwMirror = (DWORD*) lMirror.Get(0);
      for (j = 0; j < lIndex.Num(); j++) {
         lPMMIRROR.Clear();
         lPMMIRROR.Required (lPMMIRROR.Num() + lIndex.Num());
         for (k = 0; k < lIndex.Num(); k++) {
            if (j == k)
               continue;   // skip mirror with self
            pmm.dwObject = padwIndex[k];
            pmm.dwType = padwMirror[j] ^ padwMirror[k];
            lPMMIRROR.Add (&pmm);
         } // k
         if (lPMMIRROR.Num())
            ppv[padwIndex[j]]->MirrorAdd ((PMMIRROR*)lPMMIRROR.Get(0), lPMMIRROR.Num());
      } // j
   } // i

   // loop through all the sides looking for a mirror. Do some quick check to make sure
   // the points and normals are ok. If that, then do full point compare
   for (i = 0; i < m_lSide.Num(); i++) {
      ps = pps[i];
      if (ps->m_wNumMirror)
         continue;   // already mirrored

      // keep track of all the mirror sets that find
      DWORD x,y,z,xMax,yMax,zMax;
      lIndex.Clear();
      lMirror.Clear();
      lIndex.Add (&i);
      x = 0;
      lMirror.Add (&x);

      // look through all the sides and keep track of their center points
      lSides.Clear();
      padw = ps->VertGet();
      lSides.Required (lSides.Num() + (DWORD)ps->m_wNumVert);
      for (j = 0; j < (DWORD)ps->m_wNumVert; j++) {
         lSides.Add (&ppv[padw[j]]->m_pLoc);
      } // j


      xMax = (m_dwSymmetry & 0x01) ? 2 : 1;
      yMax = (m_dwSymmetry & 0x02) ? 2 : 1;
      zMax = (m_dwSymmetry & 0x04) ? 2 : 1;
      CPoint pWant, pWantNorm, pMirror;
      PCPoint papSidesMirror;
      DWORD k, q;
      for (x = 0; x < xMax; x++) for (y = 0; y < yMax; y++) for (z = 0; z < zMax; z++) {
         // dont do self
         if (!x && !y && !z)
            continue;

         //what looking for
         pMirror.Zero();
         pMirror.p[0] = x ? -1 : 1;
         pMirror.p[1] = y ? -1 : 1;
         pMirror.p[2] = z ? -1 : 1;

         pWant.Copy (&papSideCenter[i]);
         pWantNorm.Copy (&ps->m_pNorm);
         lSidesMirror.Init (sizeof(CPoint), lSides.Get(0), lSides.Num());
         papSidesMirror = (PCPoint) lSidesMirror.Get(0);
         for (j = 0; j < 3; j++) {
            pWant.p[j] *= pMirror.p[j];
            pWantNorm.p[j] *= pMirror.p[j];
            for (k = 0; k < lSidesMirror.Num(); k++)
               papSidesMirror[k].p[j] *= pMirror.p[j];
         }

         // consider flipping the mirror
         DWORD dwFlip;
         dwFlip = x + y + z;
         if (dwFlip % 2) {
            DWORD dwNum = lSidesMirror.Num();
            CPoint p;
            for (j = 0; j < dwNum/2; j++) {
               p.Copy (&papSidesMirror[j]);
               papSidesMirror[j].Copy (&papSidesMirror[dwNum-j-1]);
               papSidesMirror[dwNum-j-1].Copy (&p);
            } // j
         } // flip

         // loop through all the other vertices
         PCPMSide psj;
         for (j = i+1; j < m_lSide.Num(); j++) {
            psj = pps[j];
            if (psj->m_wNumMirror)
               continue;   // already mirrored elsewhere so ignore

            // if number of points different ignore
            if (psj->m_wNumVert != ps->m_wNumVert)
               continue;

            // both the desired point and the normal must be close
            if (!pWant.AreClose (&papSideCenter[j]) || !pWantNorm.AreClose (&psj->m_pNorm))
               continue;

            // get the points for all the vertices
            lSidesTest.Clear();
            padw = psj->VertGet();
            lSidesTest.Required ((DWORD)psj->m_wNumVert);
            for (k = 0; k < (DWORD)psj->m_wNumVert; k++) {
               lSidesTest.Add (&ppv[padw[k]]->m_pLoc);
            } // j
            PCPoint pTest;
            pTest = (PCPoint)lSidesTest.Get(0);

            // loop through all the possible permutations
            for (k = 0; k <ps->m_wNumVert; k++) {
               for (q = 0; q < ps->m_wNumVert; q++) {
                  if (!pTest[(k+q)%ps->m_wNumVert].AreClose (&papSidesMirror[q]))
                     break;
               } // q
               if (q >= ps->m_wNumVert)
                  break;   // found a complete match
            } // k
            if (k >= ps->m_wNumVert)
               continue;   // no match at all

            // else found a match
            break;
         } // j

         if (j < m_lSide.Num()) {
            // found a mirror
            lIndex.Add (&j);
            q = (x ? 1 : 0) | (y ? 2 : 0) | (z ? 4 : 0);
            lMirror.Add (&q);
         }
      } // xyz

      // when get here have a list of all the mirrors of the point in lIndex and lMirror
      // use this to set all the mirror params
      DWORD *padwIndex, *padwMirror;
      PMMIRROR pmm;
      memset (&pmm, 0, sizeof(pmm));
      padwIndex = (DWORD*) lIndex.Get(0);
      padwMirror = (DWORD*) lMirror.Get(0);
      for (j = 0; j < lIndex.Num(); j++) {
         lPMMIRROR.Clear();
         lPMMIRROR.Required (lIndex.Num());
         for (k = 0; k < lIndex.Num(); k++) {
            if (j == k)
               continue;   // skip mirror with self
            pmm.dwObject = padwIndex[k];
            pmm.dwType = padwMirror[j] ^ padwMirror[k];
            lPMMIRROR.Add (&pmm);
         } // k
         if (lPMMIRROR.Num())
            pps[padwIndex[j]]->MirrorAdd ((PMMIRROR*)lPMMIRROR.Get(0), lPMMIRROR.Num());
      } // j
   } // i
   
   // when get here done
   return;
}

/**************************************************************************************
CPolyMesh::CalcEdges - Recalculates the edges (if m_fDIrtyEdge is TRUE), deriving
the data from the vertices and sides

NOTE: Becase EdgeAdd() doesn't keep the mirror information for edges up to date,
neither will CalcEdges()
*/
void CPolyMesh::CalcEdges (void)
{
   if (!m_fDirtyEdge)
      return;

   // clear all the edge information out
   DWORD i;
   PCPMEdge *ppe = (PCPMEdge*) m_lEdge.Get(0);
   for (i = 0; i < m_lEdge.Num(); i++)
      delete ppe[i];
   m_lEdge.Clear();

   // clear edge information out of the vertices
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < m_lVert.Num(); i++)
      ppv[i]->EdgeClear();

   // create a list of all edges by going through all the polygons
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   WORD j;
   for (i = 0; i < m_lSide.Num(); i++) {
      PCPMSide ps = pps[i];
      DWORD *padwVert = ps->VertGet();
      for (j = 0; j < ps->m_wNumVert; j++)
         EdgeAdd (padwVert[j], padwVert[(j+1)%ps->m_wNumVert], i);
   } // i

   // go through all the edges and link them to th epolygons
   PCPMVert pv;
   ppe = (PCPMEdge*) m_lEdge.Get(0);
   for (i = 0; i < m_lEdge.Num(); i++) {
      for (j = 0; j < 2; j++) {
         pv = VertGet (ppe[i]->m_adwVert[j]);
         pv->EdgeAdd (&i);
      } // j
   } // i

   m_fDirtyEdge = FALSE;
}

/**************************************************************************************
CPolyMesh::CalcNorm - Recalculates the normals (if m_fDirtyNorm is TRUE),
calculating them for all the points
*/
void CPolyMesh::CalcNorm (void)
{
   if (!m_fDirtyNorm)
      return;

   // recalculate normal for all sides
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   DWORD i;
   for (i = 0; i < m_lSide.Num(); i++)
      pps[i]->CalcNorm(this);

   // Recalc normals for points
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < m_lVert.Num(); i++)
      ppv[i]->CalcNorm(this);

   m_fDirtyNorm = FALSE;
}

/**************************************************************************************
CPolyMesh::EdgeFind - Given the start and end vertices, this finds the edge.
You MUST have called CalcEdges() before this will work. It does this by
finding one of the points, and knowing that the point will point to one of the
right edges, it does a fast comparison

inputs
   DWORD       dwVert1, dwVert2 - Two vertices
returns
   DWORD - Edge index, or -1 if cant find
*/
DWORD CPolyMesh::EdgeFind (DWORD dwVert1, DWORD dwVert2)
{
   // swap edges so sorted properly
   if (dwVert1 > dwVert2) {
      DWORD dw = dwVert1;
      dwVert1 = dwVert2;
      dwVert2 = dw;
   }

   // get the Vert
   PCPMVert pv = VertGet (dwVert1);
   if (!pv)
      pv = VertGet (dwVert2);
   if (!pv)
      return -1;

   // get the edge indecies
   DWORD *padwEdge = pv->EdgeGet ();
   PCPMEdge pe;
   WORD j;
   PCPMEdge *ppe = (PCPMEdge*) m_lEdge.Get(0);
   for (j = 0; j < pv->m_wNumEdge; j++) {
      pe = ppe[padwEdge[j]];
      if ((pe->m_adwVert[0] == dwVert1) && (pe->m_adwVert[1] == dwVert2))
         return padwEdge[j];
   }

   // else cant find
   return -1;
}


/**************************************************************************************
CPolyMesh::Subdivide - Subdivides the current polymesh. They polygon mesh is modified
in place.

NOTE: Calling subdivide will much up internal symmetry pointers in the process. If you
wish to reconstitute the symmetry information when it's done then you MUST call
SymmetryRecalc() yourself. This isn't done because most of the time subdivide is just
used to produce a set of triangles to draw, so it isn't necessary to recalc.

inputs
   BOOL        fKeepDeform - If TRUE, keep the deformation information when subdiving
               else discard it since wont need it.
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::Subdivide (BOOL fKeepDeform)
{
   // need edges
   CalcEdges ();

   // remember the original number of points, edges, and sides
   DWORD dwOrigVert, dwOrigEdge, dwOrigSide;
   dwOrigVert = m_lVert.Num();
   dwOrigEdge = m_lEdge.Num();
   dwOrigSide = m_lSide.Num();

   // create one point per side, which is in the center of each side
   DWORD i;
   WORD j;
   DWORD *padw;
   DWORD dwCenterVert;
   CListFixed lScratch;
   dwCenterVert = m_lVert.Num(); // where the center vertecies start
   lScratch.Init (sizeof(PCPMVert));
   for (i = 0; i < dwOrigSide; i++) {
      // get the side
      PCPMSide ps = SideGet (i);
      if (!ps)
         return FALSE;

      // make a list so can average togther
      lScratch.Clear();
      padw = ps->VertGet();
      lScratch.Required (ps->m_wNumVert);
      for (j = 0; j < ps->m_wNumVert; j++) {
         PCPMVert pv = VertGet(padw[j]);
         lScratch.Add (&pv);
      } // j

      PCPMVert pNew;
      pNew = new CPMVert;
      if (!pNew)
         return FALSE;
      if (!pNew->Average ((PCPMVert*) lScratch.Get(0), NULL, lScratch.Num(), i, fKeepDeform)) {
         delete pNew;
         return FALSE;
      }

      // add this to the points
      m_lVert.Add (&pNew);
   } // i

   // loop through all the original vertices and use m_pNorm as a scratch
   // element. Start out with 4 zero's
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < dwOrigVert; i++) {
      ppv[i]->m_pNorm.Zero4();
   }

   // create one vertex per edge
   DWORD dwEdgeVert;
   PCPMVert paVert[2];
   dwEdgeVert = m_lVert.Num();
   for (i = 0; i < m_lEdge.Num(); i++) {
      PCPMEdge pe = *((PCPMEdge*)m_lEdge.Get(i));
      
      for (j = 0; j < 2; j++)
         paVert[j] = VertGet(pe->m_adwVert[j]);

      PCPMVert pNew;
      pNew = new CPMVert;
      if (!pNew)
         return FALSE;
      if (!pNew->Average (paVert, NULL, 2, pe->m_adwSide[0], fKeepDeform)) {
         delete pNew;
         return FALSE;
      }
      // NOTE - Only getting the texture for one of the sides, not the other

      // determine the texture for the other side
      if (pe->m_adwSide[1] != -1) {
         TEXTUREPOINT tp;
         PTEXTUREPOINT atp[2];
         atp[0] = paVert[0]->TextureGet (pe->m_adwSide[1]);
         atp[1] = paVert[1]->TextureGet (pe->m_adwSide[1]);
         tp.h = (atp[0]->h + atp[1]->h) / 2.0;
         tp.v = (atp[0]->v + atp[1]->v) / 2.0;
         pNew->TextureSet (pe->m_adwSide[1], &tp);
      }

      // this point needs to be averaged in with the two sides...
      if (pe->m_adwSide[1] != -1) {
         PCPMVert ps;
         CPoint pAvg;
         ps = VertGet (dwCenterVert + pe->m_adwSide[0]);
         pAvg.Copy (&ps->m_pLoc);
         ps = VertGet (dwCenterVert + pe->m_adwSide[1]);
         pAvg.Average (&ps->m_pLoc);

         // average the midpoint of the two sides with the midpint of the two edges
         pNew->m_pLoc.Average (&pAvg);//, 1.0 / 3.0);
         // BUGFIX - Weight this let so a single cube becomes more spherelike
         // BUGFIX - Unweight this because if put a sphere in and subdivide becomes oddly shapped
         // BUGFIX - When back to 1/3
         // BUGFIX - Should be 1/2 so that matches catmul
      }
      else {
         // BUGFIX - Do this so don't get jaggies if on edge
         PCPMVert ps;
         ps = VertGet (dwCenterVert + pe->m_adwSide[0]);

         // average the midpoint of the two sides with the midpint of the two edges
         pNew->m_pLoc.Average (&ps->m_pLoc, 1.0 / 3.0);
      }

      // while we have the edge open, go through the two vertices and add the
      // cetner of the edge to m_pNorm (scratch) of each of the vertices
      for (j = 0; j < 2; j++) {
         paVert[j]->m_pNorm.Add (&pNew->m_pLoc);
         paVert[j]->m_pNorm.p[3] += 1; // so know how many points added in
      }

      // add this to the points
      m_lVert.Add (&pNew);

   } // i

   // go back through all the original vertices and use the information in m_pNorm
   // to figure out the average of all the edge centers with the vertex point.
   // average this in with the current point
   ppv = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < dwOrigVert; i++) {
      fp f = ppv[i]->m_pNorm.p[3];
      if (!f)
         continue;   // shouldnt happen, but check
      ppv[i]->m_pNorm.Scale (1.0 / f);
      ppv[i]->m_pLoc.Average (&ppv[i]->m_pNorm, 2.0 / f);
      // BUGFIX - Weight this so a cube subdivided becomes more spherelike
      // BUGFIX - Unweight this because if put a sphere in and subdivide becomes oddly shapped
      // BUGFIX - When back to 1/3
      // BUGFIX - Changed to N-2/N for orig point, 2/N for other so matches catmul, was 1.0/sqrt(2)
   }

   // go through all the sides and divide up each into several quads formed
   // by an original vertex, half way through an edge, and the center of the side
   DWORD adwVert[4];
   DWORD k;
   DWORD *padwVert;
   for (i = 0; i < dwOrigSide; i++) {
      PCPMSide ps = SideGet (i);
      if (!ps)
         return FALSE;
      padwVert = ps->VertGet();

      // loop over vertices
      for (j = 0; j < ps->m_wNumVert; j++) {
         PCPMSide pNew = (j+1 < ps->m_wNumVert) ? ps->Clone() : ps;
         if (!pNew)
            return FALSE;

         // figure out the quad...
         adwVert[0] = padwVert[j];  // original vertex
         adwVert[1] = EdgeFind (padwVert[j], padwVert[(j+1)%ps->m_wNumVert]);
         adwVert[2] = dwCenterVert + i;   // center
         adwVert[3] = EdgeFind (padwVert[(j+ps->m_wNumVert-1)%ps->m_wNumVert],
            padwVert[j]);
         if ((adwVert[1] == -1) || (adwVert[3]==-1)) {
            if (pNew != ps)
               delete pNew;
            return FALSE;
         }
         adwVert[1] += dwEdgeVert;
         adwVert[3] += dwEdgeVert;

         // set these new vertices
         pNew->VertClear();
         pNew->VertAdd (adwVert, 4);

         // NOTE: Changing these vertices will completely mess up the edge's notion
         // of what's symmetrical with the edge, but that doesn't matter much
         // because will recalc the symmetry down below

         // need to reestablish which points will be linked to which sides
         DWORD dwRename;
         PCPMVert pv;
         dwRename = (ps == pNew) ? i : m_lSide.Num();
         if (i != dwRename) {
            pv = VertGet (adwVert[0]);
            pv->SideRename (1, &i, &dwRename);
            pv->SideTextRename (1, &i, &dwRename, FALSE);
         }
         for (k = 1; k < 4; k++) {
            pv = VertGet (adwVert[k]);
            pv->SideAdd (&dwRename);

            // duplicate the texture
            PTEXTUREPOINT pt1, pt2;
            pt1 = pv->TextureGet (i);
            pt2 = pv->TextureGet (dwRename);
            if (pt1 != pt2) {
               TEXTUREPOINT tp;
               tp = *pt1;
               pv->TextureSet (dwRename, &tp);
            }
            // NOTE: This will leave around the texture info for the old side
            // occasionally but not such a big deal
         }

         if (pNew != ps) {
            m_lSide.Add (&pNew);

            // get orig side again since just added
            ps = SideGet (i);
         }
         else
            break; // need to have this here or might loop back just after added new point
            // else, just wrote over existing one
      } // j
   } // i

   // set some flags so know need to recalc
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtySymmetry = TRUE;
   m_fDirtyScale = TRUE;


   return TRUE;
}

/**************************************************************************************
CPolyMesh::VertFind - Look through the polygon mesh for the given vertex.

inputs
   PCPoint        pPoint - Point to look for
   PCListFixed    plVert - Can be NULL. If not NULL, must be initialized to
                  sizeof(DWORD) and cleared. Will be filled in with a list of vertices
                  that match. (Since more than one vertex may match.

NOTE: Uses the m_pLoc, so doesnt take deformations into account

returns
   DWORD - Vertex index that matches, or -1 if none do
*/
DWORD CPolyMesh::VertFind (PCPoint pPoint, PCListFixed plVert)
{
   DWORD dwRet = -1;

   // loop through all the vertices
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   DWORD i;
   for (i = 0; i < m_lVert.Num(); i++) {
      if (ppv[i]->m_pLoc.AreClose (pPoint)) {
         dwRet = i;
         if (plVert)
            plVert->Add (&i);
      }
   }

   return dwRet;
}


/**************************************************************************************
CPolyMesh::VertAdd - Adds a vertex. (NOte this doesnt check to see if another vertex
occupies the same space.) In the process of adding this will add mirrors as
specified by the m_dwSymmetry flags. A symmetrical point WON'T be added if the point's
x==0 (no symmetry in x), y == 0 (no symmetry in y), or z == 0 (no symmetry in z).

inputs
   PCPoint        pPoint - Point location to add
returns
   DWORD - Index to the new point. (To get the symmetrical ones you'll need to look
            at the mirrors.) Or -1 if error
*/
DWORD CPolyMesh::VertAdd (PCPoint pPoint)
{
   // keep a list of all the points that add...
   DWORD adwAdd[8], adwBits[8], dwNum;
   dwNum = 0;

   // add them
   DWORD x,y,z, xMax, yMax, zMax;
   xMax = ((m_dwSymmetry & 0x01) && (pPoint->p[0] != 0)) ? 2 : 1;
   yMax = ((m_dwSymmetry & 0x02) && (pPoint->p[1] != 0)) ? 2 : 1;
   zMax = ((m_dwSymmetry & 0x04) && (pPoint->p[2] != 0)) ? 2 : 1;
   m_lVert.Required (m_lVert.Num() + xMax * yMax * zMax);
   for (x = 0; x < xMax; x++) {
      for (y = 0; y < yMax; y++) {
         for (z = 0; z < zMax; z++) {
            PCPMVert pNew = new CPMVert;
            if (!pNew)
               continue;
            pNew->m_pLoc.Copy (pPoint);

            // mirror
            if (x)
               pNew->m_pLoc.p[0] *= -1;
            if (y)
               pNew->m_pLoc.p[1] *= -1;
            if (z)
               pNew->m_pLoc.p[2] *= -1;

            m_lVert.Add (&pNew);
            adwAdd[dwNum] = m_lVert.Num() - 1;
            DWORD dwBits = (x ? 0x01 : 0) | (y ? 0x02 : 0) | (z ? 0x04 : 0);
            adwBits[dwNum] = dwBits;
            dwNum++;
         } // z
      } // y
   } // x

   // go back through and set up the mirror
   PMMIRROR aMirror[7];
   DWORD i, j, dwCur;
   PCPMVert *ppv;
   ppv = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < dwNum; i++) { // i == the one that filling in mirror infomration for
      dwCur = 0;
      for (j = 0; j < dwNum; j++) {
         // dont remember a mirror to oneself
         if (j == i)
            continue;

         aMirror[dwCur].dwObject = adwAdd[j];
         aMirror[dwCur].dwType = adwBits[i] ^ adwBits[j];

         // increment to next mirror site
         dwCur++;
      }

      // add all the mirror info
      if (dwCur)
         ppv[adwAdd[i]]->MirrorAdd (aMirror, (WORD)dwCur);
   }

   return adwAdd[0];
}


/**************************************************************************************
CPolyMesh::VertNum - Returns the number of vertices
*/
DWORD CPolyMesh::VertNum (void)
{
   return m_lVert.Num();
}

/**************************************************************************************
CPolyMesh::SideNum - Returns the number of sudes
*/
DWORD CPolyMesh::SideNum (void)
{
   return m_lSide.Num();
}

/**************************************************************************************
CPolyMesh::VertGet - Given an index into a vertex this returns a pointer to it.

inputs
   DWORD          dwVert - Vertex
returns
   PCPMVert - Pointer to it, or NULL if none
*/
PCPMVert CPolyMesh::VertGet (DWORD dwVert)
{
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(dwVert);
   if (!ppv)
      return NULL;
   return *ppv;
}

/**************************************************************************************
CPolyMesh::EdgeGet - Given an index into a Edgeex this returns a pointer to it.

inputs
   DWORD          dwEdge - Edgeex
returns
   PCPMEdge - Pointer to it, or NULL if none
*/
PCPMEdge CPolyMesh::EdgeGet (DWORD dwEdge)
{
   // make sure calculated
   CalcEdges ();

   PCPMEdge *ppv = (PCPMEdge*) m_lEdge.Get(dwEdge);
   if (!ppv)
      return NULL;
   return *ppv;
}

/**************************************************************************************
CPolyMesh::SideGet - Given an index into a side this returns a pointer to it.

inputs
   DWORD          dwSide - Side index
returns
   PCPMSide - Pointer to it, or NULL if none
*/
PCPMSide CPolyMesh::SideGet (DWORD dwSide)
{
   PCPMSide *ppv = (PCPMSide*) m_lSide.Get(dwSide);
   if (!ppv)
      return NULL;
   return *ppv;
}


/**************************************************************************************
CPolyMesh::SideAdd - Adds a side with mirroring, just like the other SideADd().
   This will either add a triangle (if 3 points specified) or a quad (if 4 points)

inputs
   DWORD       dwSurface - Surface ID to use
   DWORD       dwVert1..4 - Vert indecies. If dwVert4 == -1 then add a triangle
returns
   DWORD - Index to the side added. Mirrors may also be added
*/
DWORD CPolyMesh::SideAdd (DWORD dwSurface, DWORD dwVert1, DWORD dwVert2, DWORD dwVert3, DWORD dwVert4)
{
   DWORD adw[4];
   adw[0] = dwVert1;
   adw[1] = dwVert2;
   adw[2] = dwVert3;
   adw[3] = dwVert4;

   return SideAdd (dwSurface, adw, (dwVert4 == -1) ? 3 : 4);
}

/**************************************************************************************
CPolyMesh::SideCalcMirror - Given an array of vertex indecies that constitute a side,
this determines what vertex indecies would be needed to make a mirror of the side.
NOTE: All the m_lVert points must already contain information about what their mirrors are.

inputs
   DWORD          *padwVert - Pointer to an array of verticies describing the side
   DWORD          dwNum - Number of points
   DWORD          dwMirror - Mirror bits, 0x01 for x, 0x02 for y, 0x04 for z
   PCListFixed    pList - Filled with the vertex indecies that would make the mirror.
                  Will be initialized to sizeof(DWORD)
returns
   BOOL - TRUE if found a mirror, FALSE if not.
*/
BOOL CPolyMesh::SideCalcMirror (DWORD *padwVert, DWORD dwNum, DWORD dwMirror,
                                PCListFixed pList)
{
   // if there isn't a mirror here then trivial
   if (!dwMirror) {
      pList->Init (sizeof(DWORD), padwVert, dwNum);
      return TRUE;
   }

   // clear the list
   pList->Init (sizeof(DWORD));
   pList->Clear();

   // loop through all the points finding their mirror
   DWORD i, dwWantMirror;
   WORD j;
   for (i = 0; i < dwNum; i++) {
      PCPMVert pVert = VertGet (padwVert[i]);
      if (!pVert)
         return FALSE;

      CPoint pMirror;
      pMirror.Copy (&pVert->m_pLoc);
      if (dwMirror & 0x01)
         pMirror.p[0] *= -1;
      if (dwMirror & 0x02)
         pMirror.p[1] *= -1;
      if (dwMirror & 0x04)
         pMirror.p[2] *= -1;

      // BUGFIX - Beause mirroring isnt done if a point is on 0, when test, ignore
      // this case
      dwWantMirror = dwMirror;
      if (pVert->m_pLoc.p[0] == 0)
         dwWantMirror &= ~(0x01);
      if (pVert->m_pLoc.p[1] == 0)
         dwWantMirror &= ~(0x02);
      if (pVert->m_pLoc.p[2] == 0)
         dwWantMirror &= ~(0x04);

      // loop through all the mirror flags
      PPMMIRROR pm;
      pm = pVert->MirrorGet();
      // BUGFIX - Include self in list of potential mirrors
      for (j = 0; j <= pVert->m_wNumMirror; j++) {
         if ((j ? pm[j-1].dwType : 0) != dwWantMirror)
            continue;

         // BUGFIX - Verify that same point
         PCPMVert pv2;
         pv2 = j ? VertGet (pm[j-1].dwObject) : pVert;
         if (pMirror.AreClose (&pv2->m_pLoc))
            break;
         // else not same so continue
      } // j
      if (j == 0)
         pList->Add (&padwVert[i]);  // self
      else if (j <= pVert->m_wNumMirror)
         pList->Add (&pm[j-1].dwObject); // found mirror
      else
         return FALSE;  // BUGFIX - If cant find mirror then error
         // old code pList->Add (&padwVert[i]); // didn't find mirror, so add the same point
   } // i

   // may need to swap the list of points if only mirror once, since points will
   // need to go in opposite direction
   DWORD dw;
   BOOL fSwap;
   for (dw = dwMirror, fSwap = FALSE; dw; dw /= 2)
      if (dw & 0x01)
         fSwap = !fSwap;
   if (fSwap) {
      DWORD *padw = (DWORD*) pList->Get(0);
      dwNum = pList->Num();
      for (i = 0; i < dwNum/2; i++) {
         dw = padw[i];
         padw[i] = padw[dwNum - i - 1];
         padw[dwNum - i - 1] = dw;
      }

      // BUGFIX - Rotate so what was the first point is still the first point
      // otherwise dont get symmetrical sides
      dw = padw[dwNum-1];
      memmove (padw+1, padw, sizeof(DWORD)*(dwNum-1));
      padw[0] = dw;
   }

   return TRUE;
}

/**************************************************************************************
CPolyMesh::SideAreSame - Given two sets of vertex indecies that describe two sides,
this determines if they're effectively the same side. It tests to see if the vertex
numbers, in order but any modulo, match. NOTE: If one side is clockwise and the other
counterclockwise then this WON'T indicate a match.

inputs
   DWORD          *padwVert1 - Pointer to an array of vertices for side1
   DWORD          *padwVert2 - Pointer to an array of vertices for side2
   DWORD          dwNum - Number of points in the sides... they must both be the same to have a chance of matching
returns
   BOOL - TRUE if they match, FALSE if not
*/
BOOL CPolyMesh::SideAreSame (DWORD *padwVert1, DWORD *padwVert2, DWORD dwNum)
{
   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      for (j = 0; j < dwNum; j++) {
         if (padwVert1[j] != padwVert2[(i+j)%dwNum])
            break;
      } // j
      if (j >= dwNum)
         return TRUE;   // found match
   } // i

   // if get here no match
   return FALSE;
}


/**************************************************************************************
CPolyMesh::EdgeAdd - This adds a new edge to the polymesh.
It also adds mirrors if the mirror setting is on.

NOTE: Unlike the other XXXAdd() calls, this looks for duplicates. If a duplicate
already exists then the edge isn't added.

NOTE: EdgeAdd() doesn't actually update the mirrors. Took this out because it
was inherently flawed as coded, and doesn't seem to be doable without an order n-square
algorithm ,which I'd rather not do

inputs
   DWORD       dwVert1, dwVert2 - Two vertices
   DWORD       dwSide - Side that is being added for.
returns
   BOOL - TRUE if addded, FALSE if cant add. To find the mirrors
            look at the mirrors within the edge
*/
BOOL CPolyMesh::EdgeAdd (DWORD dwVert1, DWORD dwVert2, DWORD dwSide)
{
   // when adding the edge make sure there aren't any duplicates. To do this,
   // go through the existing edges and look for a match. It would seem obvious
   // that to make fast do a sort, but if did this then all the other edge references
   // in the system would need to be udpated every time. It's ultimately faster
   // to just do a linear search
   DWORD adwSort[2];
   adwSort[0] = min(dwVert1, dwVert2);
   adwSort[1] = max(dwVert1, dwVert2);
   PCPMEdge *ppe = (PCPMEdge*) m_lEdge.Get(0);
   DWORD dwNum = m_lEdge.Num();
   PCPMEdge *ppMod = NULL;
   if (!dwNum)
      goto addedge;

   // BUGFIX - Made this faster by making into a tree

   PCPMEdge pCur = ppe[0];
   while (pCur) {
      if (adwSort[0] > pCur->m_adwVert[0]) {
         ppMod = &pCur->m_apTree[1];
         pCur = *ppMod;
         continue;
      }
      else if (adwSort[0] < pCur->m_adwVert[0]) {
         ppMod = &pCur->m_apTree[0];
         pCur = *ppMod;
         continue;
      }
      else if (adwSort[1] > pCur->m_adwVert[1]) {
         ppMod = &pCur->m_apTree[1];
         pCur = *ppMod;
         continue;
      }
      else if (adwSort[1] < pCur->m_adwVert[1]) {
         ppMod = &pCur->m_apTree[0];
         pCur = *ppMod;
         continue;
      }

      // else, match
      // set the side, either the 1st one if it's blank, or the 2nd (as long as not
      // already set in the 1st one)
      if (pCur->m_adwSide[0] == -1)
         pCur->m_adwSide[0] = dwSide;
      else if (pCur->m_adwSide[0] != dwSide) {
#ifdef _DEBUG
         if ((pCur->m_adwSide[1] != -1) && (pCur->m_adwSide[1] != dwSide))
            pCur->m_adwSide[1] = -1;   // to test that not more than 2 sides per edge
#endif
         pCur->m_adwSide[1] = dwSide;
      }
      return TRUE;
   } // while pCur



#ifdef MIRROREDGE // Dont store mirror info in edges

   note: this code is inherently busted

   // NOTE: Since its assumed that edges are all added under the same mirror conditions,
   // dont need to check that their mirrors exists, since if the edge exists the mirrors
   // also exist

   // figure out all the mirrors
   CListFixed alMirror[8];
   DWORD adwAdd[8];
   DWORD adwBits[8];
   DWORD dwNumMirror, j;
   DWORD x,y,z, xMax, yMax, zMax;
   xMax = (m_dwSymmetry & 0x01) ? 2 : 1;
   yMax = (m_dwSymmetry & 0x02) ? 2 : 1;
   zMax = (m_dwSymmetry & 0x04) ? 2 : 1;
   dwNumMirror = 0;
   for (x = 0; x < xMax; x++) for (y = 0; y < yMax; y++) for (z = 0; z < zMax; z++) {
      // mirror bits
      DWORD dwBits = (x ? 0x01 : 0) | (y ? 0x02 : 0) | (z ? 0x04 : 0);
      if (!SideCalcMirror(adwSort, 2, dwBits, &alMirror[dwNumMirror]))
         continue;   // error

      // set the lowest vertex tot he beginning
      DWORD *padw1, *padw2;
      padw1 = (DWORD*) alMirror[dwNumMirror].Get(0);
      if (padw1[0] > padw1[1]) {
         DWORD dw;
         dw = padw1[0];
         padw1[0] = padw1[1];
         padw1[1] = dw;
      }

      // make sure that this isn't a match of existing onces
      if (dwNumMirror) {
         for (j = 0; j < dwNumMirror; j++) {
            padw2 = (DWORD*) alMirror[j].Get(0);
            if ((padw1[0] == padw2[0]) && (padw1[1] == padw2[1]))
               break;
         } // j
         if (j < dwNumMirror)
            continue;   // mirror images match
      } // if dwNumMirror

      // if get here is unique, so add
      PCPMEdge pNew;
      pNew = new CPMEdge;
      if (!pNew)
         continue;
      pNew->VertSet (padw1[0], padw1[1]);
      if (dwBits == 0)  // BUGFIX - Only set the side for the first one, otherwise get duplicates
         pNew->m_adwSide[0] = dwSide;

      // add to list
      m_lEdge.Add (&pNew); note: not adding tree
      adwAdd[dwNumMirror] = m_lEdge.Num() - 1;
      adwBits[dwNumMirror] = dwBits;
      dwNumMirror++;
   } // xyz

   // go back through and indicate which are mirrors of the others
   PMMIRROR aMirror[7];
   DWORD dwCur;
   PCPMEdge *ppv;
   ppv = (PCPMEdge*) m_lEdge.Get(0);
   for (i = 0; i < dwNumMirror; i++) { // i == the one that filling in mirror infomration for
      PCPMEdge ps;
      ps = ppv[adwAdd[i]];
      dwCur = 0;
      for (j = 0; j < dwNumMirror; j++) {
         // dont remember a mirror to oneself
         if (j == i)
            continue;

         aMirror[dwCur].dwObject = adwAdd[j];
         aMirror[dwCur].dwType = adwBits[i] ^ adwBits[j];

         // increment to next mirror site
         dwCur++;
      }

      // add all the mirror info
      if (dwCur)
         ps->MirrorAdd (aMirror, (WORD)dwCur);

      // NOTE: Not adding this edge to the points yet, do that later
   }

   return adwAdd[0]; note - wrong return
#else // MIRROREDGE

addedge:
   // if get here is unique, so add
   PCPMEdge pNew;
   pNew = new CPMEdge;
   if (!pNew)
      return -1;
   pNew->VertSet (adwSort[0], adwSort[1]);
   pNew->m_adwSide[0] = dwSide;

   // set tree parent
   if (ppMod)
      *ppMod = pNew;

   // add to list
   m_lEdge.Add (&pNew);
   return TRUE;
#endif // MIRROREDGE
}


/**************************************************************************************
CPolyMesh::SideAdd - This adds a new side to the polymesh using an array of vertex
indecies to create the side. It also adds mirrors if the mirror setting is on.

inputs
   DWORD          dwSurface - Surface number to use
   DWORD          *padwVert - Pointer to an array of vertices
   DWORD          dwNum - Number of vertices
returns
   DWORD - Index to side that was added, or -1 if cant add. To find the mirrors
            look at the mirrors within the side
*/
DWORD CPolyMesh::SideAdd (DWORD dwSurface, DWORD *padwVert, DWORD dwNum)
{
   // set dirty flags
   m_fDirtyNorm = TRUE;
   m_fDirtyEdge = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyMorphCOM = TRUE;
   m_fDirtyScale = TRUE;

   // figure out all the mirrors
   CListFixed alMirror[8];
   DWORD adwAdd[8];
   DWORD adwBits[8];
   DWORD dwNumMirror, j;
   DWORD x,y,z, xMax, yMax, zMax;
   xMax = (m_dwSymmetry & 0x01) ? 2 : 1;
   yMax = (m_dwSymmetry & 0x02) ? 2 : 1;
   zMax = (m_dwSymmetry & 0x04) ? 2 : 1;
   dwNumMirror = 0;
   for (x = 0; x < xMax; x++) for (y = 0; y < yMax; y++) for (z = 0; z < zMax; z++) {
      // mirror bits
      DWORD dwBits = (x ? 0x01 : 0) | (y ? 0x02 : 0) | (z ? 0x04 : 0);
      if (!SideCalcMirror(padwVert, dwNum, dwBits, &alMirror[dwNumMirror]))
         continue;   // error

      // make sure that this isn't a match of existing onces
      DWORD *padw1, *padw2;
      padw1 = (DWORD*) alMirror[dwNumMirror].Get(0);
      if (dwNumMirror) {
         for (j = 0; j < dwNumMirror; j++) {
            padw2 = (DWORD*) alMirror[j].Get(0);
            if (SideAreSame (padw1, padw2, dwNum))
               break;
         } // j
         if (j < dwNumMirror)
            continue;   // mirror images match
      } // if dwNumMirror

      // if get here is unique, so add
      PCPMSide pNew;
      pNew = new CPMSide;
      if (!pNew)
         continue;
      if (!pNew->VertAdd (padw1, dwNum)) {
         delete pNew;
         continue;
      }
      pNew->m_dwSurfaceText = dwSurface;

      // add to list
      m_lSide.Add (&pNew);
      adwAdd[dwNumMirror] = m_lSide.Num() - 1;
      adwBits[dwNumMirror] = dwBits;
      dwNumMirror++;
   } // xyz

   // go back through and indicate which are mirrors of the others
   PMMIRROR aMirror[7];
   DWORD i, dwCur;
   PCPMSide *ppv;
   ppv = (PCPMSide*) m_lSide.Get(0);
   for (i = 0; i < dwNumMirror; i++) { // i == the one that filling in mirror infomration for
      PCPMSide ps;
      ps = ppv[adwAdd[i]];
      dwCur = 0;
      for (j = 0; j < dwNumMirror; j++) {
         // dont remember a mirror to oneself
         if (j == i)
            continue;

         aMirror[dwCur].dwObject = adwAdd[j];
         aMirror[dwCur].dwType = adwBits[i] ^ adwBits[j];

         // increment to next mirror site
         dwCur++;
      }

      // add all the mirror info
      if (dwCur)
         ps->MirrorAdd (aMirror, (WORD)dwCur);

      // while here, go through all the points and indicate that the side is part
      // or the points
      DWORD *padwVert;
      padwVert = ps->VertGet();
      for (j = 0; j < ps->m_wNumVert; j++) {
         PCPMVert pv = VertGet(padwVert[j]);
         if (pv)
            pv->SideAdd (&adwAdd[i]);
      } // j
   }

   return adwAdd[0];
}


/*************************************************************************************
CPolyMesh::VertToBoundBoxRot - Looks at all the vertices (given the list of points)
and uses this to determine the bounding box.

inputs
   PCPoint        pMin, pMax - Filled with minimum and maximum
   DWORD          dwNumVert - Number of vertices to look through
   DWORD          *padwVert - Array of dwNumVert. If this is NULL then all points will be used
   PCMatrix       pmObjectToTexture - Rotates from object coords (that polymesh in) into
                  the texture coords.
*/
void CPolyMesh::VertToBoundBoxRot (PCPoint pMin, PCPoint pMax, DWORD dwNumVert, DWORD *padwVert,
                                   PCMatrix pmObjectToTexture)
{
   pMin->Zero();
   pMax->Zero();
   DWORD i;
   CPoint p;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);

   for (i = 0; i < (padwVert ? dwNumVert : m_lVert.Num()); i++) {
      p.Copy (&ppv[padwVert ? (padwVert[i]) : i]->m_pLoc);
      p.p[3] = 1;
      if (pmObjectToTexture)
         p.MultiplyLeft (pmObjectToTexture);

      if (i) {
         pMin->Min (&p);
         pMax->Max (&p);
      }
      else {
         pMin->Copy (&p);
         pMax->Copy (&p);
      }
   }
}




/*************************************************************************************
CPolyMesh::VertToBoundBox - Looks at all the vertices and uses this to determine
the bounding box.

inputs
   PCPoint        pMin, pMax - Filled in with the minimum and maximum
   PCPoint        pScaleSize - Can be NULL. Filled in with scale size
returns
   none
*/
void CPolyMesh::VertToBoundBox (PCPoint pMin, PCPoint pMax, PCPoint pScaleSize)
{
   if (!m_fDirtyScale)
      goto fillin;

   m_pBoundMin.Zero();
   m_pBoundMax.Zero();
   DWORD i;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);

   for (i = 0; i < m_lVert.Num(); i++) {
      if (i) {
         m_pBoundMin.Min (&ppv[i]->m_pLoc);
         m_pBoundMax.Max (&ppv[i]->m_pLoc);
      }
      else {
         m_pBoundMin.Copy (&ppv[i]->m_pLoc);
         m_pBoundMax.Copy (&ppv[i]->m_pLoc);
      }
   }

   m_fDirtyScale = FALSE;

fillin:
   if (pMin)
      pMin->Copy (&m_pBoundMin);
   if (pMax)
      pMax->Copy (&m_pBoundMax);
   if (pScaleSize) {
      // Because of symmetry reasons, scale is always from 0,0,0
      CPoint pmx, pmn;
      pmx.Copy (&m_pBoundMax);
      pmn.Copy (&m_pBoundMin);
      pScaleSize->Zero();
      pScaleSize->Max (&pmx);
      pmx.Scale(-1);
      pScaleSize->Max (&pmx);
      pScaleSize->Max (&pmn);
      pmn.Scale(-1);
      pScaleSize->Max (&pmn);
      pScaleSize->Scale (2.0);  // so cover all directions
      for (i = 0; i < 3; i++)
         pScaleSize->p[i] = max(CLOSE, pScaleSize->p[i]);
   }
   return;  // all done
}

/*************************************************************************************
CPolyMesh::WipeExtraText - Wipe any extra texture maps that may be in the points

inputs
   DWORD          dwNumSide - Number of sides to modify. These must be in sorted order
   DWORD          *padwSide - Array of dwNumSides. If this is NULL then do all sides.
*/
void CPolyMesh::WipeExtraText(DWORD dwNumSide, DWORD *padwSide)
{
   DWORD i;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);

   DWORD *padwRemove = NULL;
   CMem mem;
   if (padwSide) {
      if (!mem.Required (dwNumSide * sizeof(DWORD)))
         return;
      padwRemove = (DWORD*) mem.p;
      for (i = 0; i < dwNumSide; i++)
         padwRemove[i] = -1;
   }

   for (i = 0; i < m_lVert.Num(); i++) {
      if (padwSide)
         ppv[i]->SideTextRename (dwNumSide, padwSide, padwRemove, FALSE);
      else
         ppv[i]->SideTextClear();
   }

}

/*************************************************************************************
CPolyMesh::FixWraparoundTextures - Assumes that a sperical or cylindrical texutre
was placed using X and Y. This then looks for wrap-around segments. If it finds any it
will add extra texture information so it draws properly on the boundaries.

NOTE: This assumes values in H from -PI to PI

inputs
   DWORD          dwNumSide - Number of sides to modify. These must be in sorted order
   DWORD          *padwSide - Array of dwNumSides. If this is NULL then do all sides.
   PCMatrix       pmObjectToTexture - Rotates from object coords (that polymesh in) into
                  the texture coords.
*/
void CPolyMesh::FixWraparoundTextures (DWORD dwNumSide, DWORD *padwSide, PCMatrix pmObjectToTexture)
{
   // loop through all the sides
   PCPMSide *pps = (PCPMSide*)m_lSide.Get(0);
   DWORD i, dwSide;
   WORD j, wNum;
   DWORD *padwVert;
   PCPMVert pv;
   PTEXTUREPOINT ptp;
   TEXTUREPOINT tp;
   for (i = 0; i < (padwSide ? dwNumSide : m_lSide.Num()); i++) {
      dwSide = (padwSide ? padwSide[i] : i);

      // get all the vertices of the sides, finding the min/max .h value for the textures
      // along with the average
      padwVert = pps[dwSide]->VertGet();
      wNum = pps[dwSide]->m_wNumVert;
      fp fMin, fMax, fAvg;
      fMin = fMax = fAvg = 0;
      for (j = 0; j < wNum; j++) {
         pv = VertGet (padwVert[j]);
         ptp = pv->TextureGet (dwSide);   // BUGFIX - Used j instead of dwSide

         // rotate points
         CPoint pLoc;
         pLoc.Copy (&pv->m_pLoc);
         pLoc.p[3] = 1;
         if (pmObjectToTexture)
            pLoc.MultiplyLeft (pmObjectToTexture);

         // if we end up with a point at x==0 and y==0 then do some extra calculations
         // to determine approximately what the atan is. Do this by averaging the
         // angles of the two sides
         if ((fabs(pLoc.p[0]) < CLOSE) && (fabs(pLoc.p[1]) < CLOSE)) {
            CPoint pHalf;
            PCPMVert pv2;
            tp = *ptp;
            ptp = &tp;
            pv2 = VertGet(padwVert[(j+1)%wNum]);
            pHalf.Copy (&pv2->m_pLoc);
            pv2 = VertGet(padwVert[(j+wNum-1)%wNum]);
            pHalf.Average (&pv2->m_pLoc);

            if (pmObjectToTexture)
               pHalf.MultiplyLeft (pmObjectToTexture);

            tp.h = atan2 (pHalf.p[0], -pHalf.p[1]);

            pv->TextureSet (dwSide, &tp, TRUE); // BUGFIX - Added true
         }

         if (j) {
            fMin = min(fMin, ptp->h);
            fMax = max(fMax, ptp->h);
            fAvg += ptp->h;   // BUGFIX - Was just =
         }
         else {
            fMin = fMax = fAvg = ptp->h;
         }
      } // j
      fAvg /= (fp)wNum;

      // if the min and max are close to each other then no need to worry
      if (fMax - fMin < PI)
         continue;

      // else, there's a transition across the angle, so need to increase one
      // of the vertices... figure out which is the predominant sign
      DWORD dwMore, dwLess;
      dwMore = dwLess = 0;
      for (j = 0; j < wNum; j++) {
         PCPoint p1, p2;
         CPoint pTest;
         pv = VertGet(padwVert[j]);
         p1 = &pv->m_pLoc;
         pv = VertGet(padwVert[(j+1)%wNum]);
         p2 = &pv->m_pLoc;

         // test towards one end
         pTest.Average (p1, p2, .9);
         if (pmObjectToTexture)
            pTest.MultiplyLeft (pmObjectToTexture);

         if (atan2 (pTest.p[0], -pTest.p[1]) >= fAvg)
            dwMore++;
         else
            dwLess++;

         // and the other
         pTest.Average (p1, p2, .1);
         if (pmObjectToTexture)
            pTest.MultiplyLeft (pmObjectToTexture);

         if (atan2 (pTest.p[0], -pTest.p[1]) >= fAvg)
            dwMore++;
         else
            dwLess++;
      } // j

      // loop through all the vertices again, this time changing them
      for (j = 0; j < wNum; j++) {
         pv = VertGet(padwVert[j]);
         TEXTUREPOINT tp;
         tp = *(pv->TextureGet(dwSide));
         if ((dwLess >= dwMore) && (tp.h >= fAvg)) {
            tp.h -= 2.0 * PI;
            pv->TextureSet (dwSide, &tp, TRUE); // BUGFIX - make always add
         }
         else if ((dwLess < dwMore) && (tp.h < fAvg)) {
            tp.h += 2.0 * PI;
            pv->TextureSet (dwSide, &tp, TRUE); // BUGFIX - make always add
         }
      } // j
   } // i

}

/*************************************************************************************
CPolyMesh::VertOnEdge - Takes a list of vertices and a list of sides. It is assumed
that the list of vertices are all the vertices in the sides. This then looks through
the vertices and figures out which ones are connected to a side not on the list, and
fills in a new list with these.

inputs
   DWORD          dwNumVert - Number of vertices to look through
   DWORD          *padwVert - Array of dwNumVert. If this is NULL then all points will be used. Must be sorted
   DWORD          dwNumSide - Number of sides to modify. These must be in sorted order
   DWORD          *padwSide - Array of dwNumSides. If this is NULL then do all sides. Must be sorted
   PCListFixed    plVert - Filled in with an array of vertices from padwVert that are connected to a side
                     that is not in padwSide
*/
void CPolyMesh::VertOnEdge (DWORD dwNumVert, DWORD *padwVert, DWORD dwNumSide, DWORD *padwSide,
                            PCListFixed plVert)
{
   plVert->Init (sizeof(DWORD));
   if (!padwSide)
      return;  // since list of all sides, all the vertices are connected

   // loop through all the vertices
   DWORD i, j, dwVert;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < (padwVert ? dwNumVert : m_lVert.Num()); i++) {
      dwVert = (padwVert ? padwVert[i] : i);
      PCPMVert pv = ppv[dwVert];
      DWORD *padwSidePoly = pv->SideGet();
      for (j = 0; j < pv->m_wNumSide; j++)
         if (-1 == DWORDSearch (padwSidePoly[j], dwNumSide, padwSide))
            break;
      if (j < pv->m_wNumSide)
         plVert->Add (&dwVert);  // found a vertex with a side not in the list
   } // i
}

/*************************************************************************************
CPolyMesh::TextureSpherical - Create a spherical texture map.

NOTE: Doesn't call m_pWorld->ObjectChanged

inputs
   DWORD          dwNumSide - Number of sides to modify. These must be in sorted order
   DWORD          *padwSide - Array of dwNumSides. If this is NULL then do all sides.
   PCMatrix       pmObjectToTexture - Rotates from object coords (that polymesh in) into
                  the texture coords.
   BOOL           fInMeters - If TRUE size should be in meters, else wrap entirely around
*/
void CPolyMesh::TextureSpherical (DWORD dwNumSide, DWORD *padwSide, PCMatrix pmObjectToTexture,
                                  BOOL fInMeters)
{
   DWORD i;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);

   m_fDirtyRender = TRUE; // BUGFIX - So will cause a redraw
   m_fDirtyMorphCOM = TRUE;

   // BUGFIX - Move up
   WipeExtraText (dwNumSide, padwSide);

   // determine the vertices that need to look through
   CListFixed lVert, lVertOnEdge;
   DWORD dwNumVert, dwNumV, dwNumVertOnEdge;
   DWORD *padwVert, *padwVertOnEdge;
   lVert.Init (0);
   if (padwSide) {
      SidesToVert (padwSide, dwNumSide, &lVert);
      dwNumVert = lVert.Num();
      padwVert = (DWORD*)lVert.Get(0);
      dwNumV = dwNumVert;
   }
   else {
      dwNumVert = 0;
      padwVert = 0;
      dwNumV = m_lVert.Num();
   }
   VertOnEdge (dwNumVert, padwVert, dwNumSide, padwSide, &lVertOnEdge);
   dwNumVertOnEdge = lVertOnEdge.Num();
   padwVertOnEdge = (DWORD*)lVertOnEdge.Get(0);

   // figure out radius from min and max
   fp fRadH, fRadV;
   CPoint pMin, pMax, pCenter;
   VertToBoundBoxRot (&pMin, &pMax, dwNumVert, padwVert, pmObjectToTexture);
   pCenter.Average (&pMin, &pMax);
   if (fInMeters) {
      fRadH = (max(fabs(pMin.p[0]-pCenter.p[0]),fabs(pMax.p[0]-pCenter.p[0])) +
         max(fabs(pMin.p[1]-pCenter.p[1]),fabs(pMax.p[1]-pCenter.p[1])) ) / 2;
      fRadV = max(fabs(pMin.p[2]-pCenter.p[2]),fabs(pMax.p[2]-pCenter.p[2]));
   }
   else {
      fRadH = 1.0 / (2 * PI);
      fRadV = 1.0 / PI;
   }

   PCPMVert pv;
   for (i = 0; i < dwNumV; i++) {
      DWORD dwVert = (padwVert ? padwVert[i] : i);
      pv = ppv[dwVert];
      CPoint p;
      p.Copy (&pv->m_pLoc);
      p.p[3] = 1;
      if (pmObjectToTexture)
         p.MultiplyLeft (pmObjectToTexture);
      p.Subtract (&pCenter);

      // use the scale to undo for ellipsoids
      //p.p[0] /= (m_pScaleSize.p[0] / 2.0);
      //p.p[1] /= (m_pScaleSize.p[1] / 2.0);
      //p.p[2] /= (m_pScaleSize.p[2] / 2.0);

      // point
      TEXTUREPOINT tp;
      tp.h = atan2 (p.p[0], -p.p[1]);  // leave scaling out for now: * fRad * 2;
      if (tp.h >= PI - CLOSE)
         tp.h -= 2 * PI;   // so don't get case where have jagged boundary
      tp.v = -atan2 (p.p[2], sqrt(p.p[0]*p.p[0] + p.p[1] * p.p[1])) * fRadV;// BUGFIX - take out * 2;
      // BUGFIX - add PI/2 if is not in meters
      if (!fInMeters)
         tp.v += .5;

      // if this is on an outer edge then need to do piecewise set, else set all
      if (-1 != DWORDSearch (dwVert, dwNumVertOnEdge, padwVertOnEdge)) {
         DWORD j;
         DWORD *padwSidePoly = pv->SideGet();
         for (j = 0; j < pv->m_wNumSide; j++)
            if (-1 != DWORDSearch (padwSidePoly[j], dwNumSide, padwSide)) {
               pv->TextureSet (padwSidePoly[j], &tp, TRUE);
               padwSidePoly = pv->SideGet(); // reload since may muck up
            }
      }
      else
         pv->TextureSet (-1, &tp);
   }

   // fix wrapapound effect
   FixWraparoundTextures (dwNumSide, padwSide, pmObjectToTexture);

   // loop back and scale
   WORD j;
   for (i = 0; i < dwNumV; i++, pv++) {
      DWORD dwVert = (padwVert ? padwVert[i] : i);
      pv = ppv[dwVert];

      if (-1 == DWORDSearch (dwVert, dwNumVertOnEdge, padwVertOnEdge)) {
         pv->m_tText.h *= fRadH; // BUGFIX - Remove *2 since was wrong scale
         if (!fInMeters)
            pv->m_tText.h += .5;
      }

      PSIDETEXT pst = pv->SideTextGet();
      for (j = 0; j < pv->m_wNumSideText; j++)
         if (!padwSide || (-1 != DWORDSearch (pst[j].dwSide, dwNumSide, padwSide))) {
            pst[j].tp.h *= fRadH; // BUGFIX - Remove *2 since was wrong scale
            if (!fInMeters)
               pst[j].tp.h += .5;
         }
   }
}



/*************************************************************************************
CPolyMesh::TextureCylindrical - Create a spherical texture map.

NOTE: Doesn't call m_pWorld->ObjectChanged

inputs
   DWORD          dwNumSide - Number of sides to modify. These must be in sorted order
   DWORD          *padwSide - Array of dwNumSides. If this is NULL then do all sides.
   PCMatrix       pmObjectToTexture - Rotates from object coords (that polymesh in) into
                  the texture coords.
   BOOL           fInMeters - If TRUE size should be in meters, else wrap entirely around
*/
void CPolyMesh::TextureCylindrical (DWORD dwNumSide, DWORD *padwSide, PCMatrix pmObjectToTexture,
                                    BOOL fInMeters)
{
   DWORD i;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);

   m_fDirtyRender = TRUE; // BUGFIX - So will cause a redraw
   m_fDirtyMorphCOM = TRUE;

   // BUGFIX - Move up
   WipeExtraText (dwNumSide, padwSide);

   // determine the vertices that need to look through
   CListFixed lVert, lVertOnEdge;
   DWORD dwNumVert, dwNumV, dwNumVertOnEdge;
   DWORD *padwVert, *padwVertOnEdge;
   lVert.Init (0);
   if (padwSide) {
      SidesToVert (padwSide, dwNumSide, &lVert);
      dwNumVert = lVert.Num();
      padwVert = (DWORD*)lVert.Get(0);
      dwNumV = dwNumVert;
   }
   else {
      dwNumVert = 0;
      padwVert = 0;
      dwNumV = m_lVert.Num();
   }
   VertOnEdge (dwNumVert, padwVert, dwNumSide, padwSide, &lVertOnEdge);
   dwNumVertOnEdge = lVertOnEdge.Num();
   padwVertOnEdge = (DWORD*)lVertOnEdge.Get(0);


   // figure out radius from min and max
   fp fRadH, fRadV;
   CPoint pMin, pMax, pCenter;
   VertToBoundBoxRot (&pMin, &pMax, dwNumVert, padwVert, pmObjectToTexture);
   pCenter.Average (&pMin, &pMax);
   if (fInMeters) {
      fRadH = (max(fabs(pMin.p[0]-pCenter.p[0]),fabs(pMax.p[0]-pCenter.p[0])) +
         max(fabs(pMin.p[1]-pCenter.p[1]),fabs(pMax.p[1]-pCenter.p[1])) ) / 2;
      fRadV = 1;
   }
   else {
      fRadH = 1.0 / (2.0 * PI);
      fRadV = 1.0 / max(CLOSE, pMax.p[2] - pMin.p[2]);
      pCenter.p[2] = pMax.p[2];  // so start at top
   }

   PCPMVert pv;
   for (i = 0; i < dwNumV; i++) {
      DWORD dwVert = (padwVert ? padwVert[i] : i);
      pv = ppv[dwVert];
      CPoint p;
      p.Copy (&pv->m_pLoc);
      p.p[3] = 1;
      if (pmObjectToTexture)
         p.MultiplyLeft (pmObjectToTexture);
      p.Subtract (&pCenter);

      // use the scale to undo for ellipsoids
      //p.p[0] /= (m_pScaleSize.p[0] / 2.0);
      //p.p[1] /= (m_pScaleSize.p[1] / 2.0);

      // point
      TEXTUREPOINT tp;
      tp.h = atan2 (p.p[0], -p.p[1]);  // leave scaling until later * fRad * 2;
      if (tp.h >= PI - CLOSE)
         tp.h -= 2 * PI;   // so don't get case where have jagged boundary
      tp.v = -p.p[2];
      // BUGFIX - If not in meters then adjust v
      if (!fInMeters)
         tp.v *= fRadV;

      // if this is on an outer edge then need to do piecewise set, else set all
      if (-1 != DWORDSearch (dwVert, dwNumVertOnEdge, padwVertOnEdge)) {
         DWORD j;
         DWORD *padwSidePoly = pv->SideGet();
         for (j = 0; j < pv->m_wNumSide; j++)
            if (-1 != DWORDSearch (padwSidePoly[j], dwNumSide, padwSide)) {
               pv->TextureSet (padwSidePoly[j], &tp, TRUE);
               padwSidePoly = pv->SideGet(); // reload since may muck up
            }
      }
      else
         pv->TextureSet (-1, &tp);
   }

   // fix wrapapound effect
   FixWraparoundTextures (dwNumSide, padwSide, pmObjectToTexture);

   // loop back and scale
   WORD j;
   for (i = 0; i < dwNumV; i++, pv++) {
      DWORD dwVert = (padwVert ? padwVert[i] : i);
      pv = ppv[dwVert];

      if (-1 == DWORDSearch (dwVert, dwNumVertOnEdge, padwVertOnEdge)) {
         pv->m_tText.h *= fRadH; // BUGFIX - Remove *2 since was wrong scale
         if (!fInMeters)
            pv->m_tText.h += .5;
      }

      PSIDETEXT pst = pv->SideTextGet();
      for (j = 0; j < pv->m_wNumSideText; j++)
         if (!padwSide || (-1 != DWORDSearch (pst[j].dwSide, dwNumSide, padwSide))) {
            pst[j].tp.h *= fRadH; // BUGFIX - Remove *2 since was wrong scale
            if (!fInMeters)
               pst[j].tp.h += .5;
         }
   }
}

/*************************************************************************************
CPolyMesh::TextureLinear - Linear projection from the front of above


inputs
   DWORD          dwNumSide - Number of sides to modify. These must be in sorted order
   DWORD          *padwSide - Array of dwNumSides. If this is NULL then do all sides.
   PCMatrix       pmObjectToTexture - Rotates from object coords (that polymesh in) into
                  the texture coords.
   BOOL           fInMeters - If TRUE size should be in meters, else wrap entirely around

NOTE: Doesn't call m_pWorld->ObjectChanged
*/
void CPolyMesh::TextureLinear (DWORD dwNumSide, DWORD *padwSide, PCMatrix pmObjectToTexture,
                               BOOL fInMeters)
{
   DWORD i;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);

   m_fDirtyRender = TRUE; // BUGFIX - So will cause a redraw
   m_fDirtyMorphCOM = TRUE;

   // determine the vertices that need to look through
   CListFixed lVert, lVertOnEdge;
   DWORD dwNumVert, dwNumV, dwNumVertOnEdge;
   DWORD *padwVert, *padwVertOnEdge;
   lVert.Init (0);
   if (padwSide) {
      SidesToVert (padwSide, dwNumSide, &lVert);
      dwNumVert = lVert.Num();
      padwVert = (DWORD*)lVert.Get(0);
      dwNumV = dwNumVert;
   }
   else {
      dwNumVert = 0;
      padwVert = 0;
      dwNumV = m_lVert.Num();
   }
   VertOnEdge (dwNumVert, padwVert, dwNumSide, padwSide, &lVertOnEdge);
   dwNumVertOnEdge = lVertOnEdge.Num();
   padwVertOnEdge = (DWORD*)lVertOnEdge.Get(0);


   fp fRadH, fRadV;
   CPoint pMin, pMax, pCenter;
   VertToBoundBoxRot (&pMin, &pMax, dwNumVert, padwVert, pmObjectToTexture);
   pCenter.Average (&pMin, &pMax);
   WipeExtraText (dwNumSide, padwSide);
   if (fInMeters) {
      fRadH = fRadV = 1;
   }
   else {
      fRadH = 1.0 / max(CLOSE, pMax.p[0] - pMin.p[0]);
      fRadV = 1.0 / max(CLOSE, pMax.p[2] - pMin.p[2]);
      pCenter.p[0] = pMin.p[0];  // so start on left
      pCenter.p[2] = pMax.p[2];  // so start at top
   }

   PCPMVert pv;
   for (i = 0; i < dwNumV; i++) {
      DWORD dwVert = (padwVert ? padwVert[i] : i);
      pv = ppv[dwVert];

      CPoint p;
      p.Copy (&pv->m_pLoc);
      p.p[3] = 1;
      if (pmObjectToTexture)
         p.MultiplyLeft (pmObjectToTexture);
      p.Subtract (&pCenter);

      TEXTUREPOINT tp;
      tp.h = p.p[0] * fRadH;
      tp.v = -p.p[2] * fRadV;

      // if this is on an outer edge then need to do piecewise set, else set all
      if (-1 != DWORDSearch (dwVert, dwNumVertOnEdge, padwVertOnEdge)) {
         DWORD j;
         DWORD *padwSidePoly = pv->SideGet();
         for (j = 0; j < pv->m_wNumSide; j++)
            if (-1 != DWORDSearch (padwSidePoly[j], dwNumSide, padwSide)) {
               pv->TextureSet (padwSidePoly[j], &tp, TRUE);
               padwSidePoly = pv->SideGet(); // reload since may muck up
            }
      }
      else
         pv->TextureSet (-1, &tp);
   }
}





/**********************************************************************************
CPolyMesh::CreateBasedOnType - Fills in all the points based on the type
of object in dwType. (Fails if no divisions.)

inputs
   DWORD          dwType - 0 for sphere, etc. see below
   DWORD          dwDivisions - Number of facets to divide into (approx)
   PCPoint        paParam - Pointer to an array of up to 2 parameters
*/
void CPolyMesh::CreateBasedOnType (DWORD dwType, DWORD dwDivisions, PCPoint paParam)
{
   // Fill in points
   // BUGFIX - Fewer points
   switch (dwType) {
   case 0:  // sphere
   default:
      CreateSphere (dwDivisions * dwDivisions, &paParam[0]);
      break;
   case 1:  // cylinder without cap
      CreateCylinder (dwDivisions, &paParam[0], &paParam[1], FALSE);
      break;
   case 2:  // cylinder with cap
      CreateCylinder (dwDivisions, &paParam[0], &paParam[1], TRUE);
      break;
   case 3:  // cone, open on tpo
      CreateCone (dwDivisions, &paParam[0], FALSE, TRUE, paParam[1].p[0], paParam[1].p[1]);
      break;
   case 4:  // cone, closed on top
      CreateCone (dwDivisions, &paParam[0], TRUE, TRUE, paParam[1].p[0], paParam[1].p[1]);
      break;
   case 5:  // disc
      CreateCone (dwDivisions, &paParam[0], TRUE, FALSE, paParam[1].p[0], paParam[1].p[1]);
      break;
   case 6:  // two cones
      CreateCone (dwDivisions, &paParam[0], TRUE, TRUE, paParam[1].p[0], paParam[1].p[1]);
      break;
   case 7:  // box
      CreateBox (dwDivisions, &paParam[0]);
      break;
   case 8:  // plane
      CreatePlane (dwDivisions, &paParam[0]);
      break;
   case 9:  // taurus
      CreateTaurus (dwDivisions, &paParam[0], &paParam[1]);
      break;
   case 10:  // cylinder without rounded ends
      CreateCylinderRounded (dwDivisions, &paParam[0], &paParam[1]);
      break;
   }

   // calculate the scale
   //CalcScale ();

   // do the textures
   BOOL fInMeters = TRUE;
   switch (dwType) {
   case 0:  // sphere
   default:
   case 9:  // taurus
   case 10: // cylinder rounded
      TextureSpherical (0, NULL, NULL, fInMeters);
      break;
   case 1:  // cylinder without cap
   case 2:  // cylinder with cap
   case 3:  // cone, open on tpo
   case 4:  // cone, closed on top
   case 6:  // two cones
      TextureCylindrical (0, NULL, NULL, fInMeters);
      break;
   case 5:  // disc
   case 8:  // plane
      CMatrix mTop;
      CPoint pA, pB, pC;
      pA.Zero();
      pB.Zero();
      pC.Zero();
      pA.p[0] = 1;
      pB.p[2] = 1;
      pC.p[1] = 1;
      mTop.RotationFromVectors (&pA, &pB, &pC);
      TextureLinear (0, NULL, &mTop, fInMeters);
      break;
   case 7:  // box
      TextureLinear (0, NULL, NULL, fInMeters);
   }
}

/**********************************************************************************
CPolyMesh::CreateSphere - Create a meash that's a sphere. This
erases all existing vertices and sides.

inputs
   DWORD    dwNum - Number of points, approximately, in the sphere
   PCPoint  pScale - Normally -1 to 1 sphere, but multiply bu this\
returns
   none
*/
void CPolyMesh::CreateSphere (DWORD dwNum, PCPoint pScale)
{
   DWORD dwSurface = 0;    // what surface will create for

   // scale this down
   dwNum /= 8; // divide into 8 triangles
   dwNum *= 2; // make the triangles into squares
   dwNum = (DWORD) sqrt((fp)dwNum);  // find the number up and down
   dwNum = max(1,dwNum);   // at least two points, for top and bottom

   // clear out old list
   Clear();
   SymmetrySet (0x07);  // full symmetry

   // scratch memory for IDs
   CMem  mem;
   if (!mem.Required ((dwNum+1) * (dwNum+1) * sizeof(DWORD)))
      return;
   DWORD *padwScratch;
   padwScratch = (DWORD*) mem.p;

   // create one triangle and curve it so the top is at the pole, and bottom corners at 0 and 90 degrees
   DWORD x, y;
   for (x = 0; x <= dwNum; x++) for (y = 0; x+y <= dwNum; y++) {
      fp fLat, fLong;
      if (y < dwNum)
         fLong = ((fp)x / (fp)(dwNum-y)) * PI/2;
      else
         fLong = 0;  // at north/south pole
      fLat = (fp) y / (fp)dwNum * PI/2;

      // calculate the point
      CPoint pLoc;
      pLoc.Zero();
      pLoc.p[0] = cos(fLong) * cos(fLat);
      pLoc.p[1] = sin(fLong) * cos(fLat);
      pLoc.p[2] = sin(fLat);

      // if close to 0 then set to 0
      if (fabs(pLoc.p[0]) < CLOSE)
         pLoc.p[0] = 0;
      if (fabs(pLoc.p[1]) < CLOSE)
         pLoc.p[1] = 0;
      if (fabs(pLoc.p[2]) < CLOSE)
         pLoc.p[2] = 0;

      // scale
      pLoc.p[0] *= pScale->p[0];
      pLoc.p[1] *= pScale->p[1];
      pLoc.p[2] *= pScale->p[2];

      // add it
      padwScratch[x + (y * (dwNum+1))] = VertAdd (&pLoc);
   }  // x and y


   // now, create all the triangles
   DWORD adwVert[3];
   for (x = 0; x < dwNum; x++) for (y = 0; x+y < dwNum; y++) {
      // first one
      adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
      adwVert[1] = padwScratch[x + (y * (dwNum+1))];
      adwVert[2] = padwScratch[x + ((y+1) * (dwNum+1))];
      SideAdd (dwSurface, adwVert, 3);

      // might be another side
      if (x+y+2 > dwNum)
         continue;
      adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
      adwVert[1] = padwScratch[x + ((y+1) * (dwNum+1))];
      adwVert[2] = padwScratch[(x+1) + ((y+1) * (dwNum+1))];
      SideAdd (dwSurface, adwVert, 3);

   }  // over x and y

   // NOTE: Because have symmetry turned on all the rest is magically done for us

   m_fCanBackface = TRUE;
}



/**********************************************************************************
CPolyMesh::CreateCone - Create a meash that's a sphere. This
erases all existing vertices and sides.

inputs
   DWORD    dwNum - Number of points around the diameter
   PCPoint   pScale - x and y Scale around diameter. Normally -1 to 1 sphere, but multiply bu this
   BOOL     fShowTop, fShowBottom - If TRUE, show the top/bottom.
   fp       fTop, fBottom - Height of the top and bottom
returns
   none
*/
void CPolyMesh::CreateCone (DWORD dwNum, PCPoint pScale, BOOL fShowTop, BOOL fShowBottom,
                                  fp fTop, fp fBottom)
{
   DWORD dwSurface = 0;    // what surface will create for

   // scale this down
   dwNum /= 4;
   dwNum = max(1,dwNum);   // at least two points, for top and bottom

   // BUGBUG - Still not happy with create cone. may want to use rectangles
   // instead of triangles to create

   // extra boundary pieces at top and two at bottom so that get nice normals
   BOOL fExtraBottom, fExtraTop;
   fExtraBottom = (fShowTop && fShowBottom); // so will merge better
   fExtraTop = (fShowTop && (fTop != 0)) || (fShowBottom && (fBottom != 0));
   if (fExtraBottom)
      dwNum += 1;
   if (fExtraTop)
      dwNum += 1;

   // clear out old list
   Clear();
   SymmetrySet (0x03);  // x and y symmetry

   // scratch memory for IDs
   CMem  mem;
   if (!mem.Required ((dwNum+1) * (dwNum+1) * sizeof(DWORD)))
      return;
   DWORD *padwScratch;
   padwScratch = (DWORD*) mem.p;

   // create 8 triangles
   DWORD dwBottom;
   for (dwBottom = 0; dwBottom < 2; dwBottom++) {
      // show top and bottom
      if (dwBottom && !fShowBottom)
         continue;
      if (!dwBottom && !fShowTop)
         continue;

      // in each triangle, calculate the points...
      DWORD x, y;
      for (x = 0; x <= dwNum; x++) for (y = 0; x+y <= dwNum; y++) {
         fp fLat, fLong, fHeight;
         if (fExtraBottom && !y)
            fLong = ((fp)x / (fp)(dwNum-(y+1))) * PI/2;  // BUGFIX - Extra quad around rim
         else if (y < dwNum)
            fLong = ((fp)x / (fp)(dwNum-y)) * PI/2;
         else
            fLong = 0;  // at north/south pole

         if (fExtraBottom) {
            fLat = 1.0 - ((fp) y-(fp)1) / (fp)(dwNum - (fExtraTop ? 2 : 1));
            fLat = min(1, fLat);
            fLat = max(0, fLat);
            if (y == 1)
               fLat *= .99;   // extra bit
            else if (fExtraTop && (y == dwNum-1))
               fLat = .01;   // extra bit
         }
         else if (fExtraTop) {   // but not extra bottom
            fLat = 1.0 - (fp) y / (fp)(dwNum - 1);
            fLat = min(1, fLat);
            fLat = max(0, fLat);
            if (y == dwNum-1)
               fLat = .01;   // extra bit
         }
         else {
            fLat = 1.0 - (fp) y / (fp)dwNum;
         }
         fHeight = (dwBottom ? fBottom : fTop) * (1.0 - fLat);

         // if in the southern hemispher then negative latitiude and reverse longitude
         if (dwBottom) {
            fLat *= -1;
            fLong *= -1;
         }

         // calculate the point
         CPoint pLoc;
         pLoc.Zero();
         pLoc.p[0] = cos(fLong) * fLat;
         pLoc.p[1] = sin(fLong) * fLat;
         pLoc.p[2] = fHeight;

         // if close to 0 set to 0
         if (fabs(pLoc.p[0]) < CLOSE)
            pLoc.p[0] = 0;
         if (fabs(pLoc.p[1]) < CLOSE)
            pLoc.p[1] = 0;

         // scale
         pLoc.p[0] *= pScale->p[0];
         pLoc.p[1] *= pScale->p[1];

         // add it
         DWORD dwVert;
         dwVert = -1;
         if (!y && dwBottom)
            dwVert = VertFind (&pLoc);
         if (dwVert == -1)
            dwVert = VertAdd (&pLoc);
         padwScratch[x + (y * (dwNum+1))] = dwVert;
      }  // x and y


      // now, create all the triangles
      DWORD adwVert[4];
      for (x = 0; x < dwNum; x++) {
         if (fExtraBottom && (x+1 < dwNum)) {
            y = 0;
            adwVert[0] = padwScratch[x + (y * (dwNum+1))];
            adwVert[3] = padwScratch[(x+1) + (y * (dwNum+1))];
            adwVert[2] = padwScratch[(x+1) + ((y+1) * (dwNum+1))];
            adwVert[1] = padwScratch[x + ((y+1) * (dwNum+1))];
            SideAdd (dwSurface, adwVert, 4);
         }

         for (y = fExtraBottom ? 1 : 0; x+y < dwNum; y++) {
            // first one
            adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
            adwVert[1] = padwScratch[x + (y * (dwNum+1))];
            adwVert[2] = padwScratch[x + ((y+1) * (dwNum+1))];
            SideAdd (dwSurface, adwVert, 3);

            // might be another side
            if (x+y+2 > dwNum)
               continue;
            adwVert[0] = padwScratch[(x+1) + (y * (dwNum+1))];
            adwVert[1] = padwScratch[x + ((y+1) * (dwNum+1))];
            adwVert[2] = padwScratch[(x+1) + ((y+1) * (dwNum+1))];
            SideAdd (dwSurface, adwVert, 3);
         } // y
      }  // over x
   }  // over dwBottom

   // NOTE: Only need to do 1/4 of cone because rest is done automatically with
   // symmetry

   m_fCanBackface = fShowTop && fShowBottom;
}



/**********************************************************************************
CPolyMesh::CreateCylinder - Create a cylinder without any caps

inputs
   DWORD    dwNum - Number of points, approximately, around the radius of the cylinder.
   PCPoint  pScale - Normally -1 to 1 sphere, but multiply bu this
   PCPoint  pScaleNegative - Scale in the negative end. Only x and y are used
   BOOL     fCap - if TRUE, draw caps on the end, else not
returns
   none
*/
void CPolyMesh::CreateCylinder (DWORD dwNum, PCPoint pScale, PCPoint pScaleNegative, BOOL fCap)
{
   DWORD dwSurface = 0;    // what surface will create for

   dwNum = max(1,dwNum);

   // number of points over length
   DWORD dwNumV;
   fp fAvgRadius, fAvgGridSize;
   fAvgRadius = (pScale->p[0] + pScale->p[1] + pScaleNegative->p[0] + pScaleNegative->p[1]) / 4;
   fAvgGridSize = fAvgRadius * 2 * PI / (fp) dwNum;
   dwNumV = (DWORD) (pScale->p[2] * 3 / fAvgGridSize);   // BUGFIX - Was *2, but not enough divisions
   dwNumV = max(2, dwNumV);
   if (fCap)
      dwNumV += 4;   // since need to extra rows on each end

   // scale this down
   dwNum /= 4; // divide into 4 sections around
   // was in old one: dwNum *= 4;
   // was in old one: dwNum = max(4,dwNum);
   dwNum = max(1, dwNum);

   // clear out old list
   Clear();
   SymmetrySet (0x03);  // x and y symmetry

   // scratch memory for IDs
   CMem  mem;
   if (!mem.Required ((dwNum+1) * dwNumV * sizeof(DWORD)))
      return;
   DWORD *padwScratch;
   padwScratch = (DWORD*) mem.p;

   // create loop
   // in each triangle, calculate the points...
   DWORD x, y;
   CPoint pLoc;
   for (y = 0; y < dwNumV; y++) for (x = 0; x <= dwNum; x++) {
      fp fLong;
      fLong = (fp)x / (fp)dwNum * PI / 2.0;

      // calculate the point
      pLoc.Zero();
      pLoc.p[0] = cos(fLong);
      pLoc.p[1] = sin(fLong);
      if (fabs(pLoc.p[0]) < CLOSE)
         pLoc.p[0] = 0;
      if (fabs(pLoc.p[1]) < CLOSE)
         pLoc.p[1] = 0;
      if (fCap) {
         pLoc.p[2] = (((fp) y-2.0) / (fp) (dwNumV-1-4) * 2.0 - 1.0);
         pLoc.p[2] = min(1, pLoc.p[2]);
         pLoc.p[2] = max(-1, pLoc.p[2]);
         if ((y >= 2) && (y < dwNumV-2))
            pLoc.p[2] *= .99; // so dont go all the way to the top

         if ((y == 0) || (y == dwNumV-1)) {
            // bend the top in
            pLoc.p[0] *= .99;
            pLoc.p[1] *= .99;
         }
      }
      else {
         pLoc.p[2] = ((fp) y / (fp) (dwNumV-1) * 2.0 - 1.0);
      }

      // scale
      CPoint pTempScale;
      pTempScale.Average (pScale, pScaleNegative, (pLoc.p[2] + 1) /2.0);
      pLoc.p[0] *= pTempScale.p[0];
      pLoc.p[1] *= pTempScale.p[1];
      pLoc.p[2] *= pScale->p[2];

      padwScratch[x + (y * (dwNum+1))] = VertAdd (&pLoc);  // symmetry will add 4 points
   }

   // now, create all the triangles
   DWORD adwVert[4];
   for (y = 0; y+1 < dwNumV; y++) for (x = 0; x < dwNum; x++) {
      // BUGFIX - Make one quad out of it
      // first one
      adwVert[0] = padwScratch[(x+1) + y * (dwNum+1)];
      adwVert[1] = padwScratch[x + y * (dwNum+1)];
      adwVert[2] = padwScratch[x + (y+1) * (dwNum+1)];
      adwVert[3] = padwScratch[(x+1) + (y+1) * (dwNum+1)];
      SideAdd (dwSurface, adwVert, 4);
   }  // over x and y


   // make the top and bottom caps
   if (fCap) {
      DWORD dwBottom;
      for (dwBottom = 0; dwBottom < 2; dwBottom++) {
         // create a point on top and bottom
         DWORD dwVert;
         pLoc.Zero();
         pLoc.p[2] = (dwBottom ? 1 : -1) * pScale->p[2];
         dwVert = VertAdd (&pLoc);

         // do sides
         y = (dwBottom ? (dwNumV-1) : 0);
         for (x = 0; x < dwNum; x++) {
            adwVert[0] = adwVert[2] = padwScratch[(x+1) + y * (dwNum+1)];
            adwVert[1] = padwScratch[x + y * (dwNum+1)];
            adwVert[dwBottom ? 2 : 0] = dwVert;
            SideAdd (dwSurface, adwVert, 3);
         }
      } // dwBottom

   }

   m_fCanBackface = fCap;
}



/**********************************************************************************
CPolyMesh::CreateCylinderRounded - Create a cylinder with rounded (spherical)
top and bottom.

inputs
   DWORD    dwNum - Number of points, approximately, around the radius of the cylinder.
   PCPoint  pScale - Normally -1 to 1 sphere, but multiply bu this. w is the size of the cylinder
   PCPoint  pScaleNegative - Scale in the negative end.
returns
   none
*/
void CPolyMesh::CreateCylinderRounded (DWORD dwNum, PCPoint pScale, PCPoint pScaleNegative)
{
   DWORD dwSurface = 0;    // what surface will create for

   dwNum = max(1,dwNum);

   // number of points over length
   DWORD dwNumV;
   fp fAvgRadius, fAvgGridSize;
   fAvgRadius = (pScale->p[0] + pScale->p[1] + pScaleNegative->p[0] + pScaleNegative->p[1]) / 4;
   fAvgGridSize = fAvgRadius * 2 * PI / (fp) dwNum;
   dwNumV = (DWORD) (pScale->p[3] * 2 / fAvgGridSize);
   dwNumV = max(2, dwNumV);

   // scale this down
   dwNum /= 4; // divide into 4 sections around
   // bugfix - remove from old dwNum *= 4;
   // bugfix - remove from old dwNum = max(4,dwNum);
   dwNum = max(dwNum, 1);

   // clear out old list
   Clear();
   SymmetrySet (0x03);  // x and y symmetry

   // scratch memory for IDs
   CMem  mem;
   if (!mem.Required ((dwNum+1) * dwNumV * sizeof(DWORD)))
      return;
   DWORD *padwScratch;
   padwScratch = (DWORD*) mem.p;

   // create loop
   // in each triangle, calculate the points...
   DWORD x, y;
   for (y = 0; y < dwNumV; y++) for (x = 0; x <= dwNum; x++) {
      fp fLong;
      fLong = (fp)x / (fp)dwNum * PI / 2.0;

      // calculate the point
      CPoint pLoc;
      pLoc.Zero();
      pLoc.p[0] = cos(fLong);
      pLoc.p[1] = sin(fLong);
      if (fabs(pLoc.p[0]) < CLOSE)
         pLoc.p[0] = 0;
      if (fabs(pLoc.p[1]) < CLOSE)
         pLoc.p[1] = 0;
      pLoc.p[2] = ((fp) y / (fp) (dwNumV-1) * 2.0 - 1.0);

      // scale
      CPoint pTempScale;
      pTempScale.Average (pScale, pScaleNegative, (pLoc.p[2] + 1) /2.0);
      pLoc.p[0] *= pTempScale.p[0];
      pLoc.p[1] *= pTempScale.p[1];
      pLoc.p[2] *= pScale->p[3];

      padwScratch[x + (y * (dwNum+1))] = VertAdd (&pLoc);  // symmetry will add 4 points
   }

   // now, create all the triangles
   DWORD adwVert[4];
   for (y = 0; y+1 < dwNumV; y++) for (x = 0; x < dwNum; x++) {
      // BUGFIX - Make one quad out of it
      adwVert[0] = padwScratch[(x+1) + y * (dwNum+1)];
      adwVert[1] = padwScratch[x + y * (dwNum+1)];
      adwVert[2] = padwScratch[x + (y+1) * (dwNum+1)];
      adwVert[3] = padwScratch[(x+1) + (y+1) * (dwNum+1)];
      SideAdd (dwSurface, adwVert, 4);
   }  // over x and y


   // make the top and bottom caps

   // scratch memory for IDs
   CMem  mem2;
   if (!mem2.Required ((dwNum+1) * (dwNum+1) * sizeof(DWORD)))
      return;
   DWORD *padwScratch2;
   padwScratch2 = (DWORD*) mem2.p;

   // create 8 triangles
   DWORD dwBottom;
   for (dwBottom = 0; dwBottom < 2; dwBottom++) {
      // NOTE: Dont bother with sides because symmetry will take care of
      // in each triangle, calculate the points...
      DWORD x, y;
      for (x = 0; x <= dwNum; x++) for (y = 0; x+y <= dwNum; y++) {
         // if at y==0 then this matches up exactly with something in padwSratch,
         // so use that
         if (y==0) {
            padwScratch2[x + y * (dwNum+1)] = padwScratch[x +
               (dwBottom ? 0 : (dwNumV-1)) * (dwNum+1)];
            continue;
         }

         fp fLat, fLong;
         if (y < dwNum)
            fLong = ((fp)x / (fp)(dwNum-y)) * PI/2;
         else
            fLong = 0;  // at north/south pole
         fLat = (fp) y / (fp)dwNum * PI/2;

         // if in the southern hemispher then negative latitiude and reverse longitude
         if (dwBottom) {
            fLat *= -1;
            //fLong *= -1;
         }

         // calculate the point
         CPoint pLoc;
         pLoc.Zero();
         pLoc.p[0] = cos(fLong) * cos(fLat);
         pLoc.p[1] = sin(fLong) * cos(fLat);
         pLoc.p[2] = sin(fLat) * (dwBottom ? pScaleNegative : pScale)->p[2] + (dwBottom ? -1 : 1) * pScale->p[3];

         // set close to 0
         if (fabs(pLoc.p[0]) < CLOSE)
            pLoc.p[0] = 0;
         if (fabs(pLoc.p[1]) < CLOSE)
            pLoc.p[1] = 0;

         // scale
         pLoc.p[0] *= (dwBottom ? pScaleNegative : pScale)->p[0];
         pLoc.p[1] *= (dwBottom ? pScaleNegative : pScale)->p[1];

         padwScratch2[x + (y * (dwNum+1))] = VertAdd (&pLoc);

      }  // x and y


      // now, create all the triangles
      for (x = 0; x < dwNum; x++) for (y = 0; x+y < dwNum; y++) {
         // first one
         adwVert[dwBottom ? 2 : 0] = padwScratch2[(x+1) + (y * (dwNum+1))];
         adwVert[1] = padwScratch2[x + (y * (dwNum+1))];
         adwVert[dwBottom ? 0 : 2] = padwScratch2[x + ((y+1) * (dwNum+1))];
         SideAdd (dwSurface, adwVert, 3);

         // might be another side
         if (x+y+2 > dwNum)
            continue;
         adwVert[dwBottom ? 2 : 0] = padwScratch2[(x+1) + (y * (dwNum+1))];
         adwVert[1] = padwScratch2[x + ((y+1) * (dwNum+1))];
         adwVert[dwBottom ? 0 : 2] = padwScratch2[(x+1) + ((y+1) * (dwNum+1))];
         SideAdd (dwSurface, adwVert, 3);

      }  // over x and y
   }  // over dwBottom

   m_fCanBackface = TRUE;
}




/**********************************************************************************
CPolyMesh::CreateTaurus - Taurus shape

inputs
   DWORD    dwNum - Number of points, approximately, around the radius of the cylinder.
   PCPoint  pScale - x and y are used for the whole taurus
   PCPoint  pInnterScale - x and y are used for the radius of the extrusion part
returns
   none
*/
void CPolyMesh::CreateTaurus (DWORD dwNum, PCPoint pScale, PCPoint pInnerScale)
{
   DWORD dwSurface = 0;    // what surface will create for

   // scale this down
   dwNum /= 4; // divide into 4 sections around
   // remove dwNum *= 4;
   // dwNum = max(8,dwNum);
   dwNum = max(2,dwNum);

   // number of points over length
   DWORD dwNumV;
   dwNumV = dwNum;  // BUGFIX - Was dwNum/4
   dwNumV = max(2, dwNumV);

   // clear out old list
   Clear();
   SymmetrySet (0x07);  // full symmetry

   // scratch memory for IDs
   CMem  mem;
   if (!mem.Required ((dwNum+1) * (dwNumV+1) * sizeof(DWORD)))
      return;
   DWORD *padwScratch;
   padwScratch = (DWORD*) mem.p;

   // create loop
   // in each triangle, calculate the points...
   DWORD x, y;
   for (y = 0; y <= dwNumV; y++) for (x = 0; x <= dwNum; x++) {
      fp fLong, fLat;
      fLong = (fp)x / (fp)dwNum * PI / 2.0;
      fLat = ((fp)y / (fp)dwNumV) * PI;

      // calculate the point
      CPoint pLoc;
      pLoc.Zero();
      pLoc.p[0] = cos(fLong) * (1 + cos (fLat) * pInnerScale->p[0] / pScale->p[0]);
      pLoc.p[1] = sin(fLong) * (1 + cos (fLat) * pInnerScale->p[0] / pScale->p[1]);
      pLoc.p[2] = sin(fLat);


      // if close to zero set to 0
      if (fabs(pLoc.p[0]) < CLOSE)
         pLoc.p[0] = 0;
      if (fabs(pLoc.p[1]) < CLOSE)
         pLoc.p[1] = 0;
      if (fabs(pLoc.p[2]) < CLOSE)
         pLoc.p[2] = 0;

      // scale
      pLoc.p[0] *= pScale->p[0];
      pLoc.p[1] *= pScale->p[1];
      pLoc.p[2] *= pInnerScale->p[1];

      padwScratch[x + (y * (dwNum+1))] = VertAdd (&pLoc);  // symmetry will add 4 points
   }

   // now, create all the triangles
   DWORD adwVert[4];
   for (y = 0; y < dwNumV; y++) for (x = 0; x < dwNum; x++) {
      // BUGFIX - Only one quad
      // first one
      adwVert[0] = padwScratch[(x+1) + y * (dwNum+1)];
      adwVert[1] = padwScratch[x + y * (dwNum+1)];
      adwVert[2] = padwScratch[x + (y+1) * (dwNum+1)];
      adwVert[3] = padwScratch[(x+1) + (y+1) * (dwNum+1)];
      SideAdd (dwSurface, adwVert, 4);
   }  // over x and y


   m_fCanBackface = TRUE;  // BUGFIX - Can backface cull taurus
}

/**********************************************************************************
CPolyMesh::CreateBox - Create a box

inputs
   DWORD    dwNum - Number of points across on each side
   PCPoint  pScale - Normally -1 to 1 sphere, but multiply bu this
returns
   none
*/
void CPolyMesh::CreateBox (DWORD dwNum, PCPoint pScale)
{
   DWORD dwSurface = 0;    // what surface will create for

   dwNum = (DWORD) sqrt((fp)dwNum);
   dwNum = max(dwNum,1); // so at least one

   // clear out old list
   Clear();
   SymmetrySet (0x7);  // no symmetry


   // create all the sides... since have symmetry don't have quite as many points
   DWORD dwDim, dwDim1, dwDim2, x, y, i;
   CPoint ap[4];
   DWORD adwVert[4];
   for (dwDim = 0; dwDim < 3; dwDim++) {
      dwDim1 = (dwDim+1)%3;
      dwDim2 = (dwDim+2)%3;

      for (x = 0; x < (dwNum+1)/2; x++) for (y = 0; y < (dwNum+1)/2; y++) {
         // first corner
         ap[0].p[dwDim1] = pScale->p[dwDim1] * 2.0 * ((fp)x / (fp)dwNum - 0.5);
         ap[0].p[dwDim2] = pScale->p[dwDim2] * 2.0 * ((fp)y / (fp)dwNum - 0.5);
         ap[0].p[dwDim] = pScale->p[dwDim];
         ap[0].IfCloseToZeroMakeZero ();  // BUGFIX - Since occasionally splitting with roundoff error

         // next
         ap[1].Copy (&ap[0]);
         ap[1].p[dwDim1] += pScale->p[dwDim1] * 2.0 / (fp)dwNum;
         ap[1].IfCloseToZeroMakeZero ();

         // next
         ap[2].Copy (&ap[1]);
         ap[2].p[dwDim2] += pScale->p[dwDim2] * 2.0 / (fp)dwNum;
         ap[2].IfCloseToZeroMakeZero ();

         // next
         ap[3].Copy (&ap[0]);
         ap[3].p[dwDim2] = ap[2].p[dwDim2];
         ap[3].IfCloseToZeroMakeZero ();

         // find or create all these points
         for (i = 0; i < 4; i++) {
            adwVert[i] = VertFind (&ap[i]);
            if (adwVert[i] == -1)
               adwVert[i] = VertAdd (&ap[i]);
         }

         // flip vertex 1 and 3 so that right direction
         DWORD dw;
         dw = adwVert[1];
         adwVert[1] = adwVert[3];
         adwVert[3] = dw;

         // create the quad
         SideAdd (dwSurface, adwVert, 4);
      } //x,y
   } // dwDim



   m_fCanBackface = TRUE;
}



/**********************************************************************************
CPolyMesh::CreatePlane - Create a plane

inputs
   DWORD    dwNum - Number of points across on each side
   PCPoint  pScale - Normally -1 to 1 sphere, but multiply bu this
returns
   none
*/
void CPolyMesh::CreatePlane (DWORD dwNum, PCPoint pScale)
{
   DWORD dwSurface = 0;    // what surface will create for

   dwNum = max(2,dwNum);   // BUGFIX

   // clear out old list
   Clear();
   SymmetrySet (0);  // no symmetry

   // number per side due to edges
   DWORD dwSide;
   DWORD x, y;
   // scratch memory for IDs
   CMem  mem;
   if (!mem.Required ((dwNum+1) * (dwNum+1) * sizeof(DWORD)))
      return;
   DWORD *padwScratch;
   padwScratch = (DWORD*) mem.p;

   // create 8 triangles
   dwSide = dwNum - 1;
   // in each triangle, calculate the points...
   for (x = 0; x <= dwSide; x++) for (y = 0; y <= dwSide; y++) {
      // calculate the point
      CPoint pLoc;
      pLoc.Zero();
      pLoc.p[0] = ((fp) x / (fp) dwSide * 2 - 1) * pScale->p[0];
      pLoc.p[1] = ((fp) y / (fp) dwSide * 2 - 1) * pScale->p[1];
      pLoc.p[2] = 0;

      // find out which point it's nearest
      DWORD dwNear;
      // not near any, so create it
      dwNear = VertAdd (&pLoc);
      padwScratch[x + (y * (dwSide+1))] = dwNear;

   }  // x and y


   // now, create all the triangles
   DWORD adwVert[4];
   for (x = 0; x < dwSide; x++) for (y = 0; y < dwSide; y++) {
      // BUGFIX - ONly one quad
      adwVert[0] = padwScratch[(x+1) + (y * (dwSide+1))];
      adwVert[1] = padwScratch[x + (y * (dwSide+1))];
      adwVert[2] = padwScratch[x + ((y+1) * (dwSide+1))];
      adwVert[3] = padwScratch[(x+1) + ((y+1) * (dwSide+1))];
      SideAdd (dwSurface, adwVert, 4);
   }  // over x and y


   m_fCanBackface = FALSE;
   SymmetrySet (0x03);  // set symmetry
}


/**********************************************************************************
CRSNeedNewText - This is aninternal function that sees if the textures are the
same from what need given the side and what expect. If not, this adds a texture to
the list.

inputs
   PCMem                pmemText - Pointer to the memory containing the texture
   PTEXTPOINT5          *ppText - Pointer to Pointer to list of texture points stored away
   DWORD                *pdwNumText - Pointer to DWORD containing Number of texture points
   DWORD                dwCurIndex - Current index into the pText list for what
                           the texture should be
   PCPMVert             pVert - Vertex that getting from
   DWORD                dwSide - Side that getting from
returns
   DWORD - New Index. If new textures are changed, ppText will be modified as will pdwNumText
*/
static DWORD CSRNeedNewText (PCMem pmemText, PTEXTPOINT5 *ppText, DWORD *pdwNumText, DWORD dwCurIndex,
                             PCPMVert pVert, DWORD dwSide)
{
   PTEXTPOINT5 pText = *ppText;
   DWORD dwNumText = *pdwNumText;

   // see what texture the side wants
   PTEXTUREPOINT ptp = pVert->TextureGet (dwSide);
   // BUGFIX - Will need to test that the xyz match up too
   if ((ptp->h == pText[dwCurIndex].hv[0]) &&
      (ptp->v == pText[dwCurIndex].hv[1]) &&
      (pVert->m_pLocSubdivide.p[0] == pText[dwCurIndex].xyz[0]) &&
      (pVert->m_pLocSubdivide.p[1] == pText[dwCurIndex].xyz[1]) &&
      (pVert->m_pLocSubdivide.p[2] == pText[dwCurIndex].xyz[2])
      )
      return dwCurIndex;   // right match, so no change

   // else, add
   DWORD dwNeed;
   dwNeed = (dwNumText + 1) * sizeof(TEXTPOINT5);
   if (!pmemText->Required (dwNeed))
      return dwCurIndex;   // error, so return same as before
   *ppText = pText = (PTEXTPOINT5) pmemText->p;
   *pdwNumText = dwNumText+1;
   pText[dwNumText].hv[0] = ptp->h;   // keep this
   pText[dwNumText].hv[1] = ptp->v;
   pText[dwNumText].xyz[0] = pVert->m_pLocSubdivide.p[0];
   pText[dwNumText].xyz[1] = pVert->m_pLocSubdivide.p[1];
   pText[dwNumText].xyz[2] = pVert->m_pLocSubdivide.p[2];
   return dwNumText;
}



/**********************************************************************************
CPolyMesh::CalcRenderSides - Given a list of sides, this figures out all the points
for the sides and adds internal lists to use for the points, textures, normals,
vertices, colors, and polygons to pass into render. No rendering is actually done.

inputs
   DWORD          *padwSides - List of side numbers
   DWORD          dwNum - Number of sides
   DWORD          dwSurface - Surface to use for all of these; this assumes
                  that all the sides are to be rendered using the same surface
   BOOL           fSmooth - If TRUE then do normal smoothing, else faceted
   DWORD          dwSpecial - From OBJECTRENDER
   PCPolyMesh     pAddTo - add the calculated list to this polygon mesh
returns
   BOOL - TRUE is success
*/
BOOL CPolyMesh::CalcRenderSides (DWORD *padwSides, DWORD dwNum, DWORD dwSurface,
                                 BOOL fSmooth, DWORD dwSpecial, PCPolyMesh pAddTo)
{
   // make sure the normals are calculated
   CalcNorm ();

   // create a list to figure out all the points used
   CMem memUsed;
   DWORD dwNeed;
   dwNeed = m_lVert.Num() * sizeof(DWORD);
   if (!memUsed.Required (dwNeed))
      return FALSE;
   DWORD *padwUsed;
   padwUsed = (DWORD*) memUsed.p;
   memset (padwUsed, 0, dwNeed);

   // loop through all the surfaces figuring out which points are needed
   DWORD i, dwPoints;
   DWORD *padwVert;
   DWORD dwTotalVert;
   WORD j;
   dwPoints = 0;
   dwTotalVert = 0;
   for (i = 0; i < dwNum; i++) {
      PCPMSide ps = SideGet (padwSides[i]);
      if (!ps)
         continue;

      padwVert = ps->VertGet();
      dwTotalVert += (DWORD) ps->m_wNumVert; // roughly 4 x # sides (assuming each side is quad)
      for (j = 0; j < ps->m_wNumVert; j++) {
         if (padwUsed[padwVert[j]])
            continue;   // point already added

         // else, add point to list
         dwPoints++;
         padwUsed[padwVert[j]] = dwPoints;   // so know which
      } // j
   } // i
   if (!dwPoints)
      return TRUE;   // nothing to add

   // allocate memory to store all the points in
   CMem mem;
   PCPoint pPoint;
   dwNeed = dwPoints * sizeof (CPoint);
   if (!mem.Required (dwNeed))
      return FALSE;
   pPoint = (PCPoint) mem.p;
   for (i = 0; i < m_lVert.Num(); i++) {
      padwUsed[i]--; // so that right index number
      if (padwUsed[i] == -1)
         continue;   // not used

      // get this vert
      PCPMVert pv;
      pv = VertGet (i);
      if (!pv)
         return FALSE;

      // write it out
      pPoint[padwUsed[i]].Copy (&pv->m_pLoc);
   } // i
   pAddTo->m_lRenderPoints.Add (pPoint, dwPoints * sizeof(CPoint));

   // go through an calculate all the normals
   DWORD dwNumNorm;
   dwNumNorm = fSmooth ? dwPoints : m_lSide.Num();
   dwNeed = dwNumNorm * sizeof (CPoint);
   if (!mem.Required (dwNeed))
      return FALSE;
   pPoint = (PCPoint) mem.p;
   if (fSmooth) {
      for (i = 0; i < m_lVert.Num(); i++) {
         if (padwUsed[i] == -1)
            continue;   // not used

         // get this vert
         PCPMVert pv;
         pv = VertGet (i);
         if (!pv)
            return FALSE;

         // write it out
         pPoint[padwUsed[i]].Copy (&pv->m_pNorm);
      } // i
   }
   else { // faceted
      for (i = 0; i < dwNum; i++) {
         PCPMSide ps = SideGet (padwSides[i]);
         if (!ps)
            return FALSE;
         pPoint[i].Copy (&ps->m_pNorm);
      }
   }
   pAddTo->m_lRenderNorms.Add (pPoint, dwNumNorm * sizeof(CPoint));

   // allocate enough memory for the texturepoints and add them in
   DWORD dwNumText;
   dwNumText = dwPoints;
   dwNeed = (dwNumText * 3 / 2) * sizeof(TEXTPOINT5);
   if (!mem.Required (dwNeed))
      return FALSE;
   PTEXTPOINT5 ptp;
   ptp = (PTEXTPOINT5) mem.p;
   for (i = 0; i < m_lVert.Num(); i++) {
      if (padwUsed[i] == -1)
         continue;   // not used

      // get this vert
      PCPMVert pv;
      pv = VertGet (i);
      if (!pv)
         return FALSE;

      // write it out
      ptp[padwUsed[i]].hv[0] = pv->m_tText.h;
      ptp[padwUsed[i]].hv[1] = pv->m_tText.v;
      ptp[padwUsed[i]].xyz[0] = pv->m_pLocSubdivide.p[0];
      ptp[padwUsed[i]].xyz[1] = pv->m_pLocSubdivide.p[1];
      ptp[padwUsed[i]].xyz[2] = pv->m_pLocSubdivide.p[2];
   } // i
   // done add to list yet

   // allocate memory for all the colors...
   DWORD dwNumColor;
   dwNumColor = ((dwSpecial == ORSPECIAL_POLYMESH_EDITMORPH) || (dwSpecial == ORSPECIAL_POLYMESH_EDITBONE)) ?
         dwPoints : 0;
   dwNeed = dwNumColor * sizeof(COLORREF);
   if (!mem.Required (dwNeed))
      return FALSE;
   COLORREF *pcr = (COLORREF*)mem.p;
   WORD aw[3];
   if (dwNumColor) for (i = 0; i < m_lVert.Num(); i++) {
      if (padwUsed[i] == -1)
         continue;   // not used

      // get this vert
      PCPMVert pv;
      pv = VertGet (i);
      if (!pv)
         return FALSE;

      // write it out
      aw[0] = (WORD)(min((fp)0xffff, pv->m_pLocSubdivide.p[0]));
      aw[1] = (WORD)(min((fp)0xffff, pv->m_pLocSubdivide.p[1]));
      aw[2] = (WORD)(min((fp)0xffff, pv->m_pLocSubdivide.p[2]));

      pcr[padwUsed[i]] = UnGamma (aw);
   } // i
   pAddTo->m_lRenderColor.Add (pcr, dwNumColor * sizeof(COLORREF));

   // allocate enough memory for vertices
   DWORD dwNumVert;
   DWORD dwCur;
   dwNumVert = fSmooth ? dwPoints : dwTotalVert;
   dwNeed = (fSmooth ? (dwPoints * 3 / 2) : dwTotalVert) * sizeof(VERTEX);
   CMem memVert;
   if (!memVert.Required (dwNeed))
      return FALSE;
   PVERTEX pVert;
   pVert = (PVERTEX) memVert.p;
   if (fSmooth) {
      for (i = 0; i < dwNumVert; i++) {
         pVert[i].dwNormal = i;
         pVert[i].dwPoint = i;
         pVert[i].dwTexture = i;
         pVert[i].dwColor = dwNumColor ? i : -1;
      }
   }
   else { // faceted
      dwCur = 0;
      for (i = 0; i < dwNum; i++) {
         PCPMSide ps = SideGet (padwSides[i]);
         if (!ps)
            continue;

         padwVert = ps->VertGet();
         for (j = 0; j < ps->m_wNumVert; j++, dwCur++) {
            pVert[dwCur].dwNormal = i;
            pVert[dwCur].dwPoint = padwUsed[padwVert[j]];
            pVert[dwCur].dwTexture = CSRNeedNewText (&mem, &ptp, &dwNumText,
               padwUsed[padwVert[j]], VertGet(padwVert[j]), padwSides[i]);
            pVert[dwCur].dwColor = dwNumColor ? pVert[dwCur].dwPoint : -1;
         } // j
      } // i
   }
   // NOTE: Not saving this yet


   dwCur = 0;  // reset

   // create enough memory for all the polygons
   dwNeed = dwNum * sizeof(POLYDESCRIPT) + dwTotalVert * sizeof(DWORD);
   CMem memPoly;
   if (!memPoly.Required (dwNeed))
      return FALSE;
   PPOLYDESCRIPT pPoly, pPolyOrig;
   pPolyOrig = pPoly = (PPOLYDESCRIPT) memPoly.p;
   BOOL fUseIDPart = (dwSpecial == ORSPECIAL_POLYMESH_EDITPOLY) ||
      (dwSpecial == ORSPECIAL_POLYMESH_EDITMORPH) ||
      (dwSpecial == ORSPECIAL_POLYMESH_EDITBONE);
   for (i = 0; i < dwNum; i++, pPoly = (PPOLYDESCRIPT)((DWORD*)(pPoly+1) + pPoly->wNumVertices)) {
      PCPMSide ps = SideGet (padwSides[i]);
      if (!ps)
         return FALSE;

      pPoly->dwIDPart = fUseIDPart ? ps->m_dwOrigSide : 0;
      pPoly->wNumVertices = ps->m_wNumVert;

      DWORD *padw;
      padw = (DWORD*) (pPoly+1);
      
      padwVert = ps->VertGet();
      for (j = 0; j < ps->m_wNumVert; j++, dwCur++) {
         DWORD dwVert = padwVert[j];
         PCPMVert pv = VertGet(dwVert);

         if (fSmooth) {
            padw[j] = padwUsed[dwVert];

            // since may need new side check this out
            DWORD dwNeedText;
            dwNeedText = CSRNeedNewText (&mem, &ptp, &dwNumText, pVert[padw[j]].dwTexture,
               pv, padwSides[i]);

            if (dwNeedText != pVert[padw[j]].dwTexture) {
               // if get here need to add a new vertex that will have a different texture
               // than what currently using
               dwNeed = (dwNumVert + 1) * sizeof(VERTEX);
               if (!memVert.Required (dwNeed))
                  return FALSE;   // error
               pVert = (PVERTEX) memVert.p;
               pVert[dwNumVert] = pVert[padw[j]];
               pVert[dwNumVert].dwTexture = dwNeedText;
               padw[j] = dwNumVert;
               dwNumVert++;
            }
         }
         else { // facet
            padw[j] = dwCur;
            // Dont worry about different texture here because will have calculated
            // this earlier
         }

      } // j
   } // i

   // now that have calculated all this, save it...
   pAddTo->m_lRenderSurface.Add (&dwSurface);
   pAddTo->m_lRenderText.Add (ptp, dwNumText * sizeof(TEXTPOINT5));
   pAddTo->m_lRenderVertex.Add (pVert, dwNumVert * sizeof(VERTEX));
   pAddTo->m_lRenderPoly.Add (pPolyOrig, (DWORD)((PBYTE)pPoly - (PBYTE)pPolyOrig));

   return TRUE;
}


/**********************************************************************************
CPolyMesh::CalcRender - This calculates what will need to be rendered. It splits
the polymesh into several sub-groups (by surface and by bone that affects) so that
it can draw several surfaces and at the same time (because of bone segregation) will
draw faster when ray trace.

inputs
   BOOL           fSmooth - If TRUE then do normal smoothing, else faceted
   DWORD          dwSpecial - From OBJECTRENDER
   PCPolyMesh     pAddTo - add the calculated lists to this polygon mesh.
                  Note that the mesh drawing info is first cleared.
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::CalcRender (BOOL fSmooth, DWORD dwSpecial, PCPolyMesh pAddTo)
{
   // clear out
   pAddTo->m_lRenderSurface.Clear();
   pAddTo->m_lRenderPoints.Clear();
   pAddTo->m_lRenderNorms.Clear();
   pAddTo->m_lRenderText.Clear();
   pAddTo->m_lRenderVertex.Clear();
   pAddTo->m_lRenderPoly.Clear();
   pAddTo->m_lRenderColor.Clear();

   // make a list of 3-dwords per side, the first one is a combiantion of the
   // surface and the bone, second is a quadrant number (and other flags),
   // the third is the index
   CMem mem;
   if (!mem.Required (sizeof(DWORD)*3*m_lSide.Num()))
      return FALSE;
   DWORD *padw;
   padw = (DWORD*) mem.p;
   DWORD i, dwBone;
   PBONEWEIGHT pbw;
   DWORD *padwVert;
   PCPMVert pv;
   DWORD dwNum;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   BOOL fSplit = (m_lSide.Num() > 100);
   dwNum = m_lSide.Num();
   if (!dwNum)
      return TRUE;   // no points
   for (i = 0; i < dwNum; i++) {
      PCPMSide ps = SideGet (i);
      if (!ps)
         continue;

      padw[i*3+1] = 0;
      padw[i*3+2] = i;

      // get one vertex and use that to determine which bone this belongs
      // to. Group by bones so that will ray trace faster
      dwBone = -1;
      padwVert = ps->VertGet();
      if (ps->m_wNumVert) {
         pv = ppv[padwVert[0]];
         pbw = pv->BoneWeightGet();
         if (pbw && pv->m_wNumBoneWeight)
            dwBone = pbw[0].dwBone;

         if (fSplit) {
            // if there are enough points, then split the object into quadrants
            // so that when ray trace it will render a bit quicker
            if (pv->m_pLoc.p[0] < 0)
               padw[i*3+1] |= 0x01;
            if (pv->m_pLoc.p[1] < 0)
               padw[i*3+1] |= 0x02;
            if (pv->m_pLoc.p[2] < 0)
               padw[i*3+1] |= 0x04;
         }
      }

      // set based on surface and bone
      padw[i*3+0] = MAKELONG((WORD)ps->m_dwSurfaceText, (WORD)dwBone);
   }


   // sort this
   qsort (padw, dwNum, sizeof(DWORD)*3, B2DWORDCompare);

   // loop through and find chunks
   DWORD adwID[2], dwStart, dwEnd, dwSurface, j;
   PCPMSide ps;
   adwID[0] = padw[0];
   adwID[1] = padw[1];
   for (i = 0; i < dwNum; ) {
      // find a contiguous chunk of similarly labled sides
      dwStart = i;
      for (; (i < dwNum) && (padw[i*3+0] == adwID[0]) && (padw[i*3+1] == adwID[1]); i++);
      dwEnd = i - dwStart;
      if (i < dwNum) {
         adwID[0] = padw[i*3+0];
         adwID[1] = padw[i*3+1];
      }

      // put thse into a list, reuse the start of the list to save memory
      // can do this without any problem over overwriting memory that's going to be used
      for (j = 0; j < dwEnd; j++)
         padw[j] = padw[(dwStart+j)*3+2];

      ps = SideGet (padw[0]);
      if (!ps)
         return FALSE;
      dwSurface = ps->m_dwSurfaceText; // know that they will all have the same surface
      if (!CalcRenderSides (padw, dwEnd, dwSurface, fSmooth, dwSpecial, pAddTo))
         return FALSE;

   }; // i

   return TRUE;
}


/**********************************************************************************
CPolyMesh::CalcMorphColors - This is called if dwSpecial is set to
ORSPECIAL_POLYMESH_EDITMORPH. If so, then the m_pLocSubdivide will be filled with
the RGB values to be displayed.

inputs
   PCPolyMesh     pUse - Polymesh being modified, just cloned from the current polygon
                  mesh so there's a 1:1 corresponence of the vertices
returns
   none
*/
void CPolyMesh::CalcMorphColors (PCPolyMesh pUse)
{
   // find the maximum length
   fp fMaxLen = 0;
   fp f, fLen;
   DWORD i, j;
   PCPMVert *ppo = (PCPMVert*) m_lVert.Get(0);
   PCPMVert *ppn = (PCPMVert*) pUse->m_lVert.Get(0);
   BOOL fUseAll = (m_lMorphRemapID.Num() != 1);
   DWORD dwUse = fUseAll ? 0 : *((DWORD*)m_lMorphRemapID.Get(0));
   for (i = 0; i < m_lVert.Num(); i++) {
      PCPMVert pvo = ppo[i];
      PCPMVert pvn = ppn[i];

      // zero out subdivide while here
      pvn->m_pLocSubdivide.Zero();

      PVERTDEFORM pvd = pvo->VertDeformGet ();
      fLen = 0;
      for (j = 0; j < pvo->m_wNumVertDeform; j++) {
         // if only looking at a specific deform...
         if (!fUseAll && (pvd[j].dwDeform != dwUse))
            continue;

         // length
         f = pvd[j].pDeform.Length();
         fMaxLen = max(fMaxLen, fLen);
         fLen += f;
      }
      fMaxLen = max(fMaxLen, fLen);
   } // i

   // if no length then leave 0
   if (fMaxLen < CLOSE)
      return;

   // go through again and figure out color
   COLORREF cr;
   WORD aw[3];
   for (i = 0; i < m_lVert.Num(); i++) {
      PCPMVert pvo = ppo[i];
      PCPMVert pvn = ppn[i];
      PVERTDEFORM pvd = pvo->VertDeformGet ();
      for (j = 0; j < pvo->m_wNumVertDeform; j++) {
         // if only looking at a specific deform...
         if (!fUseAll && (pvd[j].dwDeform != dwUse))
            continue;

         // length
         f = pvd[j].pDeform.Length() / fMaxLen;

         // get the color
         cr = MapColorPicker (pvd[j].dwDeform);
         Gamma (cr, aw);

         // add
         pvn->m_pLocSubdivide.p[0] += (fp)aw[0] * f;
         pvn->m_pLocSubdivide.p[1] += (fp)aw[1] * f;
         pvn->m_pLocSubdivide.p[2] += (fp)aw[2] * f;
      }
   } // i
}


/**********************************************************************************
CPolyMesh::CalcRender - This CalcRender() looks to see if the m_fDirtyRender flag
is set. If it is, then it subdivides the current polymesh (cloning it as it goes along)
to conform to all the deformations and bones. The final result then has its polygons
rendered into this object.

NOTE: This goes through the original points and fills in m_pLocSubdivide with the location
of the main point after all the subdivision has happened. It also fills in m_pNorm
with the normal at the subdivision.

inputs
   DWORD          dwSpecial - From OBJECTRENDER
   PCObjectBone   pBone - Bone to use for deformations, gotten from BoneGet()
   PCMatrix       pmObjectToWorld - Used if applying bones.
*/
void CPolyMesh::CalcRender (DWORD dwSpecial, PCObjectBone pBone, PCMatrix pmObjectToWorld)
{
   // if the special has changed then dirty the render
   if (dwSpecial != m_dwSpecialLast) {
      m_fDirtyRender = TRUE;
      m_dwSpecialLast = dwSpecial;
   }

   if (!m_fDirtyRender)
      return;  // not dirty

   PCPolyMesh pUse = this;

   m_fBonesAlreadyApplied = FALSE;  // assume no bones
   m_fBoneChangeShape = FALSE;   // assume hasn't changed the shape

   // set original side
   DWORD i;
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   for (i = 0; i < m_lSide.Num(); i++)
      pps[i]->m_dwOrigSide = i;

   // loop through all the vertices and set m_pLocSubdivide to m_pLoc since this
   // will be used by volumetric textures. Need to do this before apply morphs
   // and bones or volumetric textures will bend
   PCPMVert *ppo;
   PCPMVert *ppn;
   ppo = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < m_lVert.Num(); i++)
      ppo[i]->m_pLocSubdivide.Copy (&ppo[i]->m_pLoc);

   // calculate the morp colors
   if (dwSpecial == ORSPECIAL_POLYMESH_EDITMORPH) {
      // BUGFIX - Must have pUse or will lose coloration info
      // if still modifying this one, then make sure to clone this
      if (pUse == this)
         pUse = Clone(FALSE);
      if (!pUse)
         return;

      CalcMorphColors (pUse);
   }
   else if (dwSpecial == ORSPECIAL_POLYMESH_EDITBONE) {
      // BUGFIX - Must have pUse or will lose coloration info
      // if still modifying this one, then make sure to clone this
      if (pUse == this)
         pUse = Clone(FALSE);
      if (!pUse)
         return;

      CalcBoneColors (pUse);
   }

   // Need to apply deformations
   if (m_lMorphRemapID.Num()) {
      // if still modifying this one, then make sure to clone this
      if (pUse == this)
         pUse = Clone(FALSE);
      if (!pUse)
         return;

      // since know that vertex numbers are the same, go through and calculate
      // the location on the original and tranfer straight over
      ppo = (PCPMVert*) m_lVert.Get(0);
      ppn = (PCPMVert*) pUse->m_lVert.Get(0);
      DWORD* padwID = (DWORD*)m_lMorphRemapID.Get(0);
      fp* pafVal = (fp*)m_lMorphRemapValue.Get(0);
      DWORD dwNum = m_lMorphRemapID.Num();
      for (i = 0; i < m_lVert.Num(); i++) {
         ppo[i]->DeformCalc (dwNum, padwID, pafVal, &ppn[i]->m_pLoc);
      } // i
   }


   // Need to apply bones
   if (pBone) {
      // if still modifying this one, then make sure to clone this
      if (pUse == this)
         pUse = Clone(FALSE);
      if (!pUse)
         return;

      BoneApplyToRender (pUse, pBone, pmObjectToWorld);
   }

   // Need to sudivide
   if ((m_dwSubdivide != -1) && (m_dwSubdivide > 0)) {
      // if still modifying this one, then make sure to clone this
      if (pUse == this)
         pUse = Clone(FALSE);
      if (!pUse)
         return;

      for (i = 0; i < m_dwSubdivide; i++) {
         // subdivide
         if (!pUse->Subdivide (FALSE)) {
            delete pUse;
            return;
         }
      } // i
   }

   // fill in m_pLocSubdivide and m_pNorm
   pUse->CalcNorm();
   ppo = (PCPMVert*) m_lVert.Get(0);
   ppn = (PCPMVert*) pUse->m_lVert.Get(0);
   if ((m_lVert.Num() <= pUse->m_lVert.Num())) {   // BUGFIX - Was <, need to be <=
      for (i = 0; i < m_lVert.Num(); i++) {
         ppo[i]->m_pLocSubdivide.Copy (&ppn[i]->m_pLoc);
         ppo[i]->m_pNormSubdivide.Copy (&ppn[i]->m_pNorm);

         // BUGFIX - Check for differnt objects here
         // BUGFIX - Dont copy over because added m_pNormSubdivide
         //if (ppo != ppn)
         //   ppo[i]->m_pNorm.Copy (&ppn[i]->m_pNorm);
      } // i
   }

   pUse->CalcRender (m_dwSubdivide != -1, dwSpecial, this);


   if (pUse != this)
      delete pUse;
   m_fDirtyRender = FALSE;
}


/**********************************************************************************
CPolyMesh::Render - Tells the polygon mesh to render.

inputs
   POBJECTRENDER        pr - Used for rendering
   PCRenderSurface      prs - Render to this
   PCObjectTemplate     pot - Object template, called to get the textures from
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::Render (POBJECTRENDER pr, PCRenderSurface prs, PCObjectTemplate pot)
{
   // Need to see if the bones have changed at all. If they have then
   // get the new bone values and recalc
   BOOL fHasBoneMorph;
   PCObjectBone pBone = BoneSeeIfRenderDirty (pot->m_pWorld, &fHasBoneMorph);

   // if it has a bone morph then need to recalc attributes
   if (fHasBoneMorph)
      CalcMorphRemap();

   CalcRender (pr->dwSpecial, pBone, &pot->m_MatrixObject);

   OSINFO info;
   pot->InfoGet(&info);
   DWORD dwRenderShard = info.dwRenderShard;

   // loop and draw
   DWORD i, j, dwSize, dwPoints, dwNorm, dwText, dwVert;
   DWORD *padwSurface;
   PCPoint pSrc, pPoint, pNorm;
   PTEXTPOINT5 ptSrc, pText;
   PVERTEX pvSrc, pVert;
   PPOLYDESCRIPT pPolyCur, pPolyMax, pPolyNew;
   padwSurface = (DWORD*) m_lRenderSurface.Get(0);
   prs->m_fBonesAlreadyApplied = m_fBonesAlreadyApplied;
   for (i = 0; i < m_lRenderSurface.Num(); i++) {
      prs->Commit();

      // set the color
      // if we're using colors then doing something like displaying morph/bone
      // weighting, so bypass the normal material
      if (m_lRenderColor.Size(i)) {
         RENDERSURFACE Mat;
         memset (&Mat, 0, sizeof(Mat));
         Mat.Material.InitFromID (MATERIAL_PAINTSEMIGLOSS);
         prs->SetDefMaterial (&Mat);
      }
      else
         prs->SetDefMaterial (dwRenderShard, pot->ObjectSurfaceFind (padwSurface[i]), pot->m_pWorld);

      // points
      pSrc = (PCPoint) m_lRenderPoints.Get(i);
      if (!pSrc)
         continue;
      dwSize = (DWORD)m_lRenderPoints.Size(i) / sizeof(CPoint);
      pPoint = prs->NewPoints (&dwPoints, dwSize);
      if (!pPoint)
         continue;
      memcpy (pPoint, pSrc, dwSize * sizeof(CPoint));

      // normals
      pSrc = (PCPoint) m_lRenderNorms.Get(i);
      if (!pSrc)
         continue;
      dwSize = (DWORD)m_lRenderNorms.Size(i) / sizeof(CPoint);
      dwNorm = 0;
      pNorm = prs->NewNormals (TRUE, &dwNorm, dwSize);
      if (pNorm)
         memcpy (pNorm, pSrc, dwSize * sizeof(CPoint));

      // texutre
      ptSrc = (PTEXTPOINT5) m_lRenderText.Get(i);
      if (!ptSrc)
         continue;
      dwSize = (DWORD)m_lRenderText.Size(i) / sizeof(TEXTPOINT5);
      dwText = 0;
      pText = prs->NewTextures (&dwText, dwSize);
      if (pText) {
         memcpy (pText, ptSrc, dwSize * sizeof(TEXTPOINT5));

         // only scale if in meters
         if (SurfaceInMetersGet(padwSurface[i]))
            prs->ApplyTextureRotation (pText, dwSize); // BUGFIX - need to scale properly
      }

      // colors
      COLORREF *pcr = (COLORREF*) m_lRenderColor.Get(i);
      dwSize = (DWORD)m_lRenderColor.Size(i) / sizeof(COLORREF);
      DWORD dwColor = 0;
      COLORREF *pcrUse;
      pcrUse = dwSize ? prs->NewColors (&dwColor, dwSize) : NULL;
      if (pcrUse)
         memcpy (pcrUse, pcr, dwSize * sizeof(COLORREF));

      // vertices
      pvSrc = (PVERTEX) m_lRenderVertex.Get(i);
      if (!pvSrc)
         continue;
      dwSize = (DWORD)m_lRenderVertex.Size(i) / sizeof(VERTEX);
      dwVert = 0;
      pVert = prs->NewVertices (&dwVert, dwSize);
      if (!pVert)
         continue;
      DWORD dwDefColor;
      dwDefColor = prs->DefColor ();
      for (j = 0; j < dwSize; j++) {
         pVert[j].dwColor = ((pvSrc[j].dwColor == -1) || !pcrUse) ? dwDefColor :
               (pvSrc[j].dwColor + dwColor);
         pVert[j].dwNormal = pNorm ? (pvSrc[j].dwNormal + dwNorm) : 0;
         pVert[j].dwPoint = pvSrc[j].dwPoint + dwPoints;
         pVert[j].dwTexture = pText ? (pvSrc[j].dwTexture + dwText) : 0;
      }

      // polygons
      DWORD dwSurface;
      dwSurface = prs->DefSurface ();
      pPolyCur = (PPOLYDESCRIPT) m_lRenderPoly.Get(i);
      if (!pPolyCur)
         continue;
      pPolyMax = (PPOLYDESCRIPT) ((PBYTE) pPolyCur + m_lRenderPoly.Size(i));
      for (; pPolyCur < pPolyMax; pPolyCur = (PPOLYDESCRIPT)((DWORD*)(pPolyCur+1) + pPolyCur->wNumVertices)) {
         pPolyNew = prs->NewPolygon (pPolyCur->wNumVertices);
         if (!pPolyNew)
            continue;
         pPolyNew->dwSurface = dwSurface;
         pPolyNew->fCanBackfaceCull = m_fCanBackface;
         pPolyNew->dwIDPart = pPolyCur->dwIDPart;

         DWORD *padwSrc, *padwDest;
         padwSrc = (DWORD*)(pPolyCur+1);
         padwDest = (DWORD*)(pPolyNew+1);
         for (j = 0; j < pPolyCur->wNumVertices; j++)
            padwDest[j] = padwSrc[j] + dwVert;
      } // ppolycur
   } // i

   prs->Commit();
   return TRUE;
}


/**********************************************************************************
CPolyMesh::QueryBoundingBox - Standard API
*/
void CPolyMesh::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, PCObjectTemplate pot)
{
   pCorner1->Zero();
   pCorner2->Zero();
   BOOL fSet = FALSE;

   // Need to see if the bones have changed at all. If they have then
   // get the new bone values and recalc
   BOOL fHasBoneMorph;
   PCObjectBone pBone = BoneSeeIfRenderDirty (pot->m_pWorld, &fHasBoneMorph);

   // if it has a bone morph then need to recalc attributes
   if (fHasBoneMorph)
      CalcMorphRemap();

   CalcRender (0 /*pr->dwSpecial*/, pBone, &pot->m_MatrixObject);

   // loop and draw
   DWORD i, j, dwSize;
   PCPoint pSrc;
   // prs->m_fBonesAlreadyApplied = m_fBonesAlreadyApplied;
   for (i = 0; i < m_lRenderSurface.Num(); i++) {
      // points
      pSrc = (PCPoint) m_lRenderPoints.Get(i);
      if (!pSrc)
         continue;
      dwSize = (DWORD)m_lRenderPoints.Size(i) / sizeof(CPoint);

      for (j = 0; j < dwSize; j++, pSrc++) {
         if (fSet) {
            pCorner1->Min(pSrc);
            pCorner2->Max(pSrc);
         }
         else {
            pCorner1->Copy(pSrc);
            pCorner2->Copy(pSrc);
            fSet = TRUE;
         }
      } // j

   } // i
}



/*****************************************************************************************
CPolyMesh::SelModeSet - Set the selection mode. If the new selection mode is the
same as the old then nothing is changed. Otherwise, the selection information will be
lost, since can only have points, edges, or sides selected at once, exclusively.

inputs
   DWORD       dwMode - 0 for points, 1 for edge, 2 for side
returns
   none
*/
void CPolyMesh::SelModeSet (DWORD dwMode)
{
   if (dwMode == m_dwSelMode)
      return;  // no change

   m_dwSelMode = dwMode;
   m_fSelSorted = TRUE;
   m_lSel.Init (sizeof(DWORD) * ((m_dwSelMode == 1) ? 2 : 1));
}

/*****************************************************************************************
CPolyMesh::SelModeGet - Returns the current selection mode. See SelModeSet()

returns
   DWORD - Current mode, 0 for points, 1 for edge, 2 for side
*/
DWORD CPolyMesh::SelModeGet (void)
{
   return m_dwSelMode;
}


/*****************************************************************************************
CPolyMesh::SelAdd - Adds a new selection. NOTE: The meaning of what is added will
depend upon the current selection mode.

inputs
   DWORD          dwIndex - Object index added. If the selection mode is a vertex it's
                     the vertex index. If it's a side it's the side index. If it's
                     an edge it's one of the vertex indecies of the edge.
   DWORD          dwIndex2 - Used for edge mode only. This is the second vertex index.
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::SelAdd (DWORD dwIndex,  DWORD dwIndex2)
{
   if (m_lSel.Num())
      m_fSelSorted = FALSE;

   if (m_dwSelMode == 1) {
      DWORD adw[2];
      adw[0] = min(dwIndex, dwIndex2);
      adw[1] = max(dwIndex, dwIndex2);
      m_lSel.Add (adw);
   }
   else
      m_lSel.Add (&dwIndex);

   return TRUE;
}

/*****************************************************************************************
CPolyMesh::SelRemove - Removes a specific selection from the list.

inputs
   DWORD          dwIndex - Index into the LIST returnd by SelEnum()
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::SelRemove (DWORD dwIndex)
{
   return m_lSel.Remove (dwIndex);
}

/*****************************************************************************************
CPolyMesh::SelClear - Clears all the items from the selection.
*/
void CPolyMesh::SelClear (void)
{
   m_lSel.Clear();
}


/*****************************************************************************************
CPolyMesh::SelAll - Selects everything

inputs
   BOOL           fSelAllAsymmetrical - If TRUE then select asymmetrical objects only
*/
void CPolyMesh::SelAll (BOOL fSelAllAsymmetrical)
{
   // clear first
   m_lSel.Clear();

   // if select all symmetrical then recalc symmetry just in case wasn't calculated
   if (fSelAllAsymmetrical)
      SymmetryRecalc();


   DWORD i;
   switch (m_dwSelMode) {
   case 0: // vert
      {
         PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
         for (i = 0; i < m_lVert.Num(); i++) {
            if (fSelAllAsymmetrical && ppv[i]->HasAMirror(m_dwSymmetry))
               continue;
            m_lSel.Add (&i);
         }
         m_fSelSorted = TRUE;
      }
      break;

   case 1: // edge
      {
         CalcEdges ();
         PCPMEdge *ppe = (PCPMEdge*) m_lEdge.Get(0);
         PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
         for (i = 0; i < m_lEdge.Num(); i++) {
            if (fSelAllAsymmetrical && ppe[i]->HasAMirror (m_dwSymmetry, ppv))
               continue;
            m_lSel.Add (&ppe[i]->m_adwVert);
         }
         m_fSelSorted = FALSE;
      }
      break;

   case 2: // side
      {
         PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
         PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
         for (i = 0; i < m_lSide.Num(); i++) {
            if (fSelAllAsymmetrical && pps[i]->HasAMirror(m_dwSymmetry, ppv))
               continue;
            m_lSel.Add (&i);
         }
         m_fSelSorted = TRUE;
      }
      break;

   }
}


/*****************************************************************************************
CPolyMesh::SelSort - Sorts the selection list if it's not already sorted. (It can get
unsorted after an add or rename.) The sorting orders the selection list so that
indecies go from low to high.
*/
void CPolyMesh::SelSort (void)
{
   if (m_fSelSorted)
      return;
   m_fSelSorted = TRUE;

   if (m_dwSelMode == 1)
      qsort (m_lSel.Get(0), m_lSel.Num(), sizeof(DWORD)*2, B2DWORDCompare);
   else
      qsort (m_lSel.Get(0), m_lSel.Num(), sizeof(DWORD), BDWORDCompare);
}


/*****************************************************************************************
CPolyMesh::SelEnum - Returns a pointer to the CListFixed that contains the selection.
The list is an array of DWORDs (for vertex and side mode) or 2 DWORDs (for edge mode).
They indicate the indecies passed in by SelAdd().

You can browse the list, but don't change it otherwise. The list is not necessarily
sorted. To guarantee it's sorted use SelSort() before calling SelEnum().

inputs
   DWORD       *pdwNum - Filled in with the number of slements
returns
   DWORD * - Pointer to the start of the list
*/
DWORD *CPolyMesh::SelEnum (DWORD *pdwNum)
{
   if (pdwNum)
      *pdwNum = m_lSel.Num();
   return (DWORD*) m_lSel.Get(0);
}


/*****************************************************************************************
CPolyMesh::SelFind - Given a vertex, edge, or side, this finds its index into the
list (returned by calling SelEnum()).

inputs
   DWORD       dwIndex - Vertex number, or side number. If the current mode is for
                  edges then this is one of the vertex numbers
   DWORD       dwIndex2 - Only used if in edge mode. This is the second vertex of the edge
returns
   DWORD - Index into the list returned by SelEnum(), or -1 if can't find
*/
DWORD CPolyMesh::SelFind (DWORD dwIndex, DWORD dwIndex2)
{
   // make sure to sort before finding
   SelSort ();

   if (m_dwSelMode == 1) {
      DWORD adw[2];
      adw[0] = min(dwIndex, dwIndex2);
      adw[1] = max(dwIndex, dwIndex2);

      DWORD *pdw;
      pdw = (DWORD*) bsearch (adw, m_lSel.Get(0), m_lSel.Num(), 2*sizeof(DWORD), B2DWORDCompare);
      if (!pdw)
         return -1;
      return (DWORD) ((PBYTE) pdw - (PBYTE) m_lSel.Get(0)) / (2 * sizeof(DWORD));
   }
   else
      return DWORDSearch (dwIndex, m_lSel.Num(), (DWORD*) m_lSel.Get(0));
}


/*****************************************************************************************
CPolyMesh::SelVertRemoved - Call this when a vertex in the polymesh is deleted. This
will remove it if it also exists in the selection. Alternatively, if there are any
vertices with numbers higher than the removed, they will have their numbers reduced.

This affects vertex and edge selection mode. It's ignored for side selection mode

inputs
   DWORD       dwVert - Vertex number removed
*/
void CPolyMesh::SelVertRemoved (DWORD dwVert)
{
   // if it's not a vert or edge dont care
   if ((m_dwSelMode != 0) && (m_dwSelMode != 1))
      return;

   // search through
   DWORD dwNum, *padw;
   DWORD dwStart;
   dwStart = 0;
restart:
   dwNum = m_lSel.Num() * ((m_dwSelMode == 1) ? 2 : 1);
   padw = (DWORD*) m_lSel.Get(0);
   DWORD i;

   for (i = dwStart; i < dwNum; i++,padw++) {
      if (*padw < dwVert)
         continue;
      if (*padw == dwVert) {
         m_lSel.Remove ((m_dwSelMode == 1) ? (i/2) : i);
         dwStart = i;
         goto restart;
      }
      
      // else, decrease
      (*padw)--;
   }
}


/*****************************************************************************************
CPolyMesh::SelVertRenamed - Call this when one or more vertices are renamed. It
acts just like the other rename calls.

inputs
   DWORD       dwNum - Number of items in the rename list
   DWORD       *padwOrig - Array of dwNum original vertex numbers. THIS LIST MUST BE SORTED
   DWORD       *padwNew - Array of dwNum new vertex numbers
returns
*/
void CPolyMesh::SelVertRenamed (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   // if it's not a vert or edge dont care
   if ((m_dwSelMode != 0) && (m_dwSelMode != 1))
      return;

   m_fSelSorted = FALSE;

   // first pass, go through and rename
   DWORD dwNumChange, *padw;
   dwNumChange = m_lSel.Num() * ((m_dwSelMode == 1) ? 2 : 1);
   padw = (DWORD*) m_lSel.Get(0);
   DWORD i, dwFind;
   for (i = 0; i < dwNumChange; i++, padw++) {
      dwFind = DWORDSearch (*padw, dwNum, padwOrig);
      if (dwFind == -1)
         continue;
      *padw = padwNew[dwFind];
   }

   // second pass, go through and delete
   padw = (DWORD*) m_lSel.Get(0);
   for (i = m_lSel.Num()-1; i < m_lSel.Num(); i--) {
      if (m_dwSelMode == 1) {
         // edge

         // reorder
         if (padw[i*2]> padw[i*2+1]) {
            dwFind = padw[i*2];
            padw[i*2] = padw[i*2+1];
            padw[i*2+1] = dwFind;
         }

         // BUGFIX - If two of the same then need to delete
         if (padw[i*2] == padw[i*2+1])
            padw[i*2] = padw[i*2+1] = -1;

         // see if no change
         if ((padw[i*2] != -1) && (padw[i*2+1] != -1))
            continue;   // nothing changed
      }
      else {// vert
         if (padw[i] != -1)
            continue;   // nothing changed
      }

      // if get here then delete
      m_lSel.Remove(i);
      padw = (DWORD*) m_lSel.Get(0);
   }

   SelSort ();

   // go through and get rid of duplicates
   padw = (DWORD*) m_lSel.Get(0);
   for (i = m_lSel.Num()-1; i < m_lSel.Num(); i--) {
      if (!i)
         break;   // no point because nothing below
      if (m_dwSelMode == 1) {
         if ((padw[i*2+0] != padw[(i-1)*2+0]) || (padw[i*2+1] != padw[(i-1)*2+1]))
            continue;
      }
      else {// vert
         if (padw[i] != padw[i-1])
            continue;   // nothing changed
      }

      // if get here then delete
      m_lSel.Remove(i);
      padw = (DWORD*) m_lSel.Get(0);
   }

}


/*****************************************************************************************
CPolyMesh::SelSideRemoved - Call this when a Side in the polymesh is deleted. This
will remove it if it also exists in the selection. Alternatively, if there are any
sides with numbers higher than the removed, they will have their numbers reduced.

This affects side selection mode. It's ignored for vertex and edge selection mode

inputs
   DWORD       dwSide - Side number removed
*/
void CPolyMesh::SelSideRemoved (DWORD dwSide)
{
   // if it's not a side dont care
   if (m_dwSelMode != 2)
      return;

   // search through
   DWORD dwNum, *padw;
   DWORD dwStart;
   dwStart = 0;
restart:
   dwNum = m_lSel.Num();
   padw = (DWORD*) m_lSel.Get(0);
   DWORD i;

   for (i = dwStart; i < dwNum; i++,padw++) {
      if (*padw < dwSide)
         continue;
      if (*padw == dwSide) {
         m_lSel.Remove (i);
         dwStart = i;
         goto restart;
      }
      
      // else, decrease
      (*padw)--;
   }
}


/*****************************************************************************************
CPolyMesh::SelSideRenamed - Call this when one or more sides are renamed. It
acts just like the other rename calls.

inputs
   DWORD       dwNum - Number of items in the rename list
   DWORD       *padwOrig - Array of dwNum original side numbers. THIS LIST MUST BE SORTED
   DWORD       *padwNew - Array of dwNum new side numbers
returns
*/
void CPolyMesh::SelSideRenamed (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   // if it's not a side dont care
   if (m_dwSelMode != 2)
      return;

   m_fSelSorted = FALSE;

   // first pass, go through and rename
   DWORD dwNumChange, *padw;
   dwNumChange = m_lSel.Num();
   padw = (DWORD*) m_lSel.Get(0);
   DWORD i, dwFind;
   for (i = 0; i < dwNumChange; i++, padw++) {
      dwFind = DWORDSearch (*padw, dwNum, padwOrig);
      if (dwFind == -1)
         continue;
      *padw = padwNew[dwFind];
   }

   // second pass, go through and delete
   padw = (DWORD*) m_lSel.Get(0);
   for (i = m_lSel.Num()-1; i < m_lSel.Num(); i--) {
      if (padw[i] != -1)
         continue;   // nothing changed

      // if get here then delete
      m_lSel.Remove(i);
      padw = (DWORD*) m_lSel.Get(0);
   }

   SelSort();

   // go through and get rid of duplicates
   padw = (DWORD*) m_lSel.Get(0);
   for (i = m_lSel.Num()-1; i < m_lSel.Num(); i--) {
      if (!i)
         break;   // no point because nothing below
      if (padw[i] != padw[i-1])
         continue;   // nothing changed

      // if get here then delete
      m_lSel.Remove(i);
      padw = (DWORD*) m_lSel.Get(0);
   }
}


/*****************************************************************************************
CPolyMesh::VertFindFromClick Given that a user clicked on a point in space, this
returns the nearest vertex.

NOTE: This looks at the points in m_pLocSubdivide since it's assumed the image is
drawn after subdivisions.

inputs
   PCPoint        pLoc - Location (in object space) where the user clicked
   DWORD          dwSide - Side that user clicked on
returns
   DWORD - Vertex that user clicked on, or -1 is can't tell
*/
DWORD CPolyMesh::VertFindFromClick (PCPoint pLoc, DWORD dwSide)
{
   // get the side
   PCPMSide ps = SideGet (dwSide);
   if (!ps)
      return -1;

   WORD i;
   DWORD *padwVert = ps->VertGet();
   PCPMVert pv;
   CPoint pDist;
   fp fDist, fBest, fNextBest;
   DWORD dwBest;
   dwBest = -1;
   fNextBest = 1000000000;
   for (i = 0; i < ps->m_wNumVert; i++) {
      pv = VertGet (padwVert[i]);
      if (!pv)
         continue;
      
      pDist.Subtract (&pv->m_pLocSubdivide, pLoc);
      fDist = pDist.Length();
      if ((dwBest == -1) || (fDist < fBest)) {
         if (dwBest != -1)
            fNextBest = fBest;
         dwBest = padwVert[i];
         fBest = fDist;
      }
      else if (fDist < fNextBest)
         fNextBest = fDist;
   } // i

   // BUGFIX - if there's a best but the distance to it is not half that to the next
   // best then return -1 anyway
   if ((dwBest != -1) && (fBest*3 >= fNextBest))
      return -1;

   return dwBest;
}


/*****************************************************************************************
SortListAndRemoveDup - Goes through a list of DWORDs (or 2xDWORDs) and sorts
it. The list is tranferred over to a new list and all duplicates are removed in the process.

inputs
   DWORD             dwSize - Either 1 for just DWORDs or 2 for 2xDWORDs
   PCListFixed       plOrig - Original list. This will be sorted in place.
   PCListFixed       plDest - Initialized and then filled with the sorted and duplicate-removed original list
returns
   none
*/
static void SortListAndRemoveDup (DWORD dwSize, PCListFixed plOrig, PCListFixed plDest)
{
   // sort it
   DWORD *padwOrig = (DWORD*) plOrig->Get(0);
   DWORD dwNum = plOrig->Num();
   qsort (padwOrig, dwNum, sizeof(DWORD)*dwSize, (dwSize == 1) ? BDWORDCompare : B2DWORDCompare);

   // new list
   plDest->Init (sizeof(DWORD) * dwSize);
   plDest->Clear();
   DWORD i, dwLastAdded;
   dwLastAdded = -1;
   for (i = 0; i < dwNum; i++) {
      if (dwLastAdded != -1) {
         switch (dwSize) {
         case 1:
            if (padwOrig[i] == padwOrig[dwLastAdded])
               continue;
            break;
         case 2:
            if ((padwOrig[i*2+0] == padwOrig[dwLastAdded*2+0]) && (padwOrig[i*2+1] == padwOrig[dwLastAdded*2+1]))
               continue;
            break;
         }
      }

      // else, add
      plDest->Add (padwOrig + i * dwSize);
      dwLastAdded = i;
   } // i
}


/*****************************************************************************************
CPolyMesh::MirrorVert - This takes a list of vertices and fills in a new list containing
all the original vertices PLUS all the mirrors of the vertices. The new list is sorted
and duplicates eliminated.

inputs
   DWORD       *padw - Pointer to list of elements
   DWORD       dwNum - Number of elements
   PCListFixed plMirror - Initialized to sizeof (DWORD) and filled with the sorted
               and duplicate-removed list
returns
   none
*/
void CPolyMesh::MirrorVert (DWORD *padw, DWORD dwNum, PCListFixed plMirror)
{
   CListFixed lTemp;
   lTemp.Init (sizeof(DWORD));
   DWORD i, j;
   PMMIRROR *pMirror;
   for (i = 0; i < dwNum; i++) {
      PCPMVert pv = VertGet(padw[i]);

      // add itself to the temp
      lTemp.Required (lTemp.Num() + pv->m_wNumMirror + 1);
      lTemp.Add (&padw[i]);

      // get mirros
      pMirror = pv->MirrorGet();
      for (j = 0; j < pv->m_wNumMirror; j++)
         lTemp.Add (&pMirror[j].dwObject);
   }

   // sort, remove dups, and copy
   SortListAndRemoveDup (1, &lTemp, plMirror);
}


/*****************************************************************************************
CPolyMesh::MirrorSide - This takes a list of Sides and fills in a new list containing
all the original Sides PLUS all the mirrors of the Sides. The new list is sorted
and duplicates eliminated.

inputs
   DWORD       *padw - Pointer to list of elements
   DWORD       dwNum - Number of elements
   PCListFixed plMirror - Initialized to sizeof (DWORD) and filled with the sorted
               and duplicate-removed list
returns
   none
*/
void CPolyMesh::MirrorSide (DWORD *padw, DWORD dwNum, PCListFixed plMirror)
{
   CListFixed lTemp;
   lTemp.Init (sizeof(DWORD));
   DWORD i, j;
   PMMIRROR *pMirror;
   for (i = 0; i < dwNum; i++) {
      PCPMSide ps = SideGet(padw[i]);

      // add itself to the temp
      lTemp.Required (lTemp.Num() + ps->m_wNumMirror + 1);
      lTemp.Add (&padw[i]);

      // get mirros
      pMirror = ps->MirrorGet();
      for (j = 0; j < ps->m_wNumMirror; j++)
         lTemp.Add (&pMirror[j].dwObject);
   }

   // sort, remove dups, and copy
   SortListAndRemoveDup (1, &lTemp, plMirror);
}


/*****************************************************************************************
CPolyMesh::SidesToEdges - Takes a list of sides and writes out a list of edges (2xDWORD).

inputs
   DWORD       *padw - Pointer to list of elements
   DWORD       dwNum - Number of elements
   PCListFixed pl - Initialized to 2*sizeof (DWORD) and filled with the sorted
               and duplicate-removed list of edges
returns
   none
*/
void CPolyMesh::SidesToEdges (DWORD *padw, DWORD dwNum, PCListFixed pl)
{
   CListFixed lTemp;
   lTemp.Init (2*sizeof(DWORD));
   DWORD i, adw[2], dw;
   WORD j;
   for (i = 0; i < dwNum; i++) {
      PCPMSide ps = SideGet(padw[i]);
      DWORD *padwVert = ps->VertGet();
      lTemp.Required (lTemp.Num() + ps->m_wNumVert);
      for (j = 0; j < ps->m_wNumVert; j++) {
         adw[0] = padwVert[j];
         adw[1] = padwVert[(j+1)%ps->m_wNumVert];
         if (adw[0] > adw[1]) {
            dw = adw[0];
            adw[0] = adw[1];
            adw[1] = dw;
         }

         lTemp.Add (adw);
      } // j
   } // i

   // sort, remove dups, and copy
   SortListAndRemoveDup (2, &lTemp, pl);
}

   

/*****************************************************************************************
CPolyMesh::SidesToVert - Takes a list of sides and writes out a list of vertices (DWORD).

inputs
   DWORD       *padw - Pointer to list of elements
   DWORD       dwNum - Number of elements
   PCListFixed pl - Initialized to sizeof (DWORD) and filled with the sorted
               and duplicate-removed list of vertices
returns
   none
*/
void CPolyMesh::SidesToVert (DWORD *padw, DWORD dwNum, PCListFixed pl)
{
   CListFixed lTemp;
   lTemp.Init (sizeof(DWORD));
   DWORD i;
   WORD j;
   for (i = 0; i < dwNum; i++) {
      PCPMSide ps = SideGet(padw[i]);
      DWORD *padwVert = ps->VertGet();
      lTemp.Required (lTemp.Num() + ps->m_wNumVert);
      for (j = 0; j < ps->m_wNumVert; j++)
         lTemp.Add (&padwVert[j]);
   } // i

   // sort, remove dups, and copy
   SortListAndRemoveDup (1, &lTemp, pl);
}

   
   

/*****************************************************************************************
CPolyMesh::EdgesToVert - Takes a list of Edges and writes out a list of vertices (DWORD).

inputs
   DWORD       *padw - Pointer to list of elements (2 x DWORD)
   DWORD       dwNum - Number of elements
   PCListFixed pl - Initialized to sizeof (DWORD) and filled with the sorted
               and duplicate-removed list of vertices
returns
   none
*/
void CPolyMesh::EdgesToVert (DWORD *padw, DWORD dwNum, PCListFixed pl)
{
   // easy to di this since a list of edges is really a list of vertices
   CListFixed lTemp;
   lTemp.Init (sizeof(DWORD), padw, dwNum*2);

   // sort, remove dups, and copy
   SortListAndRemoveDup (1, &lTemp, pl);
}

   


/*****************************************************************************************
CPolyMesh::MirrorEdge - This takes a list of edges (2 DWORDs) and fills in a new list containing
all the original edges PLUS all the mirrors of the edges. The new list is sorted
and duplicates eliminated.

inputs
   DWORD       *padw - Pointer to list of elements (2xDWORD)
   DWORD       dwNum - Number of elements
   PCListFixed plMirror - Initialized to 2*sizeof (DWORD) and filled with the sorted
               and duplicate-removed list
   PCListFixed    plMirrorOnly - Normally this is NULL, but IF it's not null then it
                  contains a list of DWORDs to indicate the bitfields of what should
                  be mirrored. Those mirrors NOT indicated by the bitfields are ignored.
returns
   none
*/
void CPolyMesh::MirrorEdge (DWORD *padw, DWORD dwNum, PCListFixed plMirror, PCListFixed plMirrorOnly)
{
   CalcEdges ();

   CListFixed lTemp;
   lTemp.Init (sizeof(DWORD)*2);
   DWORD i, j, k, l, m, n, p;
   PCPMVert apv[2];
   for (i = 0; i < dwNum; i++) {
      // add itself to the temp
      lTemp.Add (&padw[i*2]);

      apv[0] = VertGet (padw[i*2+0]);
      apv[1] = VertGet (padw[i*2+1]);

      // find the edge...
      DWORD dwEdge;
      PCPMEdge pe;
      dwEdge = EdgeFind (padw[i*2+0], padw[i*2+1]);
      if (dwEdge == -1)
         continue;
      pe = EdgeGet (dwEdge);
      if (!pe)
         continue;

      // loop through the two sides in common with this edge...
      for (j = 0; j < 2; j++) {
         if (pe->m_adwSide[j] == -1)
            continue;

         // get the side
         PCPMSide ps;
         ps = SideGet (pe->m_adwSide[j]);

         // loop through all the mirrors of this side
         PMMIRROR *pMirror;
         pMirror = ps->MirrorGet();
         for (k = 0; k < ps->m_wNumMirror; k++) {
            // BUGFIX - If there's a plMirrorOnly list then only apply a mirror if the
            // bits match up
            if (plMirrorOnly) {
               DWORD dwTest;
               DWORD *padwMirror = (DWORD*)plMirrorOnly->Get(0);
               for (dwTest = 0; dwTest < plMirrorOnly->Num(); dwTest++, padwMirror++)
                  if (padwMirror[0] == pMirror[k].dwType)
                     break;
               if (dwTest >= plMirrorOnly->Num())
                  continue;   // not to be used since not in list
            }

            // get the mirror
            PCPMSide psm = SideGet (pMirror[k].dwObject);
            if (!psm)
               continue;

            // see if any of these edges are mirrors of the original edge
            DWORD *padwVert = psm->VertGet();
            for (l = 0; l < psm->m_wNumVert; l++) {
               DWORD adw[2], dw;
               adw[0] = padwVert[l];
               adw[1] = padwVert[(l+1)%psm->m_wNumVert];
               if (adw[0] > adw[1]) {
                  dw = adw[0];
                  adw[0] = adw[1];
                  adw[1] = dw;
               }

               // loop through each of the two points in adw and make sure that they're
               // either equal to the original points or one of their mirrors
               for (m = 0; m < 2; m++) {
                  for (n = 0; n < 2; n++) {
                     if (adw[m] == padw[i*2+n])
                        break;   // match
                     
                     // see if any vetices match
                     PMMIRROR *pmv;
                     pmv = apv[n]->MirrorGet();
                     for (p = 0; p < apv[n]->m_wNumMirror; p++)
                        if (pmv[p].dwObject == adw[m])
                           break;
                     if (p < apv[n]->m_wNumMirror)
                        break;   // match
                     // else, no match against this point
                  } // n - over two points in original edge
                  if (n < 2)
                     continue;   // match
                  else
                     break;   // no match
               } // m - over two points in potential edge
               if (m < 2)
                  continue;   // no match

               // add mirror
               lTemp.Add (adw);
            } // l - all edges

         } // k - all mirrors
      } // j - all attached sides
   } // i - all elements in list

   // sort, remove dups, and copy
   SortListAndRemoveDup (2, &lTemp, plMirror);
}

/*****************************************************************************************
CPolyMesh::GroupVertMove - Move one vertex point (and its mirrors). This does some extra
calculations for:
   - Won't do anything if the skeleteal deformations are on, or if more than
      one morph is on
   - Moves mirrored vertices in opposite directions
   - If vertex on a border location (and symmetry) then won't move it.
   - If supposed to be moved in two directions deals with that.

inputs
   DWORD          dwVert - Vertex index to move
   PCPoint        pDir - Direction. Amount to add to its position
   DWORD          dwNumGroup - Number of vertices in the group that's selected
   DWORD          *padwGroup - Array of vertex indecies in the group selected.
                  dwVert MUST be within this list. This call does checks so that
                  if dwVert and it's mirror both appear in this list then the
                  same point(s) will only get moved once. This group should be SORTED.
returns
   BOOL - TRUE if success. FALSE if cant move because deformation on
*/
BOOL CPolyMesh::GroupVertMove (DWORD dwVert, PCPoint pDir, DWORD dwNumGroup, DWORD *padwGroup)
{
   if (!CanModify())
      return FALSE;

   // make sure symmetry implimented
   SymmetryRecalc ();

   // get the vertex
   PCPMVert pv = VertGet(dwVert);
   if (!pv)
      return FALSE;

   // look for all the mirrors
   PMMIRROR *pMirror = pv->MirrorGet();

   // if one of the mirrors has an ID LESS than the current dwVert AND it's in
   // the padwGroup list then just exit here because we already moved it
   DWORD i;
   for (i = 0; i < pv->m_wNumMirror; i++)
      if ((pMirror[i].dwObject < dwVert) && (-1 != DWORDSearch (pMirror[i].dwObject, dwNumGroup, padwGroup)))
         return TRUE;   // already moved this

   // copy the direction. If the vertex is exactly on a mirror point then set the direction
   // so it won't break the mirror
   CPoint pNew;
   pNew.Copy (pDir);
   for (i = 0; i < 3; i++)
      if ((m_dwSymmetry & (1<<i)) && (pv->m_pLoc.p[i] == 0))
         pNew.p[i] = 0;

   // modify this point
   // if there's a morph then scale pNew in case the morph is only partially active
   DWORD dwID;
   if (m_lMorphRemapValue.Num()) {
      pNew.Scale (1.0 / ((fp*)m_lMorphRemapValue.Get(0))[0]);
      dwID = ((DWORD*)m_lMorphRemapID.Get(0))[0];
   }
   else
      dwID = -1;
   pv->DeformChanged (dwID, &pNew);

   // get the location of the point after the changes
   pv->DeformCalc (m_lMorphRemapID.Num(), (DWORD*)m_lMorphRemapID.Get(0),
      (fp*) m_lMorphRemapValue.Get(0), &pNew);

   // BUGFIX - Reload pMirror since DeformChanged could have messed up
   pMirror = pv->MirrorGet();

   // loop over all the mirrors
   CPoint pMir, pOld;
   DWORD j;
   for (i = 0; i < pv->m_wNumMirror; i++) {
      pMir.Copy (&pNew);

      // only do the mirroring if the point is NOT on the list of points moved
      // BUGFIX - Mirror even if on list: if (-1 == DWORDSearch (pMirror[i].dwObject, dwNumGroup, padwGroup)) {
      for (j = 0; j < 3; j++)
         if (m_dwSymmetry & pMirror[i].dwType & (1 << j))
            pMir.p[j] *= -1;

      // get exisitng location of vertex
      PCPMVert pv2 = VertGet (pMirror[i].dwObject);
      pv2->DeformCalc (m_lMorphRemapID.Num(), (DWORD*)m_lMorphRemapID.Get(0),
         (fp*) m_lMorphRemapValue.Get(0), &pOld);
      pMir.Subtract (&pOld);  // do this round-about so that wont get numerical rounding errors

      // modify
      if (m_lMorphRemapValue.Num()) {
         pMir.Scale (1.0 / ((fp*)m_lMorphRemapValue.Get(0))[0]);
         dwID = ((DWORD*)m_lMorphRemapID.Get(0))[0];
      }
      else
         dwID = -1;
      pv2->DeformChanged (dwID, &pMir);
   } // i

   return TRUE;
}

/*****************************************************************************************
CPolyMesh::GroupCOM - Given a group of vertices, edges, or sides, this determines
the "center of mass" - which is used for rotation, etc.

inputs
   DWORD          dwType - 0 for vertex, 1 for edge, 2 for side
   DWORD          dwNum - Number of elements in the list
   DWORD          *padw - Pointer to an array of dwNum elemnets. For vertex and sides
                  these are 1 DWORD each, for edges they're 2 DWORDs
   PCPoint        pCenter - Filled with the cnter of mass
   BOOL           fIncludeDeform - If TRUE use the morphs, else without morphs
returns
   none
*/
void CPolyMesh::GroupCOM (DWORD dwType, DWORD dwNum, DWORD *padw, PCPoint pCenter,
                          BOOL fIncludeDeform)
{
   pCenter->Zero();

   // easy fix - if dwtype is an edge, just pretends its a series of vertecies (which it
   // reall is)
   if (dwType == 1) {
      dwType = 0;
      dwNum *= 2;
   }

   DWORD i, j;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   CPoint pTemp;
   for (i = 0; i < dwNum; i++) {
      if (dwType == 0)
         pCenter->Add (fIncludeDeform ? &ppv[padw[i]]->m_pLocSubdivide : &ppv[padw[i]]->m_pLoc);
      else {
         pTemp.Zero();
         PCPMSide ps = pps[padw[i]];
         DWORD *padwVert = ps->VertGet();
         for (j = 0; j < ps->m_wNumVert; j++) {
            pTemp.Add (fIncludeDeform ? &ppv[padwVert[j]]->m_pLocSubdivide : &ppv[padwVert[j]]->m_pLoc);
         } // j
         pTemp.Scale (1.0 / (fp)ps->m_wNumVert);
         pCenter->Add (&pTemp);
      }
   } // i
   if (dwNum)
      pCenter->Scale (1.0 / (fp)dwNum);
}



/*****************************************************************************************
CPolyMesh::GroupNormal - Given a group of vertices, edges, or sides, this determines
the normal - which is used for rotation, etc.

inputs
   DWORD          dwType - 0 for vertex, 1 for edge, 2 for side
   DWORD          dwNum - Number of elements in the list
   DWORD          *padw - Pointer to an array of dwNum elemnets. For vertex and sides
                  these are 1 DWORD each, for edges they're 2 DWORDs
   PCPoint        pNorm - Filled with the normal
   BOOL           fIncludeDeform - If TRUE use the morphs, else without morphs
returns
   none
*/
void CPolyMesh::GroupNormal (DWORD dwType, DWORD dwNum, DWORD *padw, PCPoint pNorm,
                             BOOL fIncludeDeform)
{
   pNorm->Zero();

   // easy fix - if dwtype is an edge, just pretends its a series of vertecies (which it
   // reall is)
   if (dwType == 1) {
      dwType = 0;
      dwNum *= 2;
   }

   // make sure normals calculated
   CalcNorm();

   DWORD i, j;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   for (i = 0; i < dwNum; i++) {
      if (dwType == 0)
         pNorm->Add (fIncludeDeform ? &ppv[padw[i]]->m_pNormSubdivide : &ppv[padw[i]]->m_pNorm);
      else {
         if (fIncludeDeform) {
            // Need to recalc normal per side side want it based on the subdivision
            CPoint pNorm2;
            pNorm2.Zero();
            PCPMSide ps = pps[padw[i]];   // BUGFIX
            DWORD *padwVert = ps->VertGet();
            for (j = 0; j < ps->m_wNumVert; j++) {
               pNorm2.Add (&ppv[padwVert[j]]->m_pNormSubdivide);
            }
            pNorm2.Normalize();
            pNorm->Add (&pNorm2);
         }
         else
            pNorm->Add (&pps[padw[i]]->m_pNorm);
      }
   } // i
   pNorm->Normalize();
   if (pNorm->Length() < .5) {
      pNorm->Zero();
      pNorm->p[0] = 1;  // just pick a direction
   }
}


/*****************************************************************************************
CPolyMesh::GroupMove - Moves all the items in the group using a specific style of
movement.

inputs
   DWORD          dwMove - How to move:
                     0 - Move in the direction of the point
                     1 - Move in line with the normal, only using pMove.p[0]
                     2 - Scale, using pMove.p[0..2 for xyz] as a multiplier for the distance from COM
                     3 - Rotate around the normal, centered at COM. Use pMove.p[0] for angle
                     4 - Rotate anything but around normal. centered at COM. Use pMove.p[0] and pMove.p[1] for angles
                     5 - Move all points in line with normal, only using pMove.p[0]
   PCPoint        pMove - How to move
   DWORD          dwType - 0 for vertex, 1 for edge, 2 for side
   DWORD          dwNum - Number of elements in the list
   DWORD          *padw - Pointer to an array of dwNum elemnets. For vertex and sides
                  these are 1 DWORD each, for edges they're 2 DWORDs
returns
   BOOL - TRUE if success. FALSE if cant move because more than one morph, etc.
*/
BOOL CPolyMesh::GroupMove (DWORD dwMove, PCPoint pMove, DWORD dwType, DWORD dwNum, DWORD *padw)
{
   if (!CanModify())
      return FALSE;

   // make sure symmetry implimented
   SymmetryRecalc ();

   // figure out what need...
   BOOL fNeedCOM = FALSE;
   BOOL fNeedNorm = FALSE;
   BOOL fNeedMatrix = FALSE;
   CPoint pCOM, pNorm, pB, pC;
   CMatrix mApply;
   pCOM.Zero();
   pNorm.Zero();
   pB.Zero();
   pC.Zero();
   switch (dwMove) {
      case 0:  // move point
      case 5:  // move all points in line with normal
         break;
      case 1:  // move along normal
         fNeedNorm = TRUE;
         break;
      case 2:  // scale
         fNeedCOM = TRUE;
         break;
      case 3:  // rotate around normal
      case 4:  // rotate around anything but normal
         fNeedNorm = TRUE;
         fNeedCOM = TRUE;
         fNeedMatrix = TRUE;
         break;
   }
   if (fNeedNorm || fNeedMatrix)
      GroupNormal (dwType, dwNum, padw, &pNorm, TRUE);
   if (fNeedCOM || fNeedMatrix)
      GroupCOM (dwType, dwNum, padw, &pCOM, TRUE);
   if (fNeedMatrix) {
      // make up the other vectors
      pB.Zero();
      pB.p[2] = 1;   // BUGFIX - Was p[0] but changed to p[2]
      pC.CrossProd (&pNorm, &pB);
      pC.Normalize();
      if (pC.Length() < .5) {
         pB.Zero();
         pB.p[0] = 1;
         pC.CrossProd (&pNorm, &pB);
         pC.Normalize();   // will have to be perpendiculat
      }
     
      // recalc pB
      pB.CrossProd (&pC, &pNorm);
      pB.Normalize();   // just to make sure

      // calculate the matrix to apply...
      // first calculate a matrix that takes the point from its current coords into
      // one that's rotatable
      CMatrix mToRot, mTemp, mInv;
      mToRot.Translation (-pCOM.p[0], -pCOM.p[1], -pCOM.p[2]);
      mTemp.RotationFromVectors (&pNorm, &pB, &pC);
      mTemp.Invert (&mInv);
      mToRot.MultiplyRight (&mInv);

      // invert this to can go back
      mToRot.Invert4 (&mInv);

      // make the rotation matrix
      switch (dwMove) {
      case 3:  // rotate around normal
      default:
         mTemp.RotationX (pMove->p[0]);
         break;
      case 4:  // rotate around anything but normal
         mTemp.Rotation (0, pMove->p[0], pMove->p[1]);
         break;
      }

      // mash these all together
      mApply.Multiply (&mTemp, &mToRot);
      mApply.MultiplyRight (&mInv);
   }

   // convert this down to a list of points
   CListFixed lVert;
   lVert.Init (sizeof(DWORD));
   switch (dwType) {
   case 0:  // vert
      lVert.Init (sizeof(DWORD), padw, dwNum);
      qsort (lVert.Get(0), lVert.Num(), sizeof(DWORD), BDWORDCompare);
      break;
   case 1:  // edge
      EdgesToVert (padw, dwNum, &lVert);
      break;
   case 2:  // side
      SidesToVert (padw, dwNum, &lVert);
      break;
   default:
      return FALSE;
   }

   // make another list to determine how much to move each points
   CListFixed lMove;
   lMove.Init (sizeof(CPoint));
   DWORD i, j;
   DWORD *padwVert;
   padwVert = (DWORD*) lVert.Get(0);
   CPoint pTemp, pTemp2;
   for (i = 0; i < lVert.Num(); i++) {
      PCPMVert pv = VertGet(padwVert[i]);

      // get location, includeing deform
      pv->DeformCalc (m_lMorphRemapID.Num(), (DWORD*)m_lMorphRemapID.Get(0),
         (fp*) m_lMorphRemapValue.Get(0), &pTemp);

      switch (dwMove) {
         case 0:  // move point
            pTemp.Copy (pMove);
            break;
         case 5:  // move all points in line with normal
            pTemp.Copy (&pv->m_pNormSubdivide);
            pTemp.Scale (pMove->p[0]);
            break;
         case 1:  // move along normal
            pTemp.Copy (&pNorm);
            pTemp.Scale (pMove->p[0]);
            break;
         case 2:  // scale
            pTemp.Subtract (&pCOM);
            for (j = 0; j < 3; j++)
               pTemp.p[j] *= (pMove->p[j] - 1.0);
            break;
         case 3:  // rotate around normal
         case 4:  // rotate around anything but normal
            pTemp2.Copy (&pTemp);
            pTemp2.p[3] = 1;
            pTemp2.MultiplyLeft (&mApply);
            pTemp2.Subtract (&pTemp);
            pTemp.Copy (&pTemp2);
            break;
      }
      lMove.Add (&pTemp);
   } // i

   // go through and move all these vertices
   PCPoint papMove = (PCPoint)lMove.Get(0);
   for (i = 0; i < lVert.Num(); i++) {
      if (!GroupVertMove (padwVert[i], &papMove[i], lVert.Num(), padwVert))
         return FALSE;
   }

   // BUGFIX - Set dirty flag
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyMorphCOM = TRUE;
   m_fDirtyScale = TRUE;

   // done
   return TRUE;
}

/*****************************************************************************************
CPolyMesh::VertLocGet - Gets the given vertex's location INCLUDING any deformation
due to morphs.

inputs
   DWORD          dwVert - Vertex index... 0..m_lVert.Num()-1
   PCPoint        pLoc - Filled in with the location
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::VertLocGet (DWORD dwVert, PCPoint pLoc)
{
   PCPMVert pv = VertGet(dwVert);
   if (!pv)
      return FALSE;

   pv->DeformCalc (m_lMorphRemapID.Num(), (DWORD*)m_lMorphRemapID.Get(0),
      (fp*)m_lMorphRemapValue.Get(0), pLoc);
   return TRUE;
}


/*****************************************************************************************
CPolyMesh::SideFlipNormal - Flips the normals of the given sides AND any mirrors.

inputs
   DWORD             dwNum - Number of side index numbers in padw
   DWORD             *padw - Array of dwNum side index numbers. MUST be sorted.
returns
   none
*/
void CPolyMesh::SideFlipNormal (DWORD dwNum, DWORD *padw)
{
   // make sure symmetry implimented
   SymmetryRecalc ();

   // normals dirty
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;

   // loop through all the dwNum
   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      // get the side
      PCPMSide ps = SideGet(padw[i]);
      if (!ps)
         return;

      // look for all the mirrors
      PMMIRROR *pMirror = ps->MirrorGet();

      // if one of the mirrors has an ID LESS than the current side AND it's in
      // the padwGroup list then just exit here because we already moved it
      for (j = 0; j < ps->m_wNumMirror; j++)
         if ((pMirror[j].dwObject < padw[i]) && (-1 != DWORDSearch (pMirror[j].dwObject, dwNum, padw)))
            return;   // already moved this

      // flip this order and all its children
      ps->Reverse ();
      for (j = 0; j < ps->m_wNumMirror; j++) {
         PCPMSide psj = SideGet(pMirror[j].dwObject);
         if (!psj) continue;
         psj->Reverse();
      } // j

   } // i
}

/*****************************************************************************************
CPolyMesh::SideRotateVert - Rotates the vectors of the given sides AND any mirrors.

inputs
   DWORD             dwNum - Number of side index numbers in padw
   DWORD             *padw - Array of dwNum side index numbers. MUST be sorted.
returns
   none
*/
void CPolyMesh::SideRotateVert (DWORD dwNum, DWORD *padw)
{
   // make sure symmetry implimented
   SymmetryRecalc ();

   // dirty
   m_fDirtyRender = TRUE;

   // loop through all the dwNum
   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      // get the side
      PCPMSide ps = SideGet(padw[i]);
      if (!ps)
         return;

      // look for all the mirrors
      PMMIRROR *pMirror = ps->MirrorGet();

      // if one of the mirrors has an ID LESS than the current side AND it's in
      // the padwGroup list then just exit here because we already moved it
      for (j = 0; j < ps->m_wNumMirror; j++)
         if ((pMirror[j].dwObject < padw[i]) && (-1 != DWORDSearch (pMirror[j].dwObject, dwNum, padw)))
            return;   // already moved this

      // flip this order and all its children
      ps->RotateVert (TRUE);
      for (j = 0; j < ps->m_wNumMirror; j++) {
         BOOL  fRight = TRUE;
         PCPMSide psj = SideGet(pMirror[j].dwObject);
         if (!psj) continue;
         if (pMirror[j].dwType & 0x01)
            fRight = !fRight;
         if (pMirror[j].dwType & 0x02)
            fRight = !fRight;
         if (pMirror[j].dwType & 0x04)
            fRight = !fRight;
         psj->RotateVert (fRight);
      } // j

   } // i
}

/*****************************************************************************************
CPolyMesh::SideMakePlanar - Loops through all the sides (and the mirrors) and makes them
planar.

inputs
   DWORD             dwNum - Number of side index numbers in padw
   DWORD             *padw - Array of dwNum side index numbers. MUST be sorted.
returns
   BOOL - TRUE if success, FALSE if failure because too many morphs
*/
BOOL CPolyMesh::SideMakePlanar (DWORD dwNum, DWORD *padw)
{
   if (!CanModify())
      return FALSE;

   // make sure symmetry implimented
   SymmetryRecalc ();

   // dirty
   m_fDirtyRender = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyMorphCOM = TRUE;

   // loop through all the dwNum
   DWORD i, j;
   for (i = 0; i < dwNum; i++) {
      // get the side
      PCPMSide ps = SideGet(padw[i]);
      if (!ps)
         return FALSE;

      // look for all the mirrors
      PMMIRROR *pMirror = ps->MirrorGet();

      // if one of the mirrors has an ID LESS than the current side AND it's in
      // the padwGroup list then just exit here because we already moved it
      for (j = 0; j < ps->m_wNumMirror; j++)
         if ((pMirror[j].dwObject < padw[i]) && (-1 != DWORDSearch (pMirror[j].dwObject, dwNum, padw)))
            return FALSE;   // already moved this

      // find the normal
      ps->CalcNorm(this);

      // find the center of mass
      CPoint pCenter;
      CPoint pA, pB, pC;
      DWORD *padwVert = ps->VertGet();
      pCenter.Zero();
      for (j = 0; j < ps->m_wNumVert; j++) {
         VertLocGet (padwVert[j], &pA);
         pCenter.Add (&pA);
      }
      pCenter.Scale (1.0 / (fp)ps->m_wNumVert);

      // calculate the normal - but based on side, not normals of group
      CPoint pNormSide;
      // calculates the normal for the side...
      pNormSide.Zero();

      // triangulate
      CPoint p1, p2, pN;
      fp fLen;
      for (j = 0; j < ps->m_wNumVert; j++) {
         // get the points with deform
         VertLocGet (padwVert[j], &pA);
         VertLocGet (padwVert[(j+1)%ps->m_wNumVert], &pB);
         VertLocGet (padwVert[(j+2)%ps->m_wNumVert], &pC);

         // BUGFIX - Sometimes get normals messed up when p1 is equal to p2
         // so fix by ignoring if very close
         if (j) {
            // use old p2
            p1.Copy (&p2);
            p1.Scale (-1);
         }
         else {
            // calculate
            p1.Subtract (&pA, &pB);
            p1.Normalize();
         }
         p2.Subtract (&pC, &pB);
         p2.Normalize();

         pN.CrossProd (&p1, &p2);
         fLen = pN.Length();
         if (fLen < .01)
            continue;
         pN.Scale (1.0 / fLen);
         pNormSide.Add (&pN);

         // if only 3 vertices might as well break here
         if (ps->m_wNumVert <= 3)
            break;
      } // j

      if (ps->m_wNumVert > 3)
         pNormSide.Normalize();

      // loop through all the points and adjust
      CPoint pOrig, pNew;
      fp fOff;
      for (j = 0; j < ps->m_wNumVert; j++) {
         if (!VertLocGet (padwVert[j], &pOrig))
            continue;
         pNew.Subtract (&pOrig, &pCenter);
         fOff = pNew.DotProd (&pNormSide);  // so know how much varies from pNew
         if (fabs(fOff) < CLOSE)
            continue;
         pNew.Copy (&pNormSide);
         pNew.Scale (-fOff);
         if (!GroupVertMove (padwVert[j], &pNew, 1, &padwVert[j]))
            return FALSE;
      } // j

      // NOTE: Dont need to look into the mirrors of the sides because since the
      // points for the other sides are mirrored, and adjusting the points,
      // will automatically adjust the mirrors
   } // i

   return TRUE;
}


/*****************************************************************************************
CPolyMesh::SymmetryVertGroups - Takes a group of vertices that are to have an operation done
on them and fills in several lists with mirrored groups. This can be used for collapse

inputs
   DWORD          dwNum - Number in the group
   DWORD          *padw - Vertices in the original group. These must be sorted
   PCListFixed    paList - Pointer to an array of 8 lists. These lists will be filled
                        in with symmetry options. (Of course, if there's no symmetry turned
                        on some will be left empty). They will be initialized to sizeof(DWORD)
                        and filled in with vertex indecies. Note that paList[0] is the same
                        as the original list.
returns
   BOOL - TRUE if all the resulting lists are independent, FALSE if there are vertices
            in common (which might cause problems if used with collapse)
*/
BOOL CPolyMesh::SymmetryVertGroups (DWORD dwNum, DWORD *padw, PCListFixed paList)
{
   // clear lists
   DWORD i;
   for (i = 0; i < 8; i++) {
      paList[i].Init (sizeof(DWORD));
      paList[i].Clear();
   }

   // set up first one to be copy of original
   paList[0].Init (sizeof(DWORD), padw, dwNum);

   // loop based on symmetry
   DWORD x,y,z,xMax,yMax,zMax;
   DWORD j;
   PMMIRROR *pMirror;
   CListFixed lTemp;
   lTemp.Init (sizeof(DWORD));
   xMax = (m_dwSymmetry & 0x01) ? 2 : 1;
   yMax = (m_dwSymmetry & 0x02) ? 2 : 1;
   zMax = (m_dwSymmetry & 0x04) ? 2 : 1;
   for (x = 0; x < xMax; x++) for (y = 0; y < yMax; y++) for (z = 0; z < zMax; z++) {
      if (!x && !y && !z)
         continue;
      lTemp.Clear();

      // loop over original points and add to lTemp
      for (i = 0; i < dwNum; i++) {
         PCPMVert pv = VertGet(padw[i]);
         if (!pv)
            continue;

         // what symmetry are we looking for?
         DWORD dwWant = (x ? 0x01 : 0) | (y ? 0x02 : 0) | (z ? 0x04 : 0);
         if (pv->m_pLoc.p[0] == 0)
            dwWant &= (~0x01);
         if (pv->m_pLoc.p[1] == 0)
            dwWant &= (~0x02);
         if (pv->m_pLoc.p[2] == 0)
            dwWant &= (~0x04);

         // if dwWant is 0 then want this point, so use it
         if (!dwWant) {
            lTemp.Add (&padw[i]);
            continue;
         }

         // look through mirrors
         pMirror = pv->MirrorGet();
         for (j = 0; j < pv->m_wNumMirror; j++) {
            if (pMirror[j].dwType == dwWant) {
               lTemp.Add (&pMirror[j].dwObject);
               break;
            }
         } // j

      } // i

      // sort the list
      SortListAndRemoveDup (1, &lTemp, &paList[x + y*2 + z*4]);
   } // xyz

   // go through all the lists... if find any lists that are exact matches
   // then they straddle the symmetries exactly so get rid of them
   for (i = 0; i < 8; i++) {
      if (!paList[i].Num())
         continue;   // nothing

      for (j = i+1; j < 8; j++) {
         if (paList[i].Num() != paList[j].Num())
            continue;   // no diff
         if (!memcmp(paList[i].Get(0), paList[j].Get(0), paList[i].Num() * sizeof(DWORD))) {
            // exact match
            paList[j].Clear();
            continue;
         }
      } // j
   } // i

   // test to see if any groups have overlapping elements... the easiest way to do this
   // in the code is to create a new list, remove duplicates, and see that number of
   // elements don't change
   CListFixed lTemp2;
   lTemp.Clear();
   for (i = 0; i < 8; i++) {
      DWORD *padwElem = (DWORD*) paList[i].Get(0);
      lTemp.Required (lTemp.Num() + paList[i].Num());
      for (j = 0; j < paList[i].Num(); j++)
         lTemp.Add (&padwElem[j]);
   } // i
   SortListAndRemoveDup (1, &lTemp, &lTemp2);
   return (lTemp.Num() == lTemp2.Num());
}


/*****************************************************************************************
CPolyMesh::VertRenamed - This goes through every sub-object of the polymesh that
knows about vertices and informs them that the vertices are renamed

NOTE: This may break symmetry... so it might be good to recalc symmetry after this.

NOTE: This could leaving hanging vertices, or invalid edges, so should call some sort
of cleanup function...

NOTE: Vertices contain a list of sides that they belong to. This could be corrupted
by this call because vertex might be renamed so it no longer belongs to
that side. Must call VertSideLinkRebuild()

inputs
   DWORD             dwNum - Number of renaming issues
   DWORD             *padwOrig - Pointer to an array of dwNum vertices. This MUST be sorted
   DWORD             *padwNew - Pointer to an array of dwNum vertices to rename to
*/
void CPolyMesh::VertRenamed (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew)
{
   SelVertRenamed (dwNum, padwOrig, padwNew);

   // mark edges invalid, along with most other things
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   // vertices
   DWORD i;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < m_lVert.Num(); i++) {
      ppv[i]->MirrorRename (dwNum, padwOrig, padwNew);
   } // i

   // sides
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   for (i = 0; i < m_lSide.Num(); i++) {
      pps[i]->VertRename (dwNum, padwOrig, padwNew);
   }
}



/*****************************************************************************************
CPolyMesh::SideRenamed - This goes through every sub-object of the polymesh that
knows about Sides and informs them that the Sides are renamed

NOTE: This may break symmetry... so it might be good to recalc symmetry after this.

NOTE: This could leaving hanging Sides, or invalid edges, so should call some sort
of cleanup function...

inputs
   DWORD             dwNum - Number of renaming issues
   DWORD             *padwOrig - Pointer to an array of dwNum Sides. This MUST be sorted
   DWORD             *padwNew - Pointer to an array of dwNum Sides to rename to
   BOOL              fCanCreateDup - TRUE if the reanming of sides could create a duplicate
                        entry in the vertices' SideText, FALSE if not
*/
void CPolyMesh::SideRenamed (DWORD dwNum, DWORD *padwOrig, DWORD *padwNew, BOOL fCanCreateDup)
{
   SelSideRenamed (dwNum, padwOrig, padwNew);

   // mark edges invalid, along with most other things
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   // vertices
   DWORD i;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < m_lVert.Num(); i++) {
      ppv[i]->SideRename (dwNum, padwOrig, padwNew);
      ppv[i]->SideTextRename (dwNum, padwOrig, padwNew, fCanCreateDup);
   } // i

   // sides
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   for (i = 0; i < m_lSide.Num(); i++) {
      pps[i]->MirrorRename (dwNum, padwOrig, padwNew);
   }
}



/*****************************************************************************************
CPolyMesh:VertDeleteMany - Delete a group of vertices.

NOTE: Depending upon what vertices deleted, may break symmetry info.

inputs
   DWORD          dwNum - Number of vertices to delete
   DWORD          *padw - Pointer to an array of the vertices to delete.
returns
   none
*/
void CPolyMesh::VertDeleteMany (DWORD dwNum, DWORD *padw)
{
   // set dirty
   m_fDirtyEdge = m_fDirtyNorm = m_fDirtyRender = m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   // create a rename list
   DWORD i;
   CListFixed lRenameOrig, lRenameNew;
   lRenameOrig.Init (sizeof(DWORD));
   lRenameOrig.Required (m_lVert.Num());
   for (i = 0; i < m_lVert.Num(); i++)
      lRenameOrig.Add (&i);
   lRenameNew.Init (sizeof(DWORD), lRenameOrig.Get(0), lRenameOrig.Num());
   DWORD *padwNew, *padwOrig;
   DWORD dwNumRename;
   dwNumRename = lRenameOrig.Num();
   padwNew = (DWORD*) lRenameNew.Get(0);
   padwOrig = (DWORD*) lRenameOrig.Get(0);

   // account for deleting
   for (i = 0; i < dwNum; i++)
      padwNew[padw[i]] = -1;  // so no to delete

   // go through again and figure out what new index will be
   DWORD dwCur;
   dwCur = 0;
   for (i = 0; i < dwNumRename; i++) {
      if (padwNew[i] == -1)
         continue;

      padwNew[i] = dwCur;
      dwCur++;
   }

   // go through and delete elements, working backwards
   for (i = m_lVert.Num()-1; i < m_lVert.Num(); i--) {
      if (padwNew[i] != -1)
         continue;   // dont delete

      // else delete
      PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
      delete ppv[i];
      m_lVert.Remove (i);
   } // i

   // rename everything, which will cause deletion of references
   VertRenamed (dwNumRename, padwOrig, padwNew);
}




/*****************************************************************************************
CPolyMesh::SideDeleteMany - Delete a group of sides.

NOTE: Ignores symmetry. Depending upon what sides deleted, may break symmetry info.

inputs
   DWORD          dwNum - Number of sides to delete
   DWORD          *padw - Pointer to an array of the Sides to delete.
returns
   none
*/
void CPolyMesh::SideDeleteMany (DWORD dwNum, DWORD *padw)
{
   // set dirty
   m_fDirtyEdge = m_fDirtyNorm = m_fDirtyRender = m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   // create a rename list
   DWORD i;
   CListFixed lRenameOrig, lRenameNew;
   lRenameOrig.Init (sizeof(DWORD));
   lRenameOrig.Required (m_lSide.Num());
   for (i = 0; i < m_lSide.Num(); i++)
      lRenameOrig.Add (&i);
   lRenameNew.Init (sizeof(DWORD), lRenameOrig.Get(0), lRenameOrig.Num());
   DWORD *padwNew, *padwOrig;
   DWORD dwNumRename;
   dwNumRename = lRenameOrig.Num();
   padwNew = (DWORD*) lRenameNew.Get(0);
   padwOrig = (DWORD*) lRenameOrig.Get(0);

   // account for deleting
   for (i = 0; i < dwNum; i++)
      padwNew[padw[i]] = -1;  // so no to delete

   // go through again and figure out what new index will be
   DWORD dwCur;
   dwCur = 0;
   for (i = 0; i < dwNumRename; i++) {
      if (padwNew[i] == -1)
         continue;

      padwNew[i] = dwCur;
      dwCur++;
   }

   // go through and delete elements, working backwards
   for (i = m_lSide.Num()-1; i < m_lSide.Num(); i--) {
      if (padwNew[i] != -1)
         continue;   // dont delete

      // else delete
      PCPMSide *ppv = (PCPMSide*) m_lSide.Get(0);
      delete ppv[i];
      m_lSide.Remove (i);
   } // i

   // rename everything, which will cause deletion of references
   SideRenamed (dwNumRename, padwOrig, padwNew, FALSE);
}


/*****************************************************************************************
CPolyMesh::SideDeleteDead - Looks through all the sides and figures out which ones
are dead ones (those with less than 3 points). It also cleans up sides which have
duplicate points following one another so the duplicate is removed. This then
deletes those sides that are mucked up.

NOTE: This may mess up symmetry depending upon which sides are deleted.
*/
void CPolyMesh::SideDeleteDead (void)
{
   // init dead list
   CListFixed lDead;
   lDead.Init (sizeof(DWORD));

   // loop through all the sides
   DWORD i, j;
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   for (i = 0; i < m_lSide.Num(); i++) {
      PCPMSide ps = pps[i];

      // if duplicate vertices remove dups
      DWORD *padwVert = ps->VertGet();
tryagain:
      for (j = 0; (j < ps->m_wNumVert) && (ps->m_wNumVert >= 3); j++) {
         if (padwVert[j] == padwVert[(j+1)%ps->m_wNumVert]) {
            ps->VertRemoveByIndex ((j+1)%ps->m_wNumVert);
            padwVert = ps->VertGet();
         }
      } // j

      // see if get vertex a folloed by vert b followed by a again. If so,
      // simplify down to b and a
      for (j = 0; (j < ps->m_wNumVert) && (ps->m_wNumVert >= 3); j++) {
         if (padwVert[j] == padwVert[(j+2)%ps->m_wNumVert]) {
            DWORD dwRemove1, dwRemove2;
            dwRemove1 = (j+1)%ps->m_wNumVert;
            dwRemove2 = (j+2)%ps->m_wNumVert;
            ps->VertRemoveByIndex (dwRemove1);
            if (dwRemove1 < dwRemove2)
               dwRemove2--;
            ps->VertRemoveByIndex (dwRemove2);
            goto tryagain;
         }
      } // j

      // if have less than 3 sides then remove
      if (ps->m_wNumVert < 3)
         lDead.Add (&i);

   } // i

   // delete the dead list
   SideDeleteMany (lDead.Num(), (DWORD*) lDead.Get(0));
}




/*****************************************************************************************
CPolyMesh::VertDeleteDead - Looks through all the vertices and figures out which ones
are dead ones (those with less than 3 points). It also cleans up vertices which have
duplicate points following one another so the duplicate is removed. This then
deletes those vertices that are mucked up.

NOTE: May want to call VertSideLinkRebuild() before this to make sure that the links
   are properly estabilished. If there are any errors then the vertices may not
   be deleted.
NOTE: This may mess up symmetry depending upon which vertices are deleted.
*/
void CPolyMesh::VertDeleteDead (void)
{
   // init dead list
   CListFixed lDead;
   lDead.Init (sizeof(DWORD));

   // loop through all the Vertices
   DWORD i;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < m_lVert.Num(); i++) {
      if (!ppv[i]->m_wNumSide)
         lDead.Add (&i);
   } // i

   // delete the dead list
   VertDeleteMany (lDead.Num(), (DWORD*) lDead.Get(0));
}

/*****************************************************************************************
CPolyMesh::VertCollapse - Takes a list of vertices and collapses them together into
one point. This also collapses all the mirrors.

inputs
   DWORD          dwNum - Number of vertices
   DWORD          *padw - Pointer to an array of dwNum vertex indecies
returns
   BOOL - TRUE if collapsed, FALSE if didn't because some of the verticies are
      overlapping one another and screwing up symmetry.
*/
BOOL CPolyMesh::VertCollapse (DWORD dwNum, DWORD *padw)
{
   SymmetryRecalc ();

   // get all the groups
   CListFixed alGroup[8];
   if (!SymmetryVertGroups(dwNum, padw, alGroup))
      return FALSE;

   // find the centers for all these groups
   DWORD i, j;
   CPoint apCenter[8];
   DWORD adwPoint[8];
   CListFixed lAverage;
   lAverage.Init (sizeof(PCPMVert));
   memset (apCenter, 0, sizeof(apCenter));
   for (i = 0; i < 8; i++) {
      adwPoint[i] = -1; // in case dont find
      if (!alGroup[i].Num())
         continue;

      GroupCOM (0, alGroup[i].Num(), (DWORD*) alGroup[i].Get(0), &apCenter[i], FALSE);

      // go through points.. if any have a 0 on a dimension that's used for
      // symmetry then the COM also has to be 0...
      DWORD *padwVert = (DWORD*) alGroup[i].Get(0);
      for (j = 0; j < alGroup[i].Num(); j++) {
         PCPMVert pv = VertGet(padwVert[j]);
         if ((m_dwSymmetry & 0x01) && (pv->m_pLoc.p[0] == 0))
            apCenter[i].p[0] = 0;
         if ((m_dwSymmetry & 0x02) && (pv->m_pLoc.p[1] == 0))
            apCenter[i].p[1] = 0;
         if ((m_dwSymmetry & 0x04) && (pv->m_pLoc.p[2] == 0))
            apCenter[i].p[2] = 0;
      } // j

      // now, add/find the point
      adwPoint[i] = VertFind (&apCenter[i]);
      if (adwPoint[i] == -1)
         adwPoint[i] = VertAdd (&apCenter[i]);
      if (adwPoint[i] == -1)
         continue;

      // this new point should be an average of the existing points
      PCPMVert pv;
      lAverage.Clear();
      lAverage.Required (alGroup[i].Num());
      for (j = 0; j < alGroup[i].Num(); j++) {
         pv = VertGet (padwVert[j]);
         lAverage.Add (&pv);
      } // i
      pv = VertGet(adwPoint[i]); // will find
      pv->Average ((PCPMVert*)lAverage.Get(0), NULL, lAverage.Num(), -1, TRUE);
      pv->m_pLoc.Copy (&apCenter[i]);  // just to make sure
   } // i

   // create a rename list that renames the existing points to their new collapsed values
   CListFixed lRenameOrig, lRenameNew;
   lRenameOrig.Init (sizeof(DWORD));
   lRenameOrig.Required (m_lVert.Num());
   for (i = 0; i < m_lVert.Num(); i++)
      lRenameOrig.Add (&i);
   lRenameNew.Init (sizeof(DWORD), lRenameOrig.Get(0), lRenameOrig.Num());
   DWORD *padwNew, *padwOrig;
   DWORD dwNumRename;
   dwNumRename = lRenameOrig.Num();
   padwNew = (DWORD*) lRenameNew.Get(0);
   padwOrig = (DWORD*) lRenameOrig.Get(0);

   for (i = 0; i < 8; i++) {
      if (adwPoint[i] == -1)
         continue;   // nothing to rename to
      DWORD *padwVert = (DWORD*) alGroup[i].Get(0);
      for (j = 0; j < alGroup[i].Num(); j++)
         padwNew[padwVert[j]] = adwPoint[i]; // rename
   } // i

   // go through everything and tell of the renaming
   VertRenamed (dwNumRename, padwOrig, padwNew);
   SideDeleteDead ();
   VertSideLinkRebuild ();
   VertDeleteDead();
   m_fDirtySymmetry = TRUE;
   SymmetryRecalc ();   // recalc symmetry just in case
   return TRUE;
}


/*****************************************************************************************
FindEdgeIndex - Look through side's vertices and find out the index for the start of
the edge. Can replace the point at the index and after that..

inputs
   WORD           wNumVert - Number of vertices
   DWORD          *padwVert - Array of vertices
   DWORD          *padwEdge - Pointer to an array of 2 DWORDs for the edge
   BOOL           *pfFlip - Filled with TRUE if the edge is found and it's in the
                     order passed in. FALSE if it's bound but the order is revered
returns
   WORD - Index, or -1 if cant find
*/
static DWORD FindEdgeIndex (WORD wNumVert, DWORD *padwVert, DWORD *padwEdge, BOOL *pfFlip)
{
   // find it
   WORD i;
   if (wNumVert < 3)
      return -1;  // too small
   for (i = 0; i < wNumVert; i++) {
      if ((padwVert[i] == padwEdge[0]) && (padwVert[(i+1)%wNumVert] == padwEdge[1])) {
         *pfFlip = FALSE;
         return i;
      }
      if ((padwVert[i] == padwEdge[1]) && (padwVert[(i+1)%wNumVert] == padwEdge[0])) {
         *pfFlip = TRUE;
         return i;
      }
   } // i

   // not found
   return -1;
}


/*****************************************************************************************
CPolyMesh::SideMerge - Merges two sides together using a common edge...

NOTE: This leaves the 2nd side arounds still...

inputs
   DWORD          dwSide1 - Side to merge into
   DWORD          dwSide2 - Side to merge from
   DWORD          *padwEdge - Pointer to an array of two dwords that define the edge
returns
   BOOL - TRUE if merged (and dwSide2 is deleted), FALSE if dwSide2 still exists afterwards
*/
BOOL CPolyMesh::SideMerge (DWORD dwSide1, DWORD dwSide2, DWORD *padwEdge)
{
   // if they're the same side then might have some points connected, so see about this...
   DWORD dwFind1, dwFind2;
   BOOL fFlip1, fFlip2;
   PCPMSide ps1 = SideGet (dwSide1);
   PCPMSide ps2 = SideGet (dwSide2);
   if (!ps1 || !ps2)
      return FALSE;
   if (dwSide1 == dwSide2) {
      while (TRUE) { // repeat while find edge
         dwFind1 = FindEdgeIndex (ps1->m_wNumVert, ps1->VertGet(), padwEdge, &fFlip1);
         if (dwFind1 == -1)
            break;

         // if found then delete that
         DWORD dw2;
         dw2 = (dwFind1+1)%ps1->m_wNumVert;
         ps1->VertRemoveByIndex (dwFind1);
         if (dwFind1 < dw2)
            dw2--;
         ps1->VertRemoveByIndex (dw2);
      }
      return FALSE;  // nothing deleted
   }

   // find out where edge appears
   DWORD *padw1, *padw2;
   dwFind1 = FindEdgeIndex (ps1->m_wNumVert, padw1 = ps1->VertGet(), padwEdge, &fFlip1);
   dwFind2 = FindEdgeIndex (ps2->m_wNumVert, padw2 = ps2->VertGet(), padwEdge, &fFlip2);
   if ((dwFind1 == -1) || (dwFind2 == -1))
      return FALSE;  // no match - shouldnt happen
   // NOTE - Ignoring flip information for now... hopefully it wont matter. Should be flipped

   // rebuild the list
   CListFixed lNew;
   lNew.Init (sizeof(DWORD));
   // add all the points from the second one, including the edge points BUT
   // make sure to start at 2nd half of edge and work way around
   DWORD i;
   lNew.Required (ps2->m_wNumVert + ps1->m_wNumVert);
   for (i = 0; i < ps2->m_wNumVert; i++)
      lNew.Add (&padw2[(i+dwFind2+1)%ps2->m_wNumVert]);
   // add all points from first one, EXCEPT edge points, starting after edge
   for (i = 0; i < (DWORD) ps1->m_wNumVert-2; i++)
      lNew.Add (&padw1[(i+dwFind1+2)%ps1->m_wNumVert]);

   // clear existing verts
   ps1->VertClear();
   ps1->VertAdd ((DWORD*) lNew.Get(0), lNew.Num());

   return TRUE;
}


/*****************************************************************************************
CPolyMesh::SideKeep - Keeps the given sides, deleting the rest. This takes symmetry
into account.

inputs
   DWORD          dwNum - Number of elements in the list
   DWORD          *padw - Pointer to an array of dwNum elemnets. DWORD IDs for sides
returns
   BOOL - TRUE if success. FALSE if cant because there won't be anything left
*/
BOOL CPolyMesh::SideKeep (DWORD dwNum, DWORD *padw)
{
   // make sure symmetry is built
   SymmetryRecalc();

   CListFixed lMirror;
   MirrorSide (padw, dwNum, &lMirror);
   DWORD *padwMirror = (DWORD*)lMirror.Get(0);
   DWORD dwMirrorNum = lMirror.Num();
   if (!dwMirrorNum)
      return FALSE;

   // make a list of the ones want to keep
   CListFixed lKeep;
   lKeep.Init (sizeof(DWORD));
   DWORD i;
   for (i = 0; i < m_lSide.Num(); i++)
      if (DWORDSearch (i, dwMirrorNum, padwMirror) == -1)
         lKeep.Add (&i);

   // delete these
   SideDeleteMany (lKeep.Num(), (DWORD*)lKeep.Get(0));
   VertSideLinkRebuild ();
   VertDeleteDead();

   // turn off backface culling if not already off
   m_fCanBackface = FALSE;

   return TRUE;
}

/*****************************************************************************************
CPolyMesh::EdgeDelete - Deletes edges. This does NOT take mirroring into account

inputs
   DWORD          dwNum - Number of elements in the list
   DWORD          *padw - Pointer to an array of dwNum elemnets. For vertex and sides
                  these are 1 DWORD each, for edges they're 2 DWORDs
   PCListFixed    plVertDel - Normally null, but the bevelling code will pass in a list
                  of vertices that are to be deleted (after the edges have been removed).
                  The list is sorted.
returns
   BOOL - TRUE if success. FALSE if cant move because more than one morph, etc.
*/
BOOL CPolyMesh::EdgeDelete (DWORD dwNum, DWORD *padw, PCListFixed plVertDel)
{

   // go through all the edges that have in our new list and created an associated
   // list providing information about what sides the edges are connected to
   CalcEdges();
   DWORD i;
   DWORD adw[2];
   CListFixed lSideMap;
   lSideMap.Init (sizeof(DWORD)*2);
   for (i = 0; i < dwNum; i++) {
      DWORD dwEdge = EdgeFind (padw[i*2+0], padw[i*2+1]);
      PCPMEdge pe = EdgeGet (dwEdge);
      if (!pe) {
         // shouldnt happen, but add -1 to list
         adw[0] = adw[1] = -1;
         lSideMap.Add (adw);
         continue;
      }

      // add the sides for this
      lSideMap.Add (pe->m_adwSide);
   } // i
   DWORD *padwSideMap;
   padwSideMap = (DWORD*) lSideMap.Get(0);

   // make a remap list
   CListFixed lRenameOrig, lRenameNew;
   lRenameOrig.Init (sizeof(DWORD));
   lRenameOrig.Required (m_lSide.Num());
   for (i = 0; i < m_lSide.Num(); i++)
      lRenameOrig.Add (&i);
   lRenameNew.Init (sizeof(DWORD), lRenameOrig.Get(0), lRenameOrig.Num());
   DWORD *padwNew, *padwOrig;
   DWORD dwNumRename;
   dwNumRename = lRenameOrig.Num();
   padwNew = (DWORD*) lRenameNew.Get(0);
   padwOrig = (DWORD*) lRenameOrig.Get(0);

   // keep a list of ones to be delted
   CListFixed lDelete;
   lDelete.Init (sizeof(DWORD));

   // go through each edge and either remove the edge from the side, or merge the two sides
   DWORD j;
   for (i = 0; i < dwNum; i++) {
      // if both -1 then do nothing
      if ((padwSideMap[i*2+0] == -1) && (padwSideMap[i*2+1] == -1))
         continue;

      // if removing the edge and there's only one side then will want to delete the side...
      if ( ((padwSideMap[i*2+0] != -1) && (padwSideMap[i*2+1] == -1)) || ((padwSideMap[i*2+0] == -1) && (padwSideMap[i*2+1] != -1))) {
         // NOTE: Never tested because can't actually get to case where can select edge with
         // just one side since use the fact that have two adjacent sides in image as
         // a way of detecting edge

         // delete the side
         DWORD dwToDel = (padwSideMap[i*2+0] != -1) ? padwSideMap[i*2+0] : padwSideMap[i*2+1];
         padwNew[dwToDel] = -1;  // so know that want to delete
         lDelete.Add (&dwToDel);
         m_fCanBackface = FALSE; // turn off backface since deleting without replacing

         // go through all the edges and remap the value
         for (j = 0; j < dwNum*2; j++)
            if (padwSideMap[j] == dwToDel)
               padwSideMap[j] = -1;

         continue;
      }

      // if get here want to merge two sides by the common edge...
      if (SideMerge (padwSideMap[i*2+0], padwSideMap[i*2+1], &padw[i*2])) {
         DWORD dwToDel = padwSideMap[i*2+1];
         padwNew[dwToDel] = padwSideMap[i*2+0];
         lDelete.Add (&dwToDel);

         // go through all the edges and remap the value
         for (j = 0; j < dwNum*2; j++)
            if (padwSideMap[j] == dwToDel)
               padwSideMap[j] = padwSideMap[i*2+0];
      }
   } // i


   // rename everything that uses the sides...
   SideRenamed (dwNumRename, padwOrig, padwNew, TRUE);

   // delete the sides...
   SideDeleteMany (lDelete.Num(), (DWORD*) lDelete.Get(0));

   // if plVertDel then delete some vertices
   if (plVertDel) {
      CListFixed lNeg;
      lNeg.Init (sizeof(DWORD), plVertDel->Get(0), plVertDel->Num());
      DWORD *padwNeg = (DWORD*) lNeg.Get(0);
      for (i = 0; i < lNeg.Num(); i++)
         padwNeg[i] = -1;

      // complications on complications... i have this feature in for doing
      // bevelling, but I cant delete all the vertices on this list because
      // some of them only appear in a triangle once, which would cause it
      // to disappear. Therefore, go through the list of vertices, see what
      // side they're on, and if they're attached to a 3-sided side then
      // dont delete
      VertSideLinkRebuild ();
      DWORD *padwVert = (DWORD*) plVertDel->Get(0);
      for (i = 0; i < plVertDel->Num(); i++) {
         PCPMVert pv = VertGet (padwVert[i]);
         DWORD *padwSide = pv->SideGet();
         for (j = 0; j < pv->m_wNumSide; j++) {
            PCPMSide ps = SideGet (padwSide[j]);
            if (ps->m_wNumVert <= 3)
               break;
         } // j
         // NOTE: Had put this in there to stop some triangles from being deleted,
         // but it causes more problems than it solves. The triangles seem to appear
         // in places you wouldn't usually bevel (like flag surfaces), so dont bother
         //if (j < pv->m_wNumSide)
         //   padwNeg[i] = padwVert[i];  // so dont delete
      } // i

      VertRenamed (plVertDel->Num(), padwVert, padwNeg);
   }

   // delete dead sides
   SideDeleteDead ();

   // relink the vertices
   VertSideLinkRebuild ();

   // delete dead vertices
   VertDeleteDead();

   // recalc symmetry just in case
   m_fDirtySymmetry = TRUE;
   SymmetryRecalc ();   // recalc symmetry just in case
   return TRUE;
}

/*****************************************************************************************
CPolyMesh::GroupDelete - Deletes the objects and their mirrors
movement.

inputs
   DWORD          dwType - 0 for vertex, 1 for edge, 2 for side
   DWORD          dwNum - Number of elements in the list
   DWORD          *padw - Pointer to an array of dwNum elemnets. For vertex and sides
                  these are 1 DWORD each, for edges they're 2 DWORDs
returns
   BOOL - TRUE if success. FALSE if cant move because more than one morph, etc.
*/
BOOL CPolyMesh::GroupDelete (DWORD dwType, DWORD dwNum, DWORD *padw)
{
   // make sure symmetry is built
   SymmetryRecalc();

   // get mirrors
   CListFixed lMirror;
   switch (dwType) {
   case 0: // vert
      MirrorVert (padw, dwNum, &lMirror);
      break;
   case 1: // edge
      MirrorEdge (padw, dwNum, &lMirror);
      break;
   case 2: // side
      MirrorSide (padw, dwNum, &lMirror);
      break;
   default:
      return FALSE;
   }
   dwNum = lMirror.Num();
   padw = (DWORD*)lMirror.Get(0);


   // if deleting sides it's pretty easy
   if (dwType == 2) {
      // delete side
      SideDeleteMany (dwNum, padw);
      VertSideLinkRebuild ();
      VertDeleteDead();

      // turn off backface culling if not already off
      m_fCanBackface = FALSE;

      return TRUE;
   }

   // if delete vertices then also easy
   if (dwType == 0) {
      VertDeleteMany (dwNum, padw);

      DWORD dwNumSide = m_lSide.Num();
      SideDeleteDead ();
      VertSideLinkRebuild ();
      VertDeleteDead();
      m_fDirtySymmetry = TRUE;
      SymmetryRecalc ();   // recalc symmetry just in case

      // turn off backface culling if not already off - if removed sides
      if (m_lSide.Num() != dwNumSide)
         m_fCanBackface = FALSE;
      return TRUE;
   }
   
   // else, deleting edges, which is trickier...
   return EdgeDelete (dwNum, padw);
}

/*****************************************************************************************
CPolyMesh::SideTesselate - Tesselate a group of sides. This also tesselates the symmetrical
sides

inputs
   DWORD          dwNum - Number of sides
   DWORD          *padw - Pointer to an array of side indecies
   DWORD          dwTess - Type of tesselation...
                     0 - Divide into triangles, no new vertices
                     1 - New vertex in the center, divide into triangles
                     2 - New vertex in the center and edges, divide into quads
returns
   BOOL - TRUE if succeded
*/
typedef struct {
   DWORD          adwVert[2];    // vertices
   DWORD          adwSide[2];    // sides involved
   DWORD          dwNewVert;     // new vertex used for division
} TESSEDGE, *PTESSEDGE;



BOOL CPolyMesh::SideTesselate (DWORD dwNum, DWORD *padw, DWORD dwTess)
{
   // make sure symmetry implimented
   SymmetryRecalc ();

   // convert this list to include symmetry
   CListFixed lMirror;
   MirrorSide (padw, dwNum, &lMirror);
   padw = (DWORD*) lMirror.Get(0);
   dwNum = lMirror.Num();

   // calculate edges, if we need to add edges
   if (dwTess == 2)  // new vertex in center and edges
      CalcEdges();

   // turn mirror off for now
   DWORD dwSym, i, j;
   dwSym = m_dwSymmetry;
   m_dwSymmetry = 0;
   m_fDirtySymmetry = TRUE;
   SymmetryRecalc();

   // if need center point then calc that
   CListFixed lCenter, lPCPMVert;
   DWORD *padwVert;
   PCPMVert pv;
   lCenter.Init (sizeof(DWORD));
   lPCPMVert.Init (sizeof(PCPMVert));
   if ((dwTess == 1) || (dwTess == 2)) {
      for (i = 0; i < dwNum; i++) {
         PCPMSide ps = SideGet (padw[i]);
         if (!ps)
            return FALSE;

         // need to average all the vertices together
         lPCPMVert.Clear();
         padwVert = ps->VertGet();
         for (j = 0; j < ps->m_wNumVert; j++) {
            pv = VertGet (padwVert[j]);
            if (!pv)
               return FALSE;
            lPCPMVert.Add (&pv);
         } // j

         // because have turned off symmetry, pretty easy to just add the point
         DWORD dwAdd;
         PCPMVert pNew;
         pNew = new CPMVert;
         if (!pNew)
            return FALSE;
         pNew->Average ((PCPMVert*)lPCPMVert.Get(0), NULL, lPCPMVert.Num(), padw[i], TRUE);
         dwAdd = m_lVert.Num();
         m_lVert.Add (&pNew);

         // add this vert
         lCenter.Add (&dwAdd);
      } // i
   } // if tess==1 or 2

   // dwTess == 2, find all the edges associated with the divisions
   // and create vertices for them
   CListFixed lEdge;
   lEdge.Init (sizeof(TESSEDGE));
   if (dwTess == 2) {
      // make sure have edges
      CalcEdges ();

      // figure out edges
      TESSEDGE te;
      PCPMEdge *ppe = (PCPMEdge*) m_lEdge.Get(0);
      memset (&te, 0, sizeof(te));
      for (i = 0; i < m_lEdge.Num(); i++) {
         // if the edge doesn't reference a subdivided side then ignore
         if ((DWORDSearch (ppe[i]->m_adwSide[0], dwNum, padw) == -1) &&
            (DWORDSearch (ppe[i]->m_adwSide[1], dwNum, padw) == -1))
            continue;

         memcpy (te.adwVert, ppe[i]->m_adwVert, sizeof(te.adwVert));
         memcpy (te.adwSide, ppe[i]->m_adwSide, sizeof(te.adwSide));

         // create new point. Since we have symmetry off dont have to worry
         // about extra info
         PCPMVert apv[2];
         PCPMVert pNew;
         apv[0] = VertGet (te.adwVert[0]);
         apv[1] = VertGet (te.adwVert[1]);
         pNew = new CPMVert;
         if (!pNew)
            return FALSE;
         pNew->Average (apv, NULL, 2, -1, TRUE);
         te.dwNewVert = m_lVert.Num();
         m_lVert.Add (&pNew);

         // add this edge
         lEdge.Add (&te);
      } // i

      // sort this by vertex number for easy reference later
      qsort (lEdge.Get(0), lEdge.Num(), sizeof(TESSEDGE), B2DWORDCompare);
   } // dwTess == 2

   // maintain a list of new sides that need to be selected
   CListFixed lToSel;
   lToSel.Init (sizeof(DWORD));
   DWORD dwAdd, k;

   DWORD adwNew[4];
   DWORD *padwCenter = (DWORD*)lCenter.Get(0);
   if (dwTess == 0) {
      // triangulate
      for (i = 0; i < dwNum; i++) {
         PCPMSide ps = SideGet(padw[i]);
         if (ps->m_wNumVert < 4)
            continue;   // must have enough sides
         padwVert = ps->VertGet();

         // make clones...
         for (j = 1; j+2 < ps->m_wNumVert; j++) {
            PCPMSide pNew = ps->Clone();
            if (!pNew)
               return FALSE;

            // set the sides
            adwNew[0] = padwVert[0];
            adwNew[1] = padwVert[j+1];
            adwNew[2] = padwVert[j+2];
            pNew->VertClear();
            pNew->VertAdd (adwNew, 3);

            // add
            dwAdd = m_lSide.Num();
            m_lSide.Add (&pNew);

            // go through vertices and fix remap based on sides
            for (k = 0; k < 3; k++) {
               TEXTUREPOINT tp;
               pv = VertGet(adwNew[k]);
               tp = *(pv->TextureGet(padw[i]));
               pv->TextureSet (dwAdd, &tp);
            }

            // if selected then remember
            if ((m_dwSelMode == 2) && (SelFind (padw[i]) != -1))
               lToSel.Add (&dwAdd);
         } // j

         // adjust the current side
         adwNew[0] = padwVert[0];
         adwNew[1] = padwVert[1];
         adwNew[2] = padwVert[2];
         ps->VertClear();
         ps->VertAdd (adwNew, 3);
      } // i
   } // dwTess==0

   else if (dwTess == 1) {
      // dividing into triangles using center point
      for (i = 0; i < dwNum; i++) {
         PCPMSide ps = SideGet(padw[i]);
         padwVert = ps->VertGet();

         // make clones...
         for (j = 1; j < ps->m_wNumVert; j++) {
            PCPMSide pNew = ps->Clone();
            if (!pNew)
               return FALSE;

            // set the sides
            adwNew[0] = padwCenter[i];
            adwNew[1] = padwVert[j];
            adwNew[2] = padwVert[(j+1)%ps->m_wNumVert];
            pNew->VertClear();
            pNew->VertAdd (adwNew, 3);

            // add
            dwAdd = m_lSide.Num();
            m_lSide.Add (&pNew);

            // go through vertices and fix remap based on sides
            // note: Don't bother with center point since will be the same for all tesselate
            for (k = 1; k < 3; k++) {
               TEXTUREPOINT tp;
               pv = VertGet(adwNew[k]);
               tp = *(pv->TextureGet(padw[i]));
               pv->TextureSet (dwAdd, &tp);
            }


            // if selected then remember
            if ((m_dwSelMode == 2) && (SelFind (padw[i]) != -1))
               lToSel.Add (&dwAdd);
         } // j

         // adjust the current side
         adwNew[0] = padwCenter[i];
         adwNew[1] = padwVert[0];
         adwNew[2] = padwVert[1];
         ps->VertClear();
         ps->VertAdd (adwNew, 3);
      } // i

   }  // dwTess==1

   else if (dwTess == 2) {
      // dividing into quads using center point
      DWORD adwEdge[2];
      PTESSEDGE pte;

      for (i = 0; i < dwNum; i++) {
         PCPMSide ps = SideGet(padw[i]);
         padwVert = ps->VertGet();

         // make clones...
         for (j = 1; j < ps->m_wNumVert; j++) {
            PCPMSide pNew = ps->Clone();
            if (!pNew)
               return FALSE;

            // set the sides
            adwNew[0] = padwCenter[i];
            adwNew[2] = padwVert[j];

            // find edge 1
            adwEdge[0] = adwEdge[1] = padwVert[(j+ps->m_wNumVert-1)%ps->m_wNumVert];
            adwEdge[0] = min(adwEdge[0], adwNew[2]);
            adwEdge[1] = max(adwEdge[1], adwNew[2]);
            pte = (PTESSEDGE) bsearch (adwEdge, lEdge.Get(0), lEdge.Num(), sizeof(TESSEDGE), B2DWORDCompare);
            if (!pte)
               return FALSE;
            adwNew[1] = pte->dwNewVert;

            // find edge 2
            adwEdge[0] = adwEdge[1] = padwVert[(j+1)%ps->m_wNumVert];
            adwEdge[0] = min(adwEdge[0], adwNew[2]);
            adwEdge[1] = max(adwEdge[1], adwNew[2]);
            pte = (PTESSEDGE) bsearch (adwEdge, lEdge.Get(0), lEdge.Num(), sizeof(TESSEDGE), B2DWORDCompare);
            if (!pte)
               return FALSE;
            adwNew[3] = pte->dwNewVert;

            pNew->VertClear();
            pNew->VertAdd (adwNew, 4);

            // add
            dwAdd = m_lSide.Num();
            m_lSide.Add (&pNew);

            // go through vertices and fix remap based on sides
            // note: Don't bother with center point since will be the same for all tesselate
            for (k = 1; k < 4; k++) {
               TEXTUREPOINT tp;
               pv = VertGet(adwNew[k]);
               tp = *(pv->TextureGet(padw[i]));
               pv->TextureSet (dwAdd, &tp);
            }


            // if selected then remember
            if ((m_dwSelMode == 2) && (SelFind (padw[i]) != -1))
               lToSel.Add (&dwAdd);
         } // j

         // adjust the current side
         j = 0;
         adwNew[0] = padwCenter[i];
         adwNew[2] = padwVert[j];
         // find edge 1
         adwEdge[0] = adwEdge[1] = padwVert[(j+ps->m_wNumVert-1)%ps->m_wNumVert];
         adwEdge[0] = min(adwEdge[0], adwNew[2]);
         adwEdge[1] = max(adwEdge[1], adwNew[2]);
         pte = (PTESSEDGE) bsearch (adwEdge, lEdge.Get(0), lEdge.Num(), sizeof(TESSEDGE), B2DWORDCompare);
         if (!pte)
            return FALSE;
         adwNew[1] = pte->dwNewVert;

         // find edge 2
         adwEdge[0] = adwEdge[1] = padwVert[(j+1)%ps->m_wNumVert];
         adwEdge[0] = min(adwEdge[0], adwNew[2]);
         adwEdge[1] = max(adwEdge[1], adwNew[2]);
         pte = (PTESSEDGE) bsearch (adwEdge, lEdge.Get(0), lEdge.Num(), sizeof(TESSEDGE), B2DWORDCompare);
         if (!pte)
            return FALSE;
         adwNew[3] = pte->dwNewVert;

         ps->VertClear();
         ps->VertAdd (adwNew, 4);
      } // i

      // go through all the edges looking for ones that reference a side which is NOT
      // also subdivided
      pte = (PTESSEDGE) lEdge.Get(0);
      CListFixed lVert;
      lVert.Init (sizeof(DWORD));
      for (i = 0; i < lEdge.Num(); i++, pte++) {
         DWORD dwNotIn = 2;
         if ((pte->adwSide[0] != -1) && (DWORDSearch(pte->adwSide[0], dwNum, padw) == -1))
            dwNotIn = 0;
         else if ((pte->adwSide[1] != -1) && (DWORDSearch(pte->adwSide[1], dwNum, padw) == -1))
            dwNotIn = 1;
         else
            continue;   // both in division

         // get side
         PCPMSide ps = SideGet (pte->adwSide[dwNotIn]);

         // get all the vertices...
         DWORD *padwVert;
         lVert.Init (sizeof(DWORD), ps->VertGet(), ps->m_wNumVert);
         padwVert = (DWORD*)lVert.Get(0);

         // find it
         for (j = 0; j < ps->m_wNumVert; j++) {
            // see if match
            if ((padwVert[j] == pte->adwVert[0]) && (padwVert[(j+ps->m_wNumVert-1)%ps->m_wNumVert] == pte->adwVert[1]))
               break;
            // or other way around
            if ((padwVert[j] == pte->adwVert[1]) && (padwVert[(j+ps->m_wNumVert-1)%ps->m_wNumVert] == pte->adwVert[0]))
               break;
         } // j
         if (j >= ps->m_wNumVert)
            continue;   // shouldnt happen

         // insert
         // BUGFIX - Never insert before item 0 since that will mess up symmetry
         if (j)
            lVert.Insert (j, &pte->dwNewVert);
         else
            lVert.Add (&pte->dwNewVert);

         // write out
         ps->VertClear();
         ps->VertAdd ((DWORD*)lVert.Get(0), lVert.Num());
      } // i

   }  // dwTess==2


   // reassocate verts with sides
   VertSideLinkRebuild();

   // note that edges are invalid
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   // rebuild symmetry
   m_dwSymmetry = dwSym;
   m_fDirtySymmetry = TRUE;
   SymmetryRecalc();

   // adjust selection
   padw = (DWORD*) lToSel.Get(0);
   for (i = 0; i < lToSel.Num(); i++)
      SelAdd (padw[i]);

   return TRUE;
}



/*****************************************************************************************
CPolyMesh::EdgeSplit - Splits a group of edges. This also splits the symmetrical edges.

NOTE: Other code within CPolyMesh relies on fact that edgeSplit adds newly split points
onto END of list.

inputs
   DWORD          dwNum - Number of sides
   DWORD          *padw - Pointer to an array of side indecies
   PCListFixed    plMirrorOnly - Normally this is NULL, but IF it's not null then it
                  contains a list of DWORDs to indicate the bitfields of what should
                  be mirrored. Those mirrors NOT indicated by the bitfields are ignored.
returns
   BOOL - TRUE if succeded
*/
BOOL CPolyMesh::EdgeSplit (DWORD dwNum, DWORD *padw, PCListFixed plMirrorOnly)
{
   // make sure symmetry implimented
   SymmetryRecalc ();

   // convert this list to include symmetry
   CListFixed lMirror;
   MirrorEdge (padw, dwNum, &lMirror, plMirrorOnly);
   padw = (DWORD*) lMirror.Get(0);
   dwNum = lMirror.Num();

   // calculate edges, if we need to add edges
   CalcEdges();

   // turn mirror off for now
   DWORD dwSym, i, j;
   dwSym = m_dwSymmetry;
   m_dwSymmetry = 0;
   m_fDirtySymmetry = TRUE;
   SymmetryRecalc();

   // find all the edges associated with the divisions
   // and create vertices for them
   CListFixed lEdge;
   lEdge.Init (sizeof(TESSEDGE));
   // make sure have edges
   CalcEdges ();

   // maintain a list of new edges that need to be selected
   CListFixed lToSel;
   lToSel.Init (2*sizeof(DWORD));

   // figure out edges
   TESSEDGE te;
   memset (&te, 0, sizeof(te));
   for (i = 0; i < dwNum; i++) {
      DWORD dwFind = EdgeFind (padw[i*2+0], padw[i*2+1]);
      if (dwFind == -1)
         continue;   // shoultn happen
      PCPMEdge pe = EdgeGet (dwFind);
      if (!pe)
         continue;   // shouldnt happen

      memcpy (te.adwVert, pe->m_adwVert, sizeof(te.adwVert));
      memcpy (te.adwSide, pe->m_adwSide, sizeof(te.adwSide));

      // create new point. Since we have symmetry off dont have to worry
      // about extra info
      PCPMVert apv[2];
      PCPMVert pNew;
      apv[0] = VertGet (te.adwVert[0]);
      apv[1] = VertGet (te.adwVert[1]);
      pNew = new CPMVert;
      if (!pNew)
         return FALSE;
      pNew->Average (apv, NULL, 2, -1, TRUE);
      te.dwNewVert = m_lVert.Num();
      m_lVert.Add (&pNew);

      // add this edge
      lEdge.Add (&te);

      // remember for selection
      if ((m_dwSelMode == 1) && (SelFind(padw[i*2+0], padw[i*2+1]) != -1)) {
         DWORD adwSel[2];
         adwSel[0] = min(te.adwVert[0], te.dwNewVert);
         adwSel[1] = max(te.adwVert[0], te.dwNewVert);
         lToSel.Add (padw);

         // and other part
         adwSel[0] = min(te.adwVert[1], te.dwNewVert);
         adwSel[1] = max(te.adwVert[1], te.dwNewVert);
         lToSel.Add (padw);
      }
   } // i

   // sort this by vertex number for easy reference later
   qsort (lEdge.Get(0), lEdge.Num(), sizeof(TESSEDGE), B2DWORDCompare);

   // go through all the edges looking for ones that reference a side which is NOT
   // also subdivided
   PTESSEDGE pte;
   pte = (PTESSEDGE) lEdge.Get(0);
   CListFixed lVert;
   DWORD dwe;
   lVert.Init (sizeof(DWORD));
   for (i = 0; i < lEdge.Num(); i++, pte++) {
      for (dwe = 0; dwe < 2; dwe++) {
         if (pte->adwSide[dwe] == -1)
            continue;

         // get side
         PCPMSide ps = SideGet (pte->adwSide[dwe]);

         // get all the vertices...
         DWORD *padwVert;
         lVert.Init (sizeof(DWORD), ps->VertGet(), ps->m_wNumVert);
         padwVert = (DWORD*)lVert.Get(0);

         // find it
         for (j = 0; j < ps->m_wNumVert; j++) {
            // see if match
            if ((padwVert[j] == pte->adwVert[0]) && (padwVert[(j+ps->m_wNumVert-1)%ps->m_wNumVert] == pte->adwVert[1]))
               break;
            // or other way around
            if ((padwVert[j] == pte->adwVert[1]) && (padwVert[(j+ps->m_wNumVert-1)%ps->m_wNumVert] == pte->adwVert[0]))
               break;
         } // j
         if (j >= ps->m_wNumVert)
            continue;   // shouldnt happen

         // insert
         // BUGFIX - Never insert before item 0 since that will mess up symmetry
         if (j)
            lVert.Insert (j, &pte->dwNewVert);
         else
            lVert.Add (&pte->dwNewVert);

         // write out
         ps->VertClear();
         ps->VertAdd ((DWORD*)lVert.Get(0), lVert.Num());
      } // dwe
   } // i


   // reassocate verts with sides
   VertSideLinkRebuild();

   // note that edges are invalid
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   // rebuild symmetry
   m_dwSymmetry = dwSym;
   m_fDirtySymmetry = TRUE;
   SymmetryRecalc();

   // adjust selection
   if (m_dwSelMode == 1) {
      SelClear ();

      // set new sel
      padw = (DWORD*) lToSel.Get(0);
      for (i = 0; i < lToSel.Num(); i++)
         SelAdd (padw[i*2+0], padw[i*2+1]);
   }

   return TRUE;
}


/*****************************************************************************************
CPolyMesh::SideSplit - Split a side between a point/edge and another point/edge.
This also handles the mirrors

inputs
   DWORD             dwSide - Side number
   DWORD             *padwSplit1 - First item to split. If it's a point padwSplit[0] is
                        the point number, and padwSplit[1]=-1. If it's an edge then neither
                        is -1

                        NOTE: The caller should make sure that don't split between the same
                        points, or the same edges, or an edge and a point on the edge.
   DWORD             *padwSplit2 - Second item to split. Same info as split1
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::SideSplit (DWORD dwSide, DWORD *padwSplit1, DWORD *padwSplit2)
{
   // make sure symmetry implimented
   SymmetryRecalc ();

   // get the side
   PCPMSide psOrig = SideGet(dwSide);
   PPMMIRROR pMirror = psOrig->MirrorGet();

   // keep a list of mirror only
   CListFixed lMirrorOnly;
   DWORD i;
   lMirrorOnly.Init (sizeof(DWORD));
   lMirrorOnly.Required (psOrig->m_wNumMirror);
   for (i = 0; i < psOrig->m_wNumMirror; i++)
      lMirrorOnly.Add (&pMirror[i].dwType);

   // if have any edges then split...
   CListFixed lEdge;
   lEdge.Init (sizeof(DWORD)*2);
   if (padwSplit1[1] != -1)
      lEdge.Add (padwSplit1);
   if (padwSplit2[1] != -1)
      lEdge.Add (padwSplit2);
   DWORD dwPreSplitNum = m_lVert.Num();
   if (lEdge.Num())
      EdgeSplit (lEdge.Num(), (DWORD*) lEdge.Get(0), &lMirrorOnly);

   // NOTE: Refresh pMirror after add edges, since may update mirroring
   pMirror = psOrig->MirrorGet();

   // go through the side and all its mirrors
   DWORD dwSplit, j, k, x;
   DWORD *padwSplit;
   CListFixed lNewVert;
   lNewVert.Init (sizeof(DWORD));
   for (i = 0; i <= psOrig->m_wNumMirror; i++) {
      DWORD dwCurSide = (i < psOrig->m_wNumMirror) ? pMirror[i].dwObject : dwSide;
      DWORD dwMirrorBits = (i < psOrig->m_wNumMirror) ? pMirror[i].dwType : 0;
      PCPMSide ps = SideGet(dwCurSide);
      if (!ps)
         continue;

      // mirror scale
      CPoint pMir;
      pMir.p[0] = (dwMirrorBits & 0x01) ? -1 : 1;
      pMir.p[1] = (dwMirrorBits & 0x02) ? -1 : 1;
      pMir.p[2] = (dwMirrorBits & 0x04) ? -1 : 1;

      // figure out the two split points, as indecies into the vertices
      DWORD *padwVert = ps->VertGet();
      DWORD adwSplitAt[2];
      for (dwSplit = 0; dwSplit < 2; dwSplit++) {
         padwSplit = dwSplit ? padwSplit2 : padwSplit1;

         if (padwSplit[1] == -1) { // split at a point
            PCPMVert pv = VertGet(padwSplit[0]);
            PPMMIRROR pVertMirror = pv->MirrorGet();
            CPoint pWant;
            pWant.Copy (&pv->m_pLoc);
            for (x = 0; x < 3; x++)
               pWant.p[x] *= pMir.p[x];

            // find a point that is < dwPreSplitNum and which is a mirror of the original
            // point
            for (j = 0; j < ps->m_wNumVert; j++) {
               if (padwVert[j] >= dwPreSplitNum)
                  continue;   // cant be it since this was added after split
               if (padwVert[j] == padwSplit[0])
                  break;   // found it

               // get point
               PCPMVert pvj = VertGet(padwVert[j]);
               if (!pvj->m_pLoc.AreClose (&pWant))
                  continue; // not what looking for

               // BUGFIX - Disable this because theoretically a check for AreClose()
               // should be good enough
               // And causing problems if leave in when doing a series of divide poly
               // BUGFIX - Renable
               k = 0;
               for (k = 0; k < pv->m_wNumMirror; k++)
                  if (pVertMirror[k].dwObject == padwVert[j])
                     break;
               if (k < pv->m_wNumMirror)
                  break;   // found it
            } // j

            if (j >= ps->m_wNumVert)
               adwSplitAt[dwSplit] = -1;   // if get here couldnt find the split
            else
               // else, split
               adwSplitAt[dwSplit] = j;
         }
         else { // split between an edge...
            PCPMVert apv[2];
            PPMMIRROR apVertMirror[2];
            apv[0] = VertGet(padwSplit[0]);
            apv[1] = VertGet(padwSplit[1]);
            apVertMirror[0] = apv[0]->MirrorGet();
            apVertMirror[1] = apv[1]->MirrorGet();
            CPoint apWant[2];
            apWant[0].Copy (&apv[0]->m_pLoc);
            apWant[1].Copy (&apv[1]->m_pLoc);
            for (x = 0; x < 3; x++) {
               apWant[0].p[x] *= pMir.p[x];
               apWant[1].p[x] *= pMir.p[x];
            }

            // find a point that is >= dwPreSplitNum and which is a mirror of the original
            // point
            for (j = 0; j < ps->m_wNumVert; j++) {
               if (padwVert[j] < dwPreSplitNum)
                  continue;   // cant be it since this was added before split

               // find left and right
               DWORD adwNear[2];
               adwNear[0] = padwVert[(j+ps->m_wNumVert-1)%ps->m_wNumVert];
               adwNear[1] = padwVert[(j+1)%ps->m_wNumVert];

               // get point
               PCPMVert pvj[2];
               pvj[0] = VertGet(adwNear[0]);
               pvj[1] = VertGet(adwNear[1]);

               // see if adwNear is a mirror of one of the originals
               DWORD dwFirstMirror = 2;
               if (adwNear[0] == padwSplit[0])
                  dwFirstMirror = 0;
               else if (adwNear[0] == padwSplit[1])
                  dwFirstMirror = 1;
               if ((dwFirstMirror == 2) && pvj[0]->m_pLoc.AreClose (&apWant[0])) {
                  // BUGFIX - Disable this because theoretically a check for AreClose()
                  // should be good enough
                  // And causing problems if leave in when doing a series of divide poly
                  // BUGFIX - Renable
                  //dwFirstMirror = 0;
                  for (k = 0; k < apv[0]->m_wNumMirror; k++)
                     if ((apVertMirror[0])[k].dwObject == adwNear[0]) {
                        dwFirstMirror = 0;
                        break;
                     }
                  }
               if ((dwFirstMirror == 2) && pvj[0]->m_pLoc.AreClose (&apWant[1])) {
                  // BUGFIX - Disable this because theoretically a check for AreClose()
                  // should be good enough
                  // And causing problems if leave in when doing a series of divide poly
                  // BUGFIX - Renable
                  // dwFirstMirror = 1;
                  for (k = 0; k < apv[1]->m_wNumMirror; k++)
                     if ((apVertMirror[1])[k].dwObject == adwNear[0]) {
                        dwFirstMirror = 1;
                        break;
                     }
                  }
               if (dwFirstMirror == 2)
                  continue;   // can't find mirror

               // see if next point also matches
               if (adwNear[1] == padwSplit[!dwFirstMirror])
                  break;   // match
               if (pvj[1]->m_pLoc.AreClose (&apWant[!dwFirstMirror])) {
                  // BUGFIX - Disable this because theoretically a check for AreClose()
                  // should be good enough.
                  // And causing problems if leave in when doing a series of divide poly
                  // BUGFIX - Renable
                  //break;
                  for (k = 0; k < apv[!dwFirstMirror]->m_wNumMirror; k++)
                     if ((apVertMirror[!dwFirstMirror])[k].dwObject == adwNear[1])
                        break; // found match
               }
               else
                  k = 1000000;   // so error
               if (k < apv[!dwFirstMirror]->m_wNumMirror)
                  break;   // found match

               // else if gets to here no match, so try again
            } // j
            if (j >= ps->m_wNumVert)
               adwSplitAt[dwSplit] = -1;  // cant find
            else
               // if get here then j contains the point where to split
               adwSplitAt[dwSplit] = j;
         } // if split between edge
      } // dwSplit

      if ((adwSplitAt[0] == -1) || (adwSplitAt[1] == -1))
         continue;

      // know the two places to split, create a new edge, starting at the first
      // split and working to the second
      DWORD dwStart, dwEnd;
      dwStart = adwSplitAt[0];
      dwEnd = adwSplitAt[1];
      if (dwEnd <= dwStart)
         dwEnd += ps->m_wNumVert;
      lNewVert.Clear();
      if (dwEnd >= dwStart)
         lNewVert.Required (dwEnd - dwStart + 1);
      for (j = dwStart; j <= dwEnd; j++)
         lNewVert.Add (&padwVert[j % ps->m_wNumVert]);

      // new side
      PCPMSide pNew;
      pNew = ps->Clone();
      if (!pNew)
         return FALSE;
      pNew->VertClear();
      pNew->VertAdd ((DWORD*) lNewVert.Get(0), lNewVert.Num());
      DWORD dwNewNum;
      dwNewNum = m_lSide.Num();
      m_lSide.Add (&pNew);

      // add texture remaps
      DWORD *padw2;
      padw2 = (DWORD*) lNewVert.Get(0);
      for (j = 0; j < lNewVert.Num(); j++) {
         PCPMVert pv = VertGet(padw2[j]);
         TEXTUREPOINT tp;
         tp = *(pv->TextureGet(dwCurSide));
         pv->TextureSet (dwNewNum, &tp);
      } // j

      // reset the original vertex to the opposite
      dwStart = adwSplitAt[1];
      dwEnd = adwSplitAt[0];
      if (dwEnd <= dwStart)
         dwEnd += ps->m_wNumVert;
      lNewVert.Clear();
      if (dwEnd >= dwStart)
         lNewVert.Required (dwEnd - dwStart + 1);
      for (j = dwStart; j <= dwEnd; j++)
         lNewVert.Add (&padwVert[j % ps->m_wNumVert]);
      ps->VertClear();
      ps->VertAdd ((DWORD*) lNewVert.Get(0), lNewVert.Num());
   } // i


   // reassocate verts with sides
   VertSideLinkRebuild();

   // note that edges are invalid
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   // rebuild symmetry
   m_fDirtySymmetry = TRUE;
   SymmetryRecalc();

   return TRUE;
}

/*****************************************************************************************
CPolyMesh::SideAddChecks - Adds a new side, but first does some checks to make sure that
the array of vertices are valid (enough points). It can also flip the direction depending
upoin what edges it finds.

inputs
   DWORD          dwNum - Number of vertices
   DWORD          *padwVert - Array of dwNum vertices
returns
   BOOL - TRUE if success, FALSE if error
*/
BOOL CPolyMesh::SideAddChecks (DWORD dwNum, DWORD *padwVert)
{
   // make sure symmetry implimented
   SymmetryRecalc ();

   if (dwNum < 3)
      return FALSE;

   CalcEdges ();

   // loop through all the edges
   DWORD i, dwFind, j;
   PCPMEdge pe;
   BOOL fFlip = FALSE;
   for (i = 0; i < dwNum; i++) {
      dwFind = EdgeFind (padwVert[i], padwVert[(i+1)%dwNum]);
      if (dwFind == -1)
         continue;
      pe = EdgeGet (dwFind);
      if (!pe)
         continue;

      // if the edge is double sided then error
      if (pe->m_adwSide[1] != -1)
         return FALSE;

      // see which direction the edge is going
      PCPMSide ps = SideGet(pe->m_adwSide[0]);
      if (!ps)
         continue;
      DWORD *padwSideVert = ps->VertGet();
      for (j = 0; j < ps->m_wNumVert; j++) {
         if (padwSideVert[j] != padwVert[i])
            continue;   // not point loiking for
         if (padwSideVert[(j+1)%ps->m_wNumVert] == padwVert[(i+1)%dwNum])
            fFlip = TRUE;
         else if (padwSideVert[(j+ps->m_wNumVert-1)%ps->m_wNumVert] == padwVert[(i+1)%dwNum])
            fFlip = FALSE;
      }
   }

   // create the list
   CListFixed lNew;
   lNew.Init (sizeof(DWORD));
   lNew.Required (dwNum);
   for (i = 0; i < dwNum; i++)
      lNew.Add (&padwVert[fFlip ? (dwNum - i - 1) : i]);

   // pick a surface
   DWORD dwSurface = 0;
   DWORD *padw = (DWORD*) lNew.Get(0);
   for (i = 0; i < lNew.Num(); i++) {
      PCPMVert pv = VertGet(padw[i]);
      DWORD *padwSide = pv->SideGet();
      for (j = 0; j < pv->m_wNumSide; j++) {
         PCPMSide ps = SideGet(padwSide[j]);
         dwSurface = ps->m_dwSurfaceText;
         break;
      } // j
      if (j < pv->m_wNumSide)
         break;
   } // i

   DWORD dwRet;
   dwRet = SideAdd (dwSurface, (DWORD*)lNew.Get(0), lNew.Num());
   return (dwRet != -1);
}




/*****************************************************************************************
CPolyMesh::SymmetrySideGroups - Takes a group of sides that are to have an operation done
on them and fills in several lists with mirrored groups. This can be used for collapse

inputs
   DWORD          dwNum - Number in the group
   DWORD          *padw - Sides in the original group. These must be sorted
   PCListFixed    paList - Pointer to an array of 8 lists. These lists will be filled
                        in with symmetry options. (Of course, if there's no symmetry turned
                        on some will be left empty). They will be initialized to sizeof(DWORD)
                        and filled in with sides indecies. Note that paList[0] is the same
                        as the original list.
returns
   BOOL - TRUE if all the resulting lists are independent, FALSE if there are sudes
            in common (which might cause problems if used with collapse)
*/
BOOL CPolyMesh::SymmetrySideGroups (DWORD dwNum, DWORD *padw, PCListFixed paList)
{
   // clear lists
   DWORD i;
   for (i = 0; i < 8; i++) {
      paList[i].Init (sizeof(DWORD));
      paList[i].Clear();
   }

   // set up first one to be copy of original
   paList[0].Init (sizeof(DWORD), padw, dwNum);

   // loop based on symmetry
   DWORD x,y,z,xMax,yMax,zMax;
   DWORD j;
   PMMIRROR *pMirror;
   CListFixed lTemp;
   lTemp.Init (sizeof(DWORD));
   xMax = (m_dwSymmetry & 0x01) ? 2 : 1;
   yMax = (m_dwSymmetry & 0x02) ? 2 : 1;
   zMax = (m_dwSymmetry & 0x04) ? 2 : 1;
   for (x = 0; x < xMax; x++) for (y = 0; y < yMax; y++) for (z = 0; z < zMax; z++) {
      if (!x && !y && !z)
         continue;
      lTemp.Clear();

      // loop over original points and add to lTemp
      for (i = 0; i < dwNum; i++) {
         PCPMSide ps = SideGet(padw[i]);
         if (!ps)
            continue;

         // what symmetry are we looking for?
         DWORD dwWant = (x ? 0x01 : 0) | (y ? 0x02 : 0) | (z ? 0x04 : 0);

         // BUGFIX - figure out is any symmetry is missing. If there isn't full
         // symmetry (maybe because side is centered) then ignore those bits
         // from dwWant
         DWORD dwMask = 0;
         pMirror = ps->MirrorGet();
         for (j = 0; j < ps->m_wNumMirror; j++)
            dwMask |= pMirror[j].dwType;

         // if dwWant is 0 then want this point, so use it
         if (!(dwWant & dwMask)) {
            lTemp.Add (&padw[i]);
            continue;
         }

         // look through mirrors
         for (j = 0; j < ps->m_wNumMirror; j++) {
            if (pMirror[j].dwType == (dwWant & dwMask)) {
               lTemp.Add (&pMirror[j].dwObject);
               break;
            }
         } // j

      } // i

      // sort the list
      SortListAndRemoveDup (1, &lTemp, &paList[x + y*2 + z*4]);
   } // xyz

   // go through all the lists... if find any lists that are exact matches
   // then they straddle the symmetries exactly so get rid of them
   for (i = 0; i < 8; i++) {
      if (!paList[i].Num())
         continue;   // nothing

      for (j = i+1; j < 8; j++) {
         if (paList[i].Num() != paList[j].Num())
            continue;   // no diff
         if (!memcmp(paList[i].Get(0), paList[j].Get(0), paList[i].Num() * sizeof(DWORD))) {
            // exact match
            paList[j].Clear();
            continue;
         }
      } // j
   } // i

   // test to see if any groups have overlapping elements... the easiest way to do this
   // in the code is to create a new list, remove duplicates, and see that number of
   // elements don't change
   CListFixed lTemp2;
   lTemp.Clear();
   for (i = 0; i < 8; i++) {
      DWORD *padwElem = (DWORD*) paList[i].Get(0);
      lTemp.Required (lTemp.Num() + paList[i].Num());
      for (j = 0; j < paList[i].Num(); j++)
         lTemp.Add (&padwElem[j]);
   } // i
   SortListAndRemoveDup (1, &lTemp, &lTemp2);
   return (lTemp.Num() == lTemp2.Num());
}


/*****************************************************************************************
CPolyMesh::SideToGroups - Given a list of sides (MUST be sorted) this organizes the
sides into contiguous groups.

inputs
   DWORD          dwNum - Number of sides
   DWORD          *padwSide - List of dwNum sides. MUST be sorted.
   PCListFixed    plSideGroup - This must be initialized to sizeof(PCListFixed). As
                  a new group is formed it a new CListFixed is created and its pointer is
                  added to the plSideGroup list. (These must be freed by the caller.)
                  The new list is initialized to sizeof(DWORD) and contains all the
                  sides in the group. NOTE: The elements in the sub-lists are sorted
                  by their side number.
returns
   non
*/
void CPolyMesh::SideToGroups (DWORD dwNum, DWORD *padwSide, PCListFixed plSideGroup)
{
   // remember the starting number of elements in plSideGroup so will just add onto the end
   DWORD dwStart = plSideGroup->Num();

   // loop through all the edges
   CalcEdges ();
   DWORD i, j, k, l;
   PCPMEdge *ppe = (PCPMEdge*) m_lEdge.Get(0);
   for (i = 0; i < m_lEdge.Num(); i++) {
      PCPMEdge pe = ppe[i];

      // see if the two sides are contained within the list of sides
      BOOL afIn[2];
      afIn[0] = (DWORDSearch (pe->m_adwSide[0], dwNum, padwSide) != -1);
      afIn[1] = (DWORDSearch (pe->m_adwSide[1], dwNum, padwSide) != -1);

      // if edge is not connected to any of the interested sides then skip
      if (!afIn[0] && !afIn[1])
         continue;

      // find the two edges in their respective group
      PCListFixed aplGroup[2];
      DWORD adwGroup[2];
      PCListFixed *paSideGroup = (PCListFixed*) plSideGroup->Get(0);
      PCListFixed pl;
      aplGroup[0] = aplGroup[1] = NULL;
      adwGroup[0] = adwGroup[1] = -1;
      for (j = 0; j < 2; j++) {
         if (!afIn[j])
            continue;
         for (k = dwStart; k < plSideGroup->Num(); k++) {
            pl = paSideGroup[k];
            DWORD *padwElem = (DWORD*)pl->Get(0);
            for (l = 0; l < pl->Num(); l++)
               if (padwElem[l] == pe->m_adwSide[j])
                  break;
            if (l < pl->Num()) {
               aplGroup[j] = pl;
               adwGroup[j] = k;
               break;
            }
         } // k
      } // j

      // if both sides exist
      if (aplGroup[0] && aplGroup[1]) {
         // if the groups are the same, which they should be, then continue
         if (adwGroup[0] == adwGroup[1])
            continue;

         // this edge indicates that they're connected, so merge the second group
         // in with the first
         DWORD *padwElem = (DWORD*) aplGroup[1]->Get(0);
         for (j = 0; j < aplGroup[1]->Num(); j++)
            aplGroup[0]->Add (&padwElem[j]);

         // delete the second group
         delete aplGroup[1];
         plSideGroup->Remove (adwGroup[1]);
         continue;
      }

      // if only one side exists
      if ((aplGroup[0] && !aplGroup[1]) || (aplGroup[1] && !aplGroup[0])) {
         DWORD dwSide = (aplGroup[0] ? 0 : 1);

         // if the second side isn't in then ignore
         if (!afIn[!dwSide])
            continue;

         // else, add the second side to the current group
         aplGroup[dwSide]->Add (&pe->m_adwSide[!dwSide]);
         continue;
      }

      // if neither side exists...
      // create a new list
      PCListFixed pNew = new CListFixed;
      if (!pNew)
         continue;
      pNew->Init (sizeof(DWORD));

      // see about adding the first side
      if (afIn[0])
         pNew->Add (&pe->m_adwSide[0]);

      // and the second
      if (afIn[1])
         pNew->Add (&pe->m_adwSide[1]);

      plSideGroup->Add (&pNew);
   } // i

   // while at it, sort all the added groups
   PCListFixed *paSideGroup = (PCListFixed*) plSideGroup->Get(0);
   for (i = dwStart;i < plSideGroup->Num(); i++)
      qsort (paSideGroup[i]->Get(0), paSideGroup[i]->Num(), sizeof(DWORD), BDWORDCompare);
}


/*****************************************************************************************
CPolyMesh::SideGroupToEdges - Given a list of sides (that are assumed to be contiguous
and resulting from SideToGroups()), this determines all of the edges that are 
to the actual sides in the group.

inputs
   DWORD          dwNum - Number of sides
   DWORD          *padwSide - Array of dwNum sides. This MUST be sorted
   PCListFixed    plEdge - Initialized to 1*sizeof(DWORD) and filled with edges. This
                     is sorted. NOTE: These edges are indecies into the edge list.
   BOOL           fIgnoreSingleEdge - If TRUE then ignore edges that are only
                     attached to one side.
returns
   none
*/
void CPolyMesh::SideGroupToEdges (DWORD dwNum, DWORD *padwSide, PCListFixed plEdge,
                                  BOOL fIgnoreSingleEdge)
{
   // init
   plEdge->Init (sizeof(DWORD));

   // loop through all the edges
   CalcEdges ();
   DWORD i;
   PCPMEdge *ppe = (PCPMEdge*) m_lEdge.Get(0);
   for (i = 0; i < m_lEdge.Num(); i++) {
      PCPMEdge pe = ppe[i];

      if (fIgnoreSingleEdge && ((pe->m_adwSide[0] == -1) || (pe->m_adwSide[1] == -1)))
         continue;

      // see if the two sides are contained within the list of sides
      BOOL afIn[2];
      afIn[0] = (DWORDSearch (pe->m_adwSide[0], dwNum, padwSide) != -1);
      afIn[1] = (DWORDSearch (pe->m_adwSide[1], dwNum, padwSide) != -1);

      // if edge is not connected to any of the interested sides then skip
      // or, if both sides in don't really care
      if ((!afIn[0] && !afIn[1]) || (afIn[0] && afIn[1]))
         continue;

      // else, only one side in, so add this...
      plEdge->Add (&i);
   }
}


/*****************************************************************************************
CPolyMesh::SideGroupToPath - Given a list of sides (that are assumed to be contiguous
and resulting from SideToGroups()), this determines all of the edges that are attached
to the sides of the group. This then strings the edges together to form ONE (which is
why the sides group is contiguous) strand that loops around all the sides.

inputs
   DWORD          dwNum - Number of sides
   DWORD          *padwSide - Array of dwNum sides. This MUST be sorted
   PCListFixed    plVert - Initialized to 2*sizeof(DWORD) and filled with vertices [0]
                  and side number [1] in the clockwise direction of the given vertex.
                  Pass in the two elements so that can look at a side and see that it's
                  between two vertices.
                  The order is such that the loop will be pointing in the same
                  direction as the outside of the edges. The last element's vertex of the list
                  should be the same as the first element's vertex (assuming that a
                  complete loop is formed)
returns
   BOOL - TRUE if success, FALSE if no loop or too many loops
*/
BOOL CPolyMesh::SideGroupToPath (DWORD dwNum, DWORD *padwSide, PCListFixed plVert)
{
   // edges
   CListFixed lEdges;
   CalcEdges();
   SideGroupToEdges (dwNum, padwSide, &lEdges);
   DWORD *padwEdge = (DWORD*) lEdges.Get(0);
   PCPMEdge *ppe = (PCPMEdge*) m_lEdge.Get(0);

   // list of segments
   CListFixed lSegList;
   DWORD i, j, k;
   PCListFixed *plSegList;
   lSegList.Init (sizeof(PCListFixed));

   // loop through all the edges
   for (i = 0; i < lEdges.Num(); i++) {
      PCPMEdge pe = ppe[padwEdge[i]];

      // figure out which side it's on
      BOOL afIn[2];
      afIn[0] = (DWORDSearch (pe->m_adwSide[0], dwNum, padwSide) != -1);
      afIn[1] = (DWORDSearch (pe->m_adwSide[1], dwNum, padwSide) != -1);
      if ((!afIn[0] && !afIn[1]) || (afIn[0] && afIn[1]))
         continue;   // shouldnt happen, but just in case
      DWORD dwSide = (afIn[0] ? 0 : 1);

      // get the side
      PCPMSide ps = SideGet(pe->m_adwSide[dwSide]);
      if (!ps)
         continue;

      // find the edge
      BOOL fFlip;
      DWORD dwIndex = FindEdgeIndex (ps->m_wNumVert, ps->VertGet(), pe->m_adwVert, &fFlip);
      if (dwIndex == -1)
         continue;   // shouldnt happen

      // what are we looking for?
      DWORD adwWant[2];
      DWORD adwElem[2];
      if (fFlip) {
         adwWant[0] = pe->m_adwVert[1];
         adwWant[1] = pe->m_adwVert[0];
      }
      else {
         adwWant[0] = pe->m_adwVert[0];
         adwWant[1] = pe->m_adwVert[1];
      }
      
      // loop through the existing elements in lSegList and see if can insert before
      // or after
      plSegList = (PCListFixed*) lSegList.Get(0);
      for (j = 0; j < lSegList.Num(); j++) {
         DWORD *padwSeg = (DWORD*) plSegList[j]->Get(0);
         if (padwSeg[0*2+0] == adwWant[1]) {
            // insert before the current item
            adwElem[0] = adwWant[0];
            adwElem[1] = pe->m_adwSide[dwSide];
            plSegList[j]->Insert (0, adwElem);
            break;
         }
         if (padwSeg[(plSegList[j]->Num()-1)*2+0] == adwWant[0]) {
            // add onto the end
            padwSeg[(plSegList[j]->Num()-1)*2+1] = pe->m_adwSide[dwSide];  // so associate side with vertex
            adwElem[0] = adwWant[1];
            adwElem[1] = pe->m_adwSide[dwSide]; // doesnt really matter
            plSegList[j]->Add (adwElem);
            break;
         }
      } // j
      if (j < lSegList.Num())
         continue;   // found and added already

      // if get here need to add new segment
      PCListFixed lNew = new CListFixed;
      if (!lNew)
         continue;
      lNew->Init (sizeof(DWORD)*2);
      adwElem[0] = adwWant[0];
      adwElem[1] = pe->m_adwSide[dwSide];
      lNew->Add (adwElem);
      adwElem[0] = adwWant[1];
      lNew->Add (adwElem);

      lSegList.Add (&lNew);
   } // i

   // go through all the segments and connect them...
recheck:
   plSegList = (PCListFixed*) lSegList.Get(0);
   for (i = 0; i < lSegList.Num(); i++) {
      DWORD *padw1 = (DWORD*) plSegList[i]->Get(0);
      DWORD dwNum1 = plSegList[i]->Num();

      for (j = 0; j < lSegList.Num(); j++) {
         if (i == j)
            continue;   // dont add to each other

         DWORD *padw2 = (DWORD*) plSegList[j]->Get(0);
         DWORD dwNum2 = plSegList[j]->Num();

         // see if the end of i is the start of j
         if (padw1[(dwNum1-1)*2+0] == padw2[0*2+0]) {
            // remove the last element of the first list
            plSegList[i]->Remove (dwNum1-1);

            // add everyhing from j
            for (k = 0; k < dwNum2; k++)
               plSegList[i]->Add (&padw2[k*2]);

            delete plSegList[j];
            lSegList.Remove (j);
            goto recheck;
         }

         // if get here then couldnt add j onto the end.
         // only need the one test since will eventually be able to add
         // one onto the end of the other until there's only one left
      } // j
   } // i

   // free all the segment lists
   BOOL fRet;
   fRet = FALSE;
   plSegList = (PCListFixed*) lSegList.Get(0);
   if (lSegList.Num() == 1) {
      plVert->Init (sizeof(DWORD)*2, plSegList[0]->Get(0), plSegList[0]->Num());
      fRet = TRUE;
   }
   for (i = 0; i < lSegList.Num(); i++)
      delete plSegList[i];

   return fRet;
}


/*****************************************************************************************
CPolyMesh::SideExtrudeOneGroup - This extrudes or insets one group.
   Internal function called by SideExtrude()

NOTE: This does NOT affect the mirror information nor the connections between verts and sides.

inputs
   DWORD          dwNum - Number of sides
   DWORD          *padwSide - Array of dwNum sides. MUST be sorted. (The must also be contiguous)
   DWORD          dwNumLoop - Number of elements in the loop around the gorup of sides.
   DWORD          *padwLoop - Array of dwNum elements. Each element is 2 * DWORD.
                  The 1st DWORD is the vertex number, the second is the side number to
                  the right (clockwise) of the loop. The last vertex in the loop
                  probably matches the first vertex in the loop.
   PCPoint        pNorm - Normal to extrude along
   PCPoint        pCOM - COM to average to
   DWORD          dwEffect - See SideExtrude()
   fp             fAmt - See SideExtrude()
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::SideExtrudeOneGroup (DWORD dwNum, DWORD *padwSide, DWORD dwNumLoop,
                                     DWORD *padwLoop, PCPoint pNorm, PCPoint pCOM,
                                     DWORD dwEffect, fp fAmt)
{
   // figure out what points are duplicated.
   CListFixed lUnSort;
   DWORD i;
   lUnSort.Init (sizeof(DWORD));
   for (i = 0; i < dwNumLoop; i++) {
      if ((i == dwNumLoop-1) && (padwLoop[i*2+0] == padwLoop[0*2+0]))
         continue;   // ignore if repeat on itself

      lUnSort.Add (&padwLoop[i*2+0]);
   } // i
   CListFixed lVertOrig;
   SortListAndRemoveDup (1, &lUnSort, &lVertOrig);
   DWORD *padwVertOrig = (DWORD*) lVertOrig.Get(0);

   // create new points
   CListFixed lVertNew;
   lVertNew.Init (sizeof(DWORD));
   for (i = 0; i < lVertOrig.Num(); i++) {
      PCPMVert pOrig = VertGet (padwVertOrig[i]);
      if (!pOrig)
         continue;   // error, shouldnt happen
      PCPMVert pNew = pOrig->Clone ();
      if (!pNew)
         continue;   // error, shouldnt happen

      // add
      DWORD dwNewNum;
      dwNewNum = m_lVert.Num();
      m_lVert.Add (&pNew);
      lVertNew.Add (&dwNewNum);
   }
   DWORD *padwVertNew = (DWORD*) lVertNew.Get(0);

   // create new sides
   DWORD adwVert[4];
   DWORD j;
   for (i = 0; i+1 < dwNumLoop; i++) {
      PCPMSide psOrig = SideGet (padwLoop[i*2+1]);
      if (!psOrig)
         continue;
      PCPMSide pNew = psOrig->Clone ();
      if (!pNew)
         continue;

      // make a quadrilateral connecting
      adwVert[0] = padwLoop[i*2+0];
      adwVert[1] = padwLoop[(i+1)*2+0];
      adwVert[2] = padwVertNew[DWORDSearch (adwVert[1], lVertOrig.Num(), padwVertOrig)];
      adwVert[3] = padwVertNew[DWORDSearch (adwVert[0], lVertOrig.Num(), padwVertOrig)];

      // reset the vertices on the new point
      pNew->VertClear();
      pNew->VertAdd (adwVert, 4);

      // add
      DWORD dwNewSide;
      dwNewSide = m_lSide.Num();
      m_lSide.Add (&pNew);

      // in original vertices, if have special texture for one of the original
      // sides then need to rename to new side
      // in new vertices, if reference one of the original sides then must also
      // reference new side with same value
      // NOTE: This may releave vertex remembering texture for original side also,
      // but shouldnt matter if leave around since will eventually be cleaned up
      for (j = 0; j < 4; j++) {
         PCPMVert pv = VertGet(adwVert[j]);

         PTEXTUREPOINT ptp1, ptp2;
         TEXTUREPOINT tp;
         ptp1 = pv->TextureGet(-1);
         ptp2 = pv->TextureGet(padwLoop[i*2+1]);
         if (ptp1 != ptp2) {
            tp = *ptp2;
            pv->TextureSet (dwNewSide, &tp, TRUE); // force add
         }
      }

   } // i

   // loop through the existing sides and tell them to use the new vertices
   lUnSort.Init (sizeof(DWORD));
   for (i = 0; i < dwNum; i++) {
      PCPMSide ps = SideGet (padwSide[i]);
      if (!ps)
         continue;

      ps->VertRename (lVertOrig.Num(), padwVertOrig, padwVertNew);

      // while at it keep track of points
      DWORD *padwVert = ps->VertGet();
      lUnSort.Required (lUnSort.Num() + ps->m_wNumVert);
      for (j = 0; j < ps->m_wNumVert; j++) {
         lUnSort.Add (&padwVert[j]);
      } // j
   } // i


   // sort and remove duplicates in the points - euse vertorig since wont need from now on
   SortListAndRemoveDup (1, &lUnSort, &lVertOrig);
   padwVertOrig = (DWORD*) lVertOrig.Get(0);

   // loop and modify all the points
   for (i = 0; i < lVertOrig.Num(); i++) {
      PCPMVert pv = VertGet(padwVertOrig[i]);

      // offset
      CPoint pTemp;
      // NOTE: for extrusion dont take deformatios into account
      pTemp.Copy (&pv->m_pLoc);

      if (dwEffect == 0) { // extrude
         pTemp.Copy (pNorm);
         pTemp.Scale (fAmt);
      }
      else {   // dwEffect = 1, inset
         pTemp.Subtract (pCOM);
         pTemp.Scale (fAmt-1);
      }

      pv->DeformChanged (-1, &pTemp);
      // NOTE: Passing -1 to deformChanged because in extrusion it's a bit
      // uncertain about how should work
   }


   return TRUE;
}

/*****************************************************************************************
CPolyMesh::SideExtrude - This takes a list of sides and extrudes (or insets) them. It
also extrudes/insets the mirrors.

inputs
   DWORD          dwNum - Number of sides
   DWORD          *padwSide - Array of dwNumSides
   DWORD          dwEffect - 0 for extrude, 1 for inset
   BOOL           fPiecewise - If TRUE then sides are added piecewise, so that
                  they are extruded or inset individually
   fp             fAmt - In the case of extrusions, this is the numbere of meters
                  to extrude, for inset this is the amount to scale.
returns
   BOOL - TRUE if success. May fail if the extusion breaks the mirroring
*/
BOOL CPolyMesh::SideExtrude (DWORD dwNum, DWORD *padwSide, DWORD dwEffect, BOOL fPiecewise, fp fAmt)
{
   // make sure symmetry implimented
   SymmetryRecalc ();

   // if the amount of change is very small do nothing
   switch (dwEffect) {
   case 0:  // extrude
   default:
      if (fabs(fAmt) < CLOSE)
         return TRUE;
      break;
   case 1:  // inset
      if (fabs(fAmt - 1) < CLOSE)
         return TRUE;
      break;
   }

   CListFixed lSideGroups;
   lSideGroups.Init (sizeof(PCListFixed));
   DWORD i;
   if (fPiecewise) {
      CListFixed lMirror;
      MirrorSide (padwSide, dwNum, &lMirror);
      DWORD *padw = (DWORD*) lMirror.Get(0);
      lSideGroups.Required (lMirror.Num());
      for (i = 0; i < lMirror.Num(); i++) {
         PCListFixed pNew = new CListFixed;
         if (!pNew)
            continue;
         pNew->Init (sizeof(DWORD), &padw[i], 1);
         lSideGroups.Add (&pNew);
      }
   }
   else {
      // get the symmetry elements
      CListFixed alSym[8];
      if (!SymmetrySideGroups (dwNum, padwSide, alSym))
         return FALSE;

      // convert this to side groups
      for (i = 0; i < 8; i++) {
         if (!alSym[i].Num())
            continue;

         SideToGroups (alSym[i].Num(), (DWORD*) alSym[i].Get(0), &lSideGroups);
      }
   }  // if piecewise
   PCListFixed *plSideGroups = (PCListFixed*) lSideGroups.Get(0);

   // go through each of the side groups and determine the loop...
   CListFixed lSideLoops;
   lSideLoops.Init (sizeof(PCListFixed));
   lSideLoops.Required (lSideGroups.Num());
   for (i = 0; i < lSideGroups.Num(); i++) {
      PCListFixed pNew = new CListFixed;
      if (!pNew)
         continue;   // shouldnt happen

      SideGroupToPath (plSideGroups[i]->Num(), (DWORD*) plSideGroups[i]->Get(0), pNew);

      // add this
      lSideLoops.Add (&pNew);
   } // i
   PCListFixed *plSideLoops = (PCListFixed*) lSideLoops.Get(0);

   // figure out the COM or normal, depending upon what looking for
   CListFixed lCOM, lNorm;
   CPoint pTemp;
   lCOM.Init (sizeof(CPoint));
   lNorm.Init (sizeof(CPoint));
   lCOM.Required (lSideGroups.Num());
   lNorm.Required (lSideGroups.Num());
   for (i = 0; i < lSideGroups.Num(); i++) {
      GroupCOM (2, plSideGroups[i]->Num(), (DWORD*) plSideGroups[i]->Get(0), &pTemp, FALSE);
      lCOM.Add (&pTemp);

      GroupNormal (2, plSideGroups[i]->Num(), (DWORD*) plSideGroups[i]->Get(0), &pTemp, FALSE);
      lNorm.Add (&pTemp);
   } // i
   PCPoint pCOM = (PCPoint) lCOM.Get(0);
   PCPoint pNorm = (PCPoint) lNorm.Get(0);

   // loop through all the groups
   for (i = 0; i < lSideGroups.Num(); i++)
      SideExtrudeOneGroup (plSideGroups[i]->Num(), (DWORD*) plSideGroups[i]->Get(0),
         plSideLoops[i]->Num(), (DWORD*) plSideLoops[i]->Get(0),
         pNorm +i, pCOM + i, dwEffect, fAmt);

   // free memory
   for (i = 0; i < lSideGroups.Num(); i++)
      delete plSideGroups[i];
   for (i = 0; i < lSideLoops.Num(); i++)
      delete plSideLoops[i];

   // reassocate verts with sides
   VertSideLinkRebuild();

   // note that edges are invalid
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   // rebuild symmetry
   m_fDirtySymmetry = TRUE;
   SymmetryRecalc();

   return TRUE;
}




/*****************************************************************************************
CPolyMesh::SideDisconnectOneGroup - This disconeects one group.
   Internal function called by SideExtrude()

NOTE: This does NOT affect the mirror information nor the connections between verts and sides.

inputs
   DWORD          dwNum - Number of sides
   DWORD          *padwSide - Array of dwNum sides. MUST be sorted. (The must also be contiguous)
   DWORD          dwNumLoop - Number of elements in the loop around the gorup of sides.
   DWORD          *padwLoop - Array of dwNum elements. Each element is 2 * DWORD.
                  The 1st DWORD is the vertex number, the second is the side number to
                  the right (clockwise) of the loop. The last vertex in the loop
                  probably matches the first vertex in the loop.
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::SideDisconnectOneGroup (DWORD dwNum, DWORD *padwSide, DWORD dwNumLoop,
                                     DWORD *padwLoop)
{
   // figure out what points are duplicated.
   CListFixed lUnSort;
   DWORD i, j;
   lUnSort.Init (sizeof(DWORD));
   for (i = 0; i < dwNumLoop; i++) {
      if ((i == dwNumLoop-1) && (padwLoop[i*2+0] == padwLoop[0*2+0]))
         continue;   // ignore if repeat on itself

      lUnSort.Add (&padwLoop[i*2+0]);
   } // i
   CListFixed lVertOrig;
   SortListAndRemoveDup (1, &lUnSort, &lVertOrig);
   DWORD *padwVertOrig = (DWORD*) lVertOrig.Get(0);

   // create new points
   CListFixed lVertNew;
   lVertNew.Init (sizeof(DWORD));
   m_lVert.Required (m_lVert.Num() + lVertOrig.Num());
   lVertNew.Required (lVertOrig.Num());
   for (i = 0; i < lVertOrig.Num(); i++) {
      PCPMVert pOrig = VertGet (padwVertOrig[i]);
      if (!pOrig)
         continue;   // error, shouldnt happen
      PCPMVert pNew = pOrig->Clone ();
      if (!pNew)
         continue;   // error, shouldnt happen

      // add
      DWORD dwNewNum;
      dwNewNum = m_lVert.Num();
      m_lVert.Add (&pNew);
      lVertNew.Add (&dwNewNum);
   }
   DWORD *padwVertNew = (DWORD*) lVertNew.Get(0);

   // loop through the existing sides and tell them to use the new vertices
   lUnSort.Init (sizeof(DWORD));
   for (i = 0; i < dwNum; i++) {
      PCPMSide ps = SideGet (padwSide[i]);
      if (!ps)
         continue;

      ps->VertRename (lVertOrig.Num(), padwVertOrig, padwVertNew);

      // while at it keep track of points
      DWORD *padwVert = ps->VertGet();
      lUnSort.Required (lUnSort.Num() + ps->m_wNumVert);
      for (j = 0; j < ps->m_wNumVert; j++) {
         lUnSort.Add (&padwVert[j]);
      } // j
   } // i


   // sort and remove duplicates in the points - euse vertorig since wont need from now on
   SortListAndRemoveDup (1, &lUnSort, &lVertOrig);
   padwVertOrig = (DWORD*) lVertOrig.Get(0);


   return TRUE;
}


/*****************************************************************************************
CPolyMesh::SideDisconnect - This takes a list of sides and disconnects them from any
other sides they're a part of. It
also extrudes/insets the mirrors.

inputs
   DWORD          dwNum - Number of sides
   DWORD          *padwSide - Array of dwNumSides
returns
   BOOL - TRUE if success. May fail if the extusion breaks the mirroring
*/
BOOL CPolyMesh::SideDisconnect (DWORD dwNum, DWORD *padwSide)
{
   // make sure symmetry implimented
   SymmetryRecalc ();

   CListFixed lSideGroups;
   lSideGroups.Init (sizeof(PCListFixed));
   DWORD i;
   // get the symmetry elements
   CListFixed alSym[8];
   if (!SymmetrySideGroups (dwNum, padwSide, alSym))
      return FALSE;

   // convert this to side groups
   for (i = 0; i < 8; i++) {
      if (!alSym[i].Num())
         continue;

      SideToGroups (alSym[i].Num(), (DWORD*) alSym[i].Get(0), &lSideGroups);
   }
   PCListFixed *plSideGroups = (PCListFixed*) lSideGroups.Get(0);

   // go through each of the side groups and determine the loop...
   CListFixed lSideLoops;
   lSideLoops.Init (sizeof(PCListFixed));
   lSideLoops.Required (lSideGroups.Num());
   for (i = 0; i < lSideGroups.Num(); i++) {
      PCListFixed pNew = new CListFixed;
      if (!pNew)
         continue;   // shouldnt happen

      SideGroupToPath (plSideGroups[i]->Num(), (DWORD*) plSideGroups[i]->Get(0), pNew);

      // add this
      lSideLoops.Add (&pNew);
   } // i
   PCListFixed *plSideLoops = (PCListFixed*) lSideLoops.Get(0);

   // loop through all the groups
   for (i = 0; i < lSideGroups.Num(); i++)
      SideDisconnectOneGroup (plSideGroups[i]->Num(), (DWORD*) plSideGroups[i]->Get(0),
         plSideLoops[i]->Num(), (DWORD*) plSideLoops[i]->Get(0));

   // free memory
   for (i = 0; i < lSideGroups.Num(); i++)
      delete plSideGroups[i];
   for (i = 0; i < lSideLoops.Num(); i++)
      delete plSideLoops[i];

   // reassocate verts with sides
   VertSideLinkRebuild();

   // delete dead vertices
   VertDeleteDead ();

   // note that edges are invalid
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   // rebuild symmetry
   m_fDirtySymmetry = TRUE;
   SymmetryRecalc();

   return TRUE;
}

/*****************************************************************************************
CPolyMesh::SideCutBevel - This is an internal function that either cuts a bevel off
a side, or a corner (bevel from same point) off a side.

NOTE: Links between vertices and sides is very important for the bevelleing functions to work.
This function keeps those links up to date.

inputs
   DWORD          dwSide - Side to modify
   DWORD          *padwEdgeVert - Pointer to an array of 2 dwords that describe the
                  edge that is to be bevelled. If only a point is to be bevelled
                  then then second parameter is -1
   fp             fRadius - Radius of the bevel
   PCListFixed    plBevelFinished - List of bevels that are already finished; they
                  are still relevent for calculations. The list is stored as 2 * sizeof(DOWRD)
                  They MUST be sorted. padwEdgeVert isn't in lBevelFinished and isn't in lBevelQueue.
                  When the side is beveled, new edges will be formed (and old ones renamed).
                  This function will do that by modifying and then resorting these lists.
   PCListFixed    plBevelQueue - List of bevels in the queue. Same info as lBevelFinished
   DWORD          dwVertAutoAccept - If a vertex number >= this value is found adjacent
                  to the vertex we're bevelling from the automatically make that the
                  split vertex since it was created by another bevel process.
   BOOL           fAutoAddEdge - If TRUE, then calling this while extending edges. If a new
                  point is created then an edge is automatically added so that can remove it
                  later on.
returns
   DWORD - ID of new side (or even old side) that is now part of the bevel. Or -1 if nothing
            is.
*/
DWORD CPolyMesh::SideCutBevel (DWORD dwSide, DWORD *padwEdgeVert, fp fRadius,
                               PCListFixed plBevelFinished, PCListFixed plBevelQueue,
                               DWORD dwVertAutoAccept, BOOL fAutoAddEdge)
{
   PCPMSide ps = SideGet(dwSide);
   if (!ps)
      return -1;
   DWORD *padwVert = ps->VertGet();

   DWORD dwIndex1, dwIndex2;
   DWORD i;
   if (padwEdgeVert[1] != -1) {
      BOOL fFlip;
      dwIndex1 = FindEdgeIndex (ps->m_wNumVert, padwVert, padwEdgeVert, &fFlip);
      if (dwIndex1 == -1)
         return -1; // cant find

      // else
      dwIndex2 = dwIndex1+1;   // which means second index could be modulo
   }
   else {
      dwIndex1 = ps->VertFind (padwEdgeVert[0]);
      if (dwIndex1 == -1)
         return -1;  // cant find
      dwIndex2 = dwIndex1;
   }
   dwIndex1 += ps->m_wNumVert;   // so no modulo problems
   dwIndex2 += ps->m_wNumVert;

   // figure out the points for the cylinder
   PCPMVert pv1, pv2;
   PCPoint pC1, pC2;
   pv1 = VertGet (padwVert[dwIndex1 % ps->m_wNumVert]);
   pv2 = VertGet (padwVert[dwIndex2 % ps->m_wNumVert]);
   pC1 = &pv1->m_pLoc;  // NOTE: Using m_pLoc for bevel, which means it ignores deformations. But I think this is right
   pC2 = &pv2->m_pLoc;

   // loop to the left and right to find out where end up with a line that leaves
   // intersection. Creeps out left/right until gets outside the cylinder or
   // completely meets up
   DWORD dwStart, dwEnd, dwRight;
   dwStart = dwIndex1;
   dwEnd = dwIndex2;
   for (dwRight = 0; dwRight < 2; dwRight++) {
      while (TRUE) {
         DWORD dwA = (dwRight ? dwEnd : dwStart);
         DWORD dwB = (dwRight ? (dwA+1) : (dwA-1));
         DWORD dwAMod = (dwA % ps->m_wNumVert);
         DWORD dwBMod = (dwB % ps->m_wNumVert);
         BOOL fLastChance = (dwBMod == ((dwRight ? dwStart : dwEnd) % ps->m_wNumVert));
            // fLastChance if set it the direction we're headed is about to touch ends with
            // the other direction

         // potentially autoaccept B
         if (padwVert[dwBMod] >= dwVertAutoAccept) {
            if (fLastChance)
               return dwSide;

            if (dwRight)
               dwEnd++;
            else
               dwStart--;

            break;
         }

         // see if the edge dwA to dwB is within 
         DWORD adwEdge[2];
         padwVert = ps->VertGet();  // reload in case added vertex to side - which is likely enough
         adwEdge[0] = min(padwVert[dwAMod], padwVert[dwBMod]);
         adwEdge[1] = max(padwVert[dwAMod], padwVert[dwBMod]);

         // get the points from the new edge
         CPoint pD;
         pv1 = VertGet (padwVert[dwAMod]);
         pv2 = VertGet (padwVert[dwBMod]);
         pD.Subtract (&pv2->m_pLoc, &pv1->m_pLoc);

         // if we're looking to split around a point...
         fp afAlpha[2];
         DWORD dwInter;
         if (dwIndex1 == dwIndex2) {
            CPoint pDNorm;
            pDNorm.Copy (&pD);
            fp fLen = pDNorm.Length();
            if (fLen)
               pDNorm.Scale (1.0 / fLen);
            dwInter = IntersectRaySpehere (&pv1->m_pLoc, &pDNorm, pC1, fRadius, &afAlpha[0], &afAlpha[1]);
            if (dwInter == 2) {
               afAlpha[0] /= fLen;
               afAlpha[1] /= fLen;
            }
            else if (dwInter == 1) {
               afAlpha[0] /= fLen;
            }
         }
         else {
            // splitting around an edge
            dwInter = IntersectRayCylinder (&pv1->m_pLoc, &pv2->m_pLoc, pC1, pC2,
               fRadius, &afAlpha[0], &afAlpha[1]);
         }

         // if an intersection is beyond 0..1 range then dont care
         if ((dwInter == 2) && ((afAlpha[1] < -CLOSE) || (afAlpha[1] > 1.0+CLOSE)))
            dwInter = 1;
         if ((dwInter >= 1) && ((afAlpha[0] < -CLOSE) || (afAlpha[0] > 1.0+CLOSE))) {
            afAlpha[0] = afAlpha[1];
            dwInter--;
         }
         if (!dwInter) {
            // didn't intersect at all, so continue on to next point and hope that
            // leaves. EXCEPT if we cant continue on because we'd wrap entirely around,
            // in which case we have a tivial accept of entire polygon into bevel
            if (fLastChance)
               return dwSide;
            if (dwRight)
               dwEnd = dwB;
            else
               dwStart = dwB;
            continue;
         }

         // else, intersected...
         fp fInter;
         fInter = (dwInter == 2) ? min(afAlpha[0], afAlpha[1]) : afAlpha[0];

         // if this is near 0 or near 1 then trivial escape
         DWORD dwNewVert;
         if (fInter < CLOSE)
            dwNewVert = padwVert[dwAMod];
         else if (fInter > 1.0 - CLOSE)
            dwNewVert = padwVert[dwBMod];
         else {
            // what is the location in space
            CPoint pLoc;
            pLoc.Copy (&pD);
            pLoc.Scale (fInter);
            pLoc.Add (&pv1->m_pLoc);

            // find the closest point
            dwNewVert = VertFind (&pLoc);

            if (dwNewVert == -1) {
               // need to create vertex
               PCPMVert pNew = new CPMVert;
               if (!pNew)
                  return FALSE;
               PCPMVert apv[2];
               fp afScale[2];
               apv[0] = pv1;
               apv[1] = pv2;
               afScale[0] = 1.0 - fInter;
               afScale[1] = fInter;
               pNew->Average (apv, afScale, 2, dwSide, TRUE);

               pNew->SideClear();

               dwNewVert = m_lVert.Num();
               m_lVert.Add (&pNew);

               if (fAutoAddEdge) {
                  // BUGFIX - So that automatic extensions get bevelled properly, make sure
                  // that plBevelFinished has the edge, if not add it
                  DWORD adwEdge[2];
                  adwEdge[0] = min(padwVert[dwAMod], dwNewVert);
                  adwEdge[1] = max(padwVert[dwAMod], dwNewVert);
                  if (-1 == DWORD2Search (adwEdge, plBevelFinished->Num(), (DWORD*)plBevelFinished->Get(0))) {
                     plBevelFinished->Add (adwEdge);
                     qsort (plBevelFinished->Get(0), plBevelFinished->Num(), sizeof(DWORD)*2, B2DWORDCompare);
                  }
               }
            }

            // make sure this vertex knows that it's on this side...
            PCPMVert pv;
            pv = VertGet (dwNewVert);
            if (!pv)
               return -1;  // error
            DWORD *padwSide = pv->SideGet();
            for (i = 0; i < pv->m_wNumSide; i++)
               if (padwSide[i] == dwSide)
                  break;
            if (i >= pv->m_wNumSide)
               pv->SideAdd (&dwSide);
         }

         // if the new vertex is vertex A then trivial accept
         if (dwNewVert == padwVert[dwAMod])
            break;

         // if the new vertex is vertex B, then trivial accept UNLESS this
         // is the last chance (before a full wrap around), in which case just
         // return the side as all-inclusive of the bevel
         if (dwNewVert == padwVert[dwBMod]) {
            if (fLastChance)
               return dwSide;

            if (dwRight)
               dwEnd++;
            else
               dwStart--;
            break;
         }

         // else, need to insert the point in the side...

         // hack - add the point to the end then rearrange memory...
         DWORD dwOldNum = ps->m_wNumVert;
         DWORD adwOldEdge[2];
         DWORD dwInsertAt;
         adwOldEdge[0] = min(padwVert[dwAMod], padwVert[dwBMod]);
         adwOldEdge[1] = max(padwVert[dwAMod], padwVert[dwBMod]);
         dwInsertAt = (dwA < dwB) ? dwBMod : dwAMod;
         ps->VertInsert (dwNewVert, dwInsertAt);
         padwVert = ps->VertGet();  // relouad since may have been moved

         // remap dwIndex1 and 2, and dwStart and dwEnd to take into account new number of vertices
         //DWORD dw;
         //dw = dwIndex1 / dwOldNum;
         //dwIndex1 = (dwIndex1 - dw * dwOldNum) + dw * ps->m_wNumVert;
         // or, easier soln since know added one point
         dwIndex1 += (DWORD)(dwIndex1 / dwOldNum);
         dwIndex2 += (DWORD)(dwIndex2 / dwOldNum);
         dwStart += (DWORD)(dwStart / dwOldNum);
         dwEnd += (DWORD)(dwEnd / dwOldNum);
         dwA += (DWORD)(dwA / dwOldNum);
         dwB += (DWORD)(dwB / dwOldNum);

         // account for the inserted point
         if ((dwIndex1 % ps->m_wNumVert) >= dwInsertAt)
            dwIndex1++;
         if ((dwIndex2 % ps->m_wNumVert) >= dwInsertAt)
            dwIndex2++;
         if ((dwStart % ps->m_wNumVert) >= dwInsertAt)
            dwStart++;
         if ((dwEnd % ps->m_wNumVert) >= dwInsertAt)
            dwEnd++;

         // now, include the new point
         if (dwRight)
            dwEnd++;
         else
            dwStart--;

         // look through all the sides attached to the first point adwOldEdge[0],
         // and see if they include the side padwOldEdge. If they do, then insert
         // the vertex into them also
         PCPMVert pv;
         pv = VertGet(adwOldEdge[0]);
         DWORD *padwSide = pv->SideGet();
         for (i = 0; i < pv->m_wNumSide; i++) {
            PCPMSide psTest = SideGet(padwSide[i]);
            BOOL fFlip;
            DWORD dwFind = FindEdgeIndex (psTest->m_wNumVert, psTest->VertGet(), adwOldEdge, &fFlip);
            if (dwFind == -1)
               continue;

            // point...
            psTest->VertInsert (dwNewVert, dwFind+1);

            // make sure the new vert knows it's in that edge
            PCPMVert pvNew = VertGet (dwNewVert);
            pvNew->SideAdd (&padwSide[i]);
         }

         // since just inserted a point, go through existing edges and make changes
         for (i = 0; i < 2; i++) {
            PCListFixed pl = i ? plBevelFinished : plBevelQueue;
            DWORD dw = DWORD2Search (adwOldEdge, pl->Num(), (DWORD*)pl->Get(0));
            if (dw == -1)
               continue;

            DWORD *pdw = (DWORD*) pl->Get(dw);
            pdw[0] = min(adwOldEdge[0], dwNewVert);
            pdw[1] = max(adwOldEdge[0], dwNewVert);

            // BUGFIX - If have selection then modify that too
            DWORD dwSel;
            dwSel = -1;
            if (SelModeGet() == 1)
               dwSel = SelFind(adwOldEdge[0], adwOldEdge[1]);
            if (dwSel != -1) {
               SelRemove (dwSel);
               SelAdd (pdw[0], pdw[1]);
            }

            DWORD adwAdd[2];
            adwAdd[0] = min(adwOldEdge[1], dwNewVert);
            adwAdd[1] = max(adwOldEdge[1], dwNewVert);
            pl->Add (adwAdd);

            if (dwSel != -1)
               SelAdd (adwAdd[0], adwAdd[1]);

            qsort (pl->Get(0), pl->Num(), sizeof(DWORD)*2, B2DWORDCompare);
         }

         // done with this left/right direction
         break;
      } // while (TRUE)
   } // dwRight

   // create the new side
   PCPMSide pNewSide;
   pNewSide = ps->Clone();
   if (!pNewSide)
      return -1; // error
   DWORD dwNewSide;
   dwNewSide = m_lSide.Num();
   m_lSide.Add (&pNewSide);

   // tell the points that are going to switch sides about the new side
   for (i = dwStart; i <= dwEnd; i++) {
      PCPMVert pv = VertGet(padwVert[i % ps->m_wNumVert]);

      if ((i == dwStart) || (i == dwEnd)) {
         // need to add new side
         pv->SideAdd (&dwNewSide);

         PTEXTUREPOINT ptp1, ptp2;
         ptp1 = pv->TextureGet (-1);
         ptp2 = pv->TextureGet (dwSide);
         if (ptp1 != ptp2) {
            TEXTUREPOINT tp;
            tp = *ptp2;
            pv->TextureSet (dwNewSide, &tp, TRUE);
         }
      }
      else {
         pv->SideRename (1, &dwSide, &dwNewSide);
         pv->SideTextRename (1, &dwSide, &dwNewSide, FALSE);
      }
   }

   // create the points for the new side
   CListFixed lVert;
   lVert.Init (sizeof(DWORD));
   if (dwEnd >= dwStart)
      lVert.Required (dwEnd - dwStart + 1);
   for (i = dwStart; i <= dwEnd; i++)
      lVert.Add (&padwVert[i % ps->m_wNumVert]);
   pNewSide->VertClear();
   pNewSide->VertAdd ((DWORD*)lVert.Get(0), lVert.Num());

   // reset the current side so it has fewer points
   lVert.Clear();
   for (i = dwEnd; ; i++) {
      lVert.Add (&padwVert[i % ps->m_wNumVert]);

      // stop when get back to the start
      if ((i%ps->m_wNumVert) == (dwStart % ps->m_wNumVert))
         break;
   }
   ps->VertClear();
   ps->VertAdd ((DWORD*)lVert.Get(0), lVert.Num());

   return dwNewSide;
}


/*****************************************************************************************
BEVertIsAttachedToOrigSide - Given a vertex, this determines if it's attached to an original
side.

inputs
   DWORD          dwVert - Vertex in question
   PCPolyMesh     pm - Polymesh to use
   PCListFixed    plNewSide - List of new sides, must be sorted
returns
   BOOL - TRUE if attached to original side, FALSE if not
*/
BOOL BEVertIsAttachedToOrigSide (DWORD dwVert, PCPolyMesh pm, PCListFixed plNewSide)
{
   PCPMVert pv = pm->VertGet(dwVert);
   DWORD *padwSide = pv->SideGet();
   DWORD i;
   for (i = 0; i < pv->m_wNumSide; i++)
      if (-1 == DWORDSearch (padwSide[i], plNewSide->Num(), (DWORD*) plNewSide->Get(0)))
         return TRUE;

   // else not
   return FALSE;
}

/*****************************************************************************************
CPolyMesh::BevelEdges - Internal function that takes a list of edges (not worrying
about mirroring, and even trashing mirror information in the process) and makes cuts
(and new points) where all the bevels will go. It does not actually create the bevel
though.

NOTE: It's important that the list of sides that a vertex belongs to is up to date in
the vertex before calling this. This function will maintain that list as it adds/changes
vertices.

This call will mess up mirroing calcs, etc.

inputs
   fp                   fRadius - Radius of the bevel
   PCListFixed          plEdge - SHould contain a list of edges (2 * sizeof(DWORD)) that
                        want to bevel. When finished, will be filled with the same list
                        of edges, but including splits that occur.
   PCListFixed          plVert - Should contain a list of vertices (sizeof(DWORD)) that
                        should be split. Generally only plVert or plEdge will have elements
   PCListFixed          plSideNew - This function will initialize this to sizeof(DWORD) and
                        then fill it with all the new sides that were added
   PCListFixed          plVertToDelete - This will be filled in with all the vertices to
                        delete. It will be sorted.
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::BevelEdges (fp fRadius, PCListFixed plEdge, PCListFixed plVert, PCListFixed plSideNew,
                            PCListFixed plVertToDelete)
{
   CListFixed lTempSide;
   lTempSide.Init (sizeof(DWORD));
   plSideNew->Init (sizeof(DWORD));

   // scratch list
   CListFixed lScratch;

   // store away the original points in the edges
   CListFixed lOrigVert;
   lScratch.Init (sizeof(DWORD), plEdge->Get(0), plEdge->Num()*2);
   SortListAndRemoveDup (1, &lScratch, &lOrigVert);

   // remember how many verts for auto accept
   DWORD dwAuto = m_lVert.Num();

   // create a queue list from the list of edges, and then clear lEdge
   CListFixed lQueue;
   lQueue.Init (sizeof(DWORD)*2);
   SortListAndRemoveDup (2, plEdge, &lQueue);
   plEdge->Clear();
   plEdge->Init (sizeof(DWORD)*2);

   // loop over all the vertices
   DWORD i, j, k, dwRet;
   DWORD adwEdge[2];
   DWORD *padwVert = (DWORD*) plVert->Get(0);
   PCPMVert pv;
   for (i = 0; i < plVert->Num(); i++) {
      adwEdge[0] = padwVert[i];
      adwEdge[1] = -1;
      pv = VertGet (padwVert[i]);

      // get list of all the sides
      DWORD *padwSide;
      lScratch.Init (sizeof(DWORD), pv->SideGet(), pv->m_wNumSide);
      padwSide = (DWORD*) lScratch.Get(0);
      for (j = 0; j < lScratch.Num(); j++) {
         dwRet = SideCutBevel (padwSide[j], adwEdge, fRadius, plEdge, &lQueue, -1);
         if (dwRet != -1)
            lTempSide.Add (&dwRet);
      } // j
   }

   // loop while there's a queue
   while (lQueue.Num()) {
      // get the last element of the queue
      DWORD dwGet = lQueue.Num()-1;
      DWORD *padwLast = (DWORD*) lQueue.Get(dwGet);
      adwEdge[0] = padwLast[0];
      adwEdge[1] = padwLast[1];
      lQueue.Remove (dwGet);

      // maintain a list of all the sides associated with vertex 1, vertex2
      // or which contain the entire edge (lScratch)
      lScratch.Init (sizeof(DWORD));
      for (i = 0; i < 2; i++) {
         PCPMVert pv = VertGet(adwEdge[i]);
         DWORD *padwSide = pv->SideGet();

         for (j = 0; j < pv->m_wNumSide; j++) {
            PCPMSide ps = SideGet (padwSide[j]);
            BOOL fFlip;

            // see if the edge occurs on this side
            if (-1 != FindEdgeIndex (ps->m_wNumVert, ps->VertGet(), adwEdge, &fFlip)) {
               // add this to the list of sides...
               DWORD *padw = (DWORD*) lScratch.Get(0);
               for (k = 0; k < lScratch.Num(); k++)
                  if (padw[k] == padwSide[j])
                     break;
               if (k >= lScratch.Num())
                  lScratch.Add (&padwSide[j]);
               continue;
            }

            // else, no edge here
         } // j
      } // i


      // go through the sides common to the edge and split them
      DWORD *padwSide;
      padwSide = (DWORD*) lScratch.Get(0);
      for (i = 0; i < lScratch.Num(); i++) {
         dwRet = SideCutBevel (padwSide[i], adwEdge, fRadius, plEdge, &lQueue, -1);
         if (dwRet != -1)
            lTempSide.Add (&dwRet);
      } // i


      // add this to the list of edges modified and sort
      plEdge->Add (adwEdge);
      qsort (plEdge->Get(0), plEdge->Num(), sizeof(DWORD)*2, B2DWORDCompare);

   } // while queue

   // rebuild the list of sides that have been added or marked as part of bevel
   lScratch.Init (sizeof(DWORD), lTempSide.Get(0), lTempSide.Num());
   SortListAndRemoveDup (1, &lScratch, &lTempSide);

   // go through all the points in the edges
   CListFixed lVert;
   lScratch.Init (sizeof(DWORD), plEdge->Get(0), plEdge->Num()*2);
   SortListAndRemoveDup (1, &lScratch, &lVert);

   // transfer the used edges over, but not the ones in the queue because they are a result
   // of extra bits on the end, and don't require deleting
   plVertToDelete->Init (sizeof(DWORD), lVert.Get(0), lVert.Num());

   padwVert = (DWORD*) lVert.Get(0);
   for (i = 0; i < lVert.Num(); i++) {
      PCPMVert pv = VertGet (padwVert[i]);
      if (!pv)
         continue;
      adwEdge[0] = padwVert[i];
      adwEdge[1] = -1;

      // loop through all the sides
      lScratch.Init (sizeof(DWORD), pv->SideGet(), pv->m_wNumSide);
      DWORD *padwSide = (DWORD*) lScratch.Get(0);
      for (j = 0; j < lScratch.Num(); j++) {
         // if the side is already on the list of edge-parts then ignore
         if (-1 != DWORDSearch (padwSide[j], lTempSide.Num(), (DWORD*)lTempSide.Get(0)))
            continue;

         // else, this vertex is located on the list of edges, but hasn't been split.
         // therefore, split it.
         dwRet = SideCutBevel (padwSide[j], adwEdge, fRadius, plEdge, &lQueue, dwAuto, TRUE);
         if ((dwRet != -1) && (-1 == DWORDSearch(dwRet, lTempSide.Num(), (DWORD*)lTempSide.Get(0)))) {
            lTempSide.Add (&dwRet);
            qsort (lTempSide.Get(0), lTempSide.Num(), sizeof(DWORD), BDWORDCompare);
         }
      } // j
   } //i

   // go through all the original verticies in the edges and make sure they're properly
   // attached to  real sides
   padwVert = (DWORD*) lOrigVert.Get(0);
   CListFixed lTemp;
   for (i = 0; i < lOrigVert.Num(); i++) {
      // go through each of the sides...
      PCPMVert pv = VertGet(padwVert[i]);
      if (!pv)
         continue;

      // need to cache sides away so if add new sides doesn't cause problems
      lTemp.Init (sizeof(DWORD), pv->SideGet(), pv->m_wNumSide);
      DWORD *padwSide = (DWORD*) lTemp.Get(0);

      // extra hack... to solve problem where corner with three bevels impinging
      // isn't divided right... if all the sides this is associated with also are linked
      // to an edge then don't bother.
      // Put this whole section in in the first place so solve two corners coming together
      for (j = 0; j < lTemp.Num(); j++) {
         DWORD dwSide = padwSide[j];
         PCPMSide ps = SideGet (dwSide);
         DWORD dwIndex = ps->VertFind (padwVert[i]);
         if (dwIndex == -1)
            continue;
         DWORD *padw = ps->VertGet();

         // look to right
         DWORD adwEdge[2];
         adwEdge[0] = min(padw[(dwIndex+1)%ps->m_wNumVert], padwVert[i]);
         adwEdge[1] = max(padw[(dwIndex+1)%ps->m_wNumVert], padwVert[i]);
         if (-1 == DWORD2Search (adwEdge, plEdge->Num(), (DWORD*)plEdge->Get(0)))
            break;

         // look left
         adwEdge[0] = min(padw[(dwIndex+ps->m_wNumVert-1)%ps->m_wNumVert], padwVert[i]);
         adwEdge[1] = max(padw[(dwIndex+ps->m_wNumVert-1)%ps->m_wNumVert], padwVert[i]);
         if (-1 == DWORD2Search (adwEdge, plEdge->Num(), (DWORD*)plEdge->Get(0)))
            break;
      }
      if (j >= lTemp.Num())
         continue;   // do nothing

      // else, see if should divide
      for (j = 0; j < lTemp.Num(); j++) {
         // find where this vertex occurs on this side
         DWORD dwSide = padwSide[j];
         PCPMSide ps = SideGet (dwSide);
         DWORD dwIndex = ps->VertFind (padwVert[i]);
         if (dwIndex == -1)
            continue;
         DWORD *padw = ps->VertGet();

         // look left/right until find a point that is attached to a preexisting side
         BOOL fLeft = FALSE;
         for (k = 1; k <= (DWORD)ps->m_wNumVert/2; k++) {
            if (BEVertIsAttachedToOrigSide(padw[(ps->m_wNumVert + dwIndex - k)%ps->m_wNumVert], this, &lTempSide)) {
               fLeft = TRUE;
               break;
            }
            if (BEVertIsAttachedToOrigSide(padw[(dwIndex + k)%ps->m_wNumVert], this, &lTempSide)) {
               fLeft = FALSE;
               break;
            }
         } // k
         if (k == 1)
            continue;   // good situation, this is what want
         if (k > (DWORD)ps->m_wNumVert/2)
            continue;   // cant do anything

         // figure out the vertex number
         DWORD dwAt;
         dwAt = ps->m_wNumVert + dwIndex;
         if (fLeft)
            dwAt -= k;
         else
            dwAt += k;
         dwAt = dwAt % ps->m_wNumVert;

         // because of loops and tests, no that there is at least one side between
         // dwIndex and dwAt to either direction.

         // split the polygon in two so that will get a dividing line between the bevelling
         lScratch.Init (sizeof(DWORD));
         DWORD dw;
         DWORD dwNewSide = m_lSide.Num();
         for (k = dwIndex; ; k++) {
            dw = k % ps->m_wNumVert;
            lScratch.Add (&padw[dw]);

            PCPMVert pv2 = VertGet (padw[dw]);

            // Make sure to reassin vertices, etc.
            if ((k == dwIndex) || (k == dwAt)) {
               // need to add new side
               pv2->SideAdd (&dwNewSide);

               PTEXTUREPOINT ptp1, ptp2;
               ptp1 = pv2->TextureGet (-1);
               ptp2 = pv2->TextureGet (dwSide);
               if (ptp1 != ptp2) {
                  TEXTUREPOINT tp;
                  tp = *ptp2;
                  pv2->TextureSet (dwNewSide, &tp, TRUE);
               }
            }
            else {
               pv2->SideRename (1, &dwSide, &dwNewSide);
               pv2->SideTextRename (1, &dwSide, &dwNewSide, FALSE);
            }

            if (dw == dwAt)
               break;
         } // k

         // create new poly
         PCPMSide pNew = ps->Clone();
         if (!pNew)
            continue;   // error
         pNew->VertClear();
         pNew->VertAdd ((DWORD*) lScratch.Get(0), lScratch.Num());
         m_lSide.Add (&pNew);

         // note that have added
         lTempSide.Add (&dwNewSide);
         qsort (lTempSide.Get(0), lTempSide.Num(), sizeof(DWORD), BDWORDCompare);

         // fewer points in this one
         lScratch.Clear();
         for (k = dwAt; ; k++) {
            dw = k % ps->m_wNumVert;
            lScratch.Add (&padw[dw]);
            if (dw == dwIndex)
               break;
         }
         ps->VertClear();
         ps->VertAdd ((DWORD*) lScratch.Get(0), lScratch.Num());
      } // j
   } // i

   // if have vertices then go through and add to plEdge all the edges surrounding the poitns
   // that beveled from so they'll be deleted
   if (plVert->Num()) {
      lTemp.Init (sizeof(DWORD)*2, plEdge->Get(0), plEdge->Num());

      padwVert = (DWORD*)plVert->Get(0);
      for (i = 0; i < plVert->Num(); i++) {
         PCPMVert pv = VertGet(padwVert[i]);
         DWORD *padwSide = pv->SideGet();
         for (j = 0; j < pv->m_wNumSide; j++) {
            PCPMSide ps = SideGet (padwSide[j]);
            DWORD dwIndex = ps->VertFind (padwVert[i]);
            if (dwIndex == -1)
               continue;
            DWORD *padw = ps->VertGet();

            // left and right
            DWORD adwEdge[2];
            adwEdge[0] = min(padw[(dwIndex+ps->m_wNumVert-1)%ps->m_wNumVert], padwVert[i]);
            adwEdge[1] = max(padw[(dwIndex+ps->m_wNumVert-1)%ps->m_wNumVert], padwVert[i]);
            lTemp.Add (adwEdge);

            adwEdge[0] = min(padw[(dwIndex+1)%ps->m_wNumVert], padwVert[i]);
            adwEdge[1] = max(padw[(dwIndex+1)%ps->m_wNumVert], padwVert[i]);
            lTemp.Add (adwEdge);
         } // j
      } // i

      SortListAndRemoveDup (2, &lTemp, plEdge);
   }
   // transfer over the sides
   SortListAndRemoveDup (1, &lTempSide, plSideNew);

   return TRUE;
}


/*****************************************************************************************
CPolyMesh::GroupBevel - Grevels a group of objects. This takes mirroing into account.

inputs
   fp                fRadius - Radius of the bevel
   DWORD             dwType - 0 for point, 1 for edge, 2 for side
   DWORD             dwNum - Number of elements
   DWORD             *padw - Pointer to elements
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::GroupBevel (fp fRadius, DWORD dwType, DWORD dwNum, DWORD *padw)
{
   if (fRadius < EPSILON)
      return TRUE;

   // make sure symmetry calculted
   SymmetryRecalc();

   // fill in a list of points and edges
   CListFixed lPoint, lEdge;
   DWORD i;
   lPoint.Init (sizeof(DWORD));
   lEdge.Init (sizeof(DWORD)*2);
   switch (dwType) {
   case 0: // points
      MirrorVert (padw, dwNum, &lPoint);
      break;
   case 1: // edges
      MirrorEdge (padw, dwNum, &lEdge);
      break;
   case 2:  // sides
      {
         CListFixed lSide, lEdgeTemp;
         lEdgeTemp.Init (sizeof(DWORD)*2);
         MirrorSide (padw, dwNum, &lSide);
         CalcEdges ();
         SideGroupToEdges (lSide.Num(), (DWORD*)lSide.Get(0), &lEdgeTemp);

         // add to lEdge
         PCPMEdge *ppe = (PCPMEdge*) m_lEdge.Get(0);
         DWORD *padwEdge = (DWORD*) lEdgeTemp.Get(0);
         lEdge.Required (lEdge.Num() + lEdgeTemp.Num());
         for (i = 0; i < lEdgeTemp.Num(); i++)
            lEdge.Add (ppe[padwEdge[i]]->m_adwVert);
         qsort (lEdge.Get(0), lEdge.Num(), sizeof(DWORD)*2, B2DWORDCompare);
      }
      break;
   default:
      return FALSE;
   }

   // bevel
   CListFixed lSideNew, lVertToDelete;
   if (!BevelEdges (fRadius, &lEdge, &lPoint, &lSideNew, &lVertToDelete))
      return FALSE;

   // reassocate verts with sides
   VertSideLinkRebuild();

   // delete dead vertices
   VertDeleteDead ();

   // note that edges are invalid
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   // rebuild symmetry
   m_fDirtySymmetry = TRUE;
   SymmetryRecalc();

   EdgeDelete (lEdge.Num(), (DWORD*)lEdge.Get(0), &lVertToDelete);

   // rebuild symmetry
   // dont need to because done by edgge delete
   //m_fDirtySymmetry = TRUE;
   //SymmetryRecalc();

   return TRUE;

}


/*****************************************************************************************
CPolyMesh::VertTextureToScale - This code takes a vertex, looks up the texture to
produce a color, and uses some element from the color to return a scale from 0..1 so
that organic move will be affected by a texture.

inputs
   DWORD             dwVert - Vertex
   DWORD             dwNumSide - Number of sides to limit to. The code tries to find the
                        texture from this side first. Failing that it looks through all
                        sides associated with the vertex.
   DWORD             *padwSide - Array of dwNumSide sides
   PCObjectSocket    pos - Object socket to get the texture info from.
   DWORD             dwColor - 0 => color doesnt affect, 1,2,3 = R,G,B, 4 = (R+G+B)/3
   BOOL              fInvert - If TRUE then flip the color
returns
   fp - Amount to scale, from 0..1
*/
fp CPolyMesh::VertTextureToScale (DWORD dwVert, DWORD dwNumSide, DWORD *padwSide,
                                  PCObjectTemplate pos, DWORD dwColor, BOOL fInvert)
{
   // if nothing then default
   if (!dwColor || !pos)
      return 1;

   // figure out the side...
   DWORD i;
   PCPMVert pv = VertGet(dwVert);
   DWORD *padw = pv->SideGet();
   if (!pv->m_wNumSide)
      return 1;   // no sides
   DWORD dwSide;
   for (i = 0; i < pv->m_wNumSide; i++) {
      if (-1 != DWORDSearch(padw[i], dwNumSide, padwSide))
         break;
   } // i
   dwSide = padw[(i >= pv->m_wNumSide) ? 0 : i];

   // get the texture...
   PCPMSide ps = SideGet(dwSide);
   PCObjectSurface pss = pos->SurfaceGet (ps->m_dwSurfaceText);
   if (!pss)
      return 1;
   if (!pss->m_fUseTextureMap) {
      delete pss;
      return 1;
   }

   // find the texture
   RENDERSURFACE rs;
   if (pss->m_szScheme[0]) {
      PCSurfaceSchemeSocket psss = pos->m_pWorld->SurfaceSchemeGet();;
      if (psss) {
         PCObjectSurface ps2 = psss->SurfaceGet(pss->m_szScheme, NULL, FALSE);
         if (ps2) {
            delete pss;
            pss = ps2;
         }
      }
   }
   memset (&rs, 0, sizeof(rs));
   memcpy (&rs.afTextureMatrix, &pss->m_afTextureMatrix, sizeof(pss->m_afTextureMatrix));
   memcpy (&rs.abTextureMatrix, &pss->m_mTextureMatrix, sizeof(pss->m_mTextureMatrix));
   rs.fUseTextureMap = pss->m_fUseTextureMap;
   rs.gTextureCode = pss->m_gTextureCode;
   rs.gTextureSub = pss->m_gTextureSub;
   memcpy (&rs.Material, &pss->m_Material, sizeof(pss->m_Material));
   wcscpy (rs.szScheme, pss->m_szScheme);
   rs.TextureMods = pss->m_TextureMods;
   delete pss;

   OSINFO info;
   pos->InfoGet (&info);

   PCTextureMapSocket pText = TextureCacheGet (info.dwRenderShard, &rs, NULL, NULL);
   if (!pText)
      return 1;

   // get the value
   PTEXTUREPOINT ptp = pv->TextureGet(dwSide);
   TEXTPOINT5 tp, tpRange;
   memset (&tp, 0, sizeof(tp));
   memset (&tpRange, 0, sizeof(tpRange));
   tp.hv[0] = rs.afTextureMatrix[0][0] * ptp->h + rs.afTextureMatrix[1][0] * ptp->v;
   tp.hv[1] = rs.afTextureMatrix[0][1] * ptp->h + rs.afTextureMatrix[1][1] * ptp->v;
   tp.xyz[0] = pv->m_pLocSubdivide.p[0];
   tp.xyz[1] = pv->m_pLocSubdivide.p[1];
   tp.xyz[2] = pv->m_pLocSubdivide.p[2];
   //   #define  MMP(i)   pDest->p[i] = p[0][i] * pRight->p[0] + p[1][i] * pRight->p[1] + p[2][i] * pRight->p[2] + p[3][i] * pRight->p[3]


   WORD awColor[3];
   memset (&awColor, 0, sizeof(awColor));
   pText->FillPixel (0, TMFP_ALL, awColor, &tp, &tpRange, &rs.Material, NULL, FALSE);
      // NOTE: Using fHighQuality = FALSE for pixel

   //OSINFO info;
   //pos->InfoGet (&info);

   TextureCacheRelease (info.dwRenderShard, pText);

   // invert?
   if (fInvert) {
      awColor[0] = 0xffff - awColor[0];
      awColor[1] = 0xffff - awColor[1];
      awColor[2] = 0xffff - awColor[2];
   }

   // finally
   switch (dwColor) {
   case 1: // red
      return (fp)awColor[0] / (fp)0xffff;
   case 2: // green
      return (fp)awColor[1] / (fp)0xffff;
   case 3: // blue
      return (fp)awColor[2] / (fp)0xffff;
   default:
   case 4: // intensity
      return ((fp)awColor[0] + (fp)awColor[1] + (fp)awColor[2]) / 3.0 / (fp)0xffff;
   }
}



/*****************************************************************************************
CPolyMesh::OrganicMove - Handles moving the points organically.

inputs
   PCPoint           pCenter - Center of the organic movement, in object space
   fp                fRadius - Radius of the movement, in meters
   fp                fBrushPow - Raise the brush shape to this power, higher values are pointier
   fp                fBrushStrength - From 0 to 1. Exact meaning varies based upon dwType
   DWORD             dwNumSide - Number of sides to limit to. If NULL then allow all sides
   DWORD             *padwSide - Array of dwNumSide sides
   DWORD             dwType - Type of movement:
                        0 - for move out perpendicular to normal, length of pMove used
                        1 - move in perpendicular to normal, length of pMove used
                        2 - move towards viewer
                        3 - move away from viewer
                        4 - smooth
                        5 - roughen
                        6 - pinch
   PCPoint           pMove - Depends upon dwType
   PCObjectSocket    posMask - Use this object socket to get colors from when calculating
                        the mask
   DWORD             dwMaskColor - Color to use for mask. See VertTextureToScale
   BOOL              fMaskInvert - If TRUE then invert the meaning of the mask for scaling
returns
   BOOL - TRUE if success, FALSE if not because morphs going on something
*/
BOOL CPolyMesh::OrganicMove (PCPoint pCenter, fp fRadius, fp fBrushPow, fp fBrushStrength,
                             DWORD dwNumSide, DWORD *padwSide, DWORD dwType, PCPoint pMove,
                             PCObjectTemplate posMask, DWORD dwMaskColor, BOOL fMaskInvert)
{
   if (!CanModify())
      return FALSE;

   // calc symmetry if necessary
   SymmetryRecalc();

   // since the sides may not be sorted or reduced, do so
   CListFixed lTemp, lSides;
   lTemp.Init (sizeof(DWORD), padwSide, dwNumSide);
   SortListAndRemoveDup (1, &lTemp, &lSides);
   dwNumSide = lSides.Num();
   padwSide = (DWORD*) lSides.Get(0);

   // may need edges...
   if ((dwType == 4) || (dwType == 5))
      CalcEdges ();

   // figure out the vertss to look through
   CListFixed lVertLook, lVertIn, lVertStrength, lVertScale;
   lVertLook.Init (sizeof(DWORD));
   lVertIn.Init (sizeof(DWORD));
   lVertStrength.Init (sizeof(fp));
   lVertScale.Init (sizeof(fp));
   if (dwNumSide)
      SidesToVert (padwSide, dwNumSide, &lVertLook);
   DWORD i, dwVert;
   PCPMVert pv;
   DWORD *padw = (DWORD*) lVertLook.Get(0);
   CPoint p;
   fp f;
   for (i = 0; i < (dwNumSide ? lVertLook.Num() : m_lVert.Num()); i++) {
      pv = VertGet(dwVert = (dwNumSide ? padw[i] : i));
      p.Subtract (&pv->m_pLocSubdivide, pCenter);
      if ((fabs(p.p[0]) > fRadius) || (fabs(p.p[1]) > fRadius) ||(fabs(p.p[2]) > fRadius))
         continue;
      f = p.Length();
      if (f >= fRadius)
         continue;

      // compute the strength
      f = cos (f / fRadius * PI/2);
      f = pow (f, fBrushPow);
      f *= fBrushStrength;

      // apply scaling for texture
      f *= VertTextureToScale (dwVert, dwNumSide, padwSide, posMask, dwMaskColor, fMaskInvert);

      // add vertex
      lVertIn.Add (&dwVert);
      lVertStrength.Add (&f);

      // potentially figure out direction of movement
      if ((dwType == 4) || (dwType == 5)) {
         CPoint pAvg;
         DWORD dwCount = 0;
         pAvg.Zero();
         DWORD j;
         DWORD *padwEdge = pv->EdgeGet();
         for (j = 0; j < pv->m_wNumEdge; j++) {
            PCPMEdge pe = EdgeGet (padwEdge[j]);
            DWORD dwOther = (pe->m_adwVert[0] == dwVert) ? pe->m_adwVert[1] : pe->m_adwVert[0];
            PCPMVert pv2 = VertGet(dwOther);
            if (!pv2)
               continue;

            pAvg.Add (&pv2->m_pLocSubdivide);
            dwCount++;
         }
         if (dwCount)
            pAvg.Scale (1.0 / (fp)dwCount);

         // cache this interms of normal
         fp fScale;
         pAvg.Subtract (&pv->m_pLocSubdivide);
         fScale = pAvg.DotProd (&pv->m_pNormSubdivide);
         lVertScale.Add (&fScale);
      }
   } // i

   // make sure the normals have been calculated
   CalcNorm ();

   // loop and modify the points
   fp *pafStrength = (fp*)lVertStrength.Get(0);
   fp *pafScale = (fp*)lVertScale.Get(0);
   padw = (DWORD*)lVertIn.Get(0);
   f = pMove->Length();
   for (i = 0; i < lVertIn.Num(); i++) {
      pv = VertGet(padw[i]);

      p.Zero();

      switch (dwType) {
      case 0:  // move out by normal
         p.Copy (&pv->m_pNormSubdivide);
         p.Scale (f * pafStrength[i]);
         break;

      case 1:  // move in by normal
         p.Copy (&pv->m_pNormSubdivide);
         p.Scale (-f * pafStrength[i]);
         break;

      case 2:  // move towards viewer
      case 3:  // move away from viewer
         // do the same thing for either
         p.Copy (pMove);
         p.p[0] *= pafStrength[i];  // BUGFIX - Did this because scaling seems to scale by iteslef
         p.p[1] *= pafStrength[i];
         p.p[2] *= pafStrength[i];
         //p.Scale (pafStrength[i]);
         break;

      case 4:  // smooth
         p.Copy (&pv->m_pNormSubdivide);
         p.Scale (pafScale[i] * pafStrength[i]);
         break;

      case 5:  // roughen
         p.Copy (&pv->m_pNormSubdivide);
         p.Scale (-pafScale[i] * pafStrength[i]);
         break;

      case 6: // pinch
         p.Subtract (pCenter, &pv->m_pLocSubdivide);
         p.Scale (pafStrength[i] * (1.0 - 1.0 / pMove->p[0]));
         break;

      } // dwType

      if (!GroupVertMove (padw[i], &p, lVertIn.Num(), padw))
         return FALSE;
   } // i

   return TRUE;
}



/*****************************************************************************************
CPolyMesh::MorphPaint - Deals with painting a morph area

inputs
   PCPoint           pCenter - Center of the organic movement, in object space
   fp                fRadius - Radius of the movement, in meters
   fp                fBrushPow - Raise the brush shape to this power, higher values are pointier
   fp                fBrushStrength - From 0 to 1. Exact meaning varies based upon dwType
   DWORD             dwNumSide - Number of sides to limit to. If NULL then allow all sides
   DWORD             *padwSide - Array of dwNumSide sides
   DWORD             dwType - Type of movement:
                        0 - increase strength of active morph, fScale used, 1.0 = no change
                        1 - decrease strength, 1.0 / fScale
                        2,3 - set morph to 0 (erase)
   fp                fScale - Depnds upon dwType
returns
   BOOL - TRUE if success, FALSE if not because morphs going on something
*/
BOOL CPolyMesh::MorphPaint (PCPoint pCenter, fp fRadius, fp fBrushPow, fp fBrushStrength,
                             DWORD dwNumSide, DWORD *padwSide, DWORD dwType, fp fScale)
{
   if (!CanModify())
      return FALSE;

   // calc symmetry if necessary
   SymmetryRecalc();

   // since the sides may not be sorted or reduced, do so
   CListFixed lTemp, lSides;
   lTemp.Init (sizeof(DWORD), padwSide, dwNumSide);
   SortListAndRemoveDup (1, &lTemp, &lSides);
   dwNumSide = lSides.Num();
   padwSide = (DWORD*) lSides.Get(0);

   // figure out the vertss to look through
   CListFixed lVertLook, lVertIn, lVertStrength, lVertScale;
   lVertLook.Init (sizeof(DWORD));
   lVertIn.Init (sizeof(DWORD));
   lVertStrength.Init (sizeof(fp));
   lVertScale.Init (sizeof(fp));
   if (dwNumSide)
      SidesToVert (padwSide, dwNumSide, &lVertLook);
   DWORD i, dwVert;
   PCPMVert pv;
   DWORD *padw = (DWORD*) lVertLook.Get(0);
   CPoint p;
   fp f;
   for (i = 0; i < (dwNumSide ? lVertLook.Num() : m_lVert.Num()); i++) {
      pv = VertGet(dwVert = (dwNumSide ? padw[i] : i));
      p.Subtract (&pv->m_pLocSubdivide, pCenter);
      if ((fabs(p.p[0]) > fRadius) || (fabs(p.p[1]) > fRadius) ||(fabs(p.p[2]) > fRadius))
         continue;
      f = p.Length();
      if (f >= fRadius)
         continue;

      // compute the strength
      f = cos (f / fRadius * PI/2);
      f = pow (f, fBrushPow);
      f *= fBrushStrength;

      // add vertex
      lVertIn.Add (&dwVert);
      lVertStrength.Add (&f);
   } // i

   // figure out the morph to edit
   if (m_lMorphRemapID.Num() != 1)
      return FALSE;
   DWORD dwMorph = *((DWORD*)m_lMorphRemapID.Get(0));

   // loop and modify the points
   fp *pafStrength = (fp*)lVertStrength.Get(0);
   fp *pafScale = (fp*)lVertScale.Get(0);
   padw = (DWORD*)lVertIn.Get(0);
   DWORD j, k;
   for (i = 0; i < lVertIn.Num(); i++) {
      pv = VertGet(padw[i]);

      // if any of its mirrors exist on the list and they have a lower number
      // than the current one then skip. So if paint across mirror will only
      // affect equally
      PMMIRROR *ppm = pv->MirrorGet();
      for (j = 0; j < pv->m_wNumMirror; j++)
         if ((ppm[j].dwObject < i) && (-1 != DWORDSearch (ppm[j].dwObject, lVertIn.Num(), padw)))
            break;
      if (j < pv->m_wNumMirror)
         continue;

      // loop through this and all its mirrors...
      for (j = 0; j <= pv->m_wNumMirror; j++) {
         PCPMVert pvm = (j < pv->m_wNumMirror) ? VertGet(ppm[j].dwObject) : pv;

         // see if can find the current morph
         PVERTDEFORM pvd = pvm->VertDeformGet();
         for (k = 0; k < pvm->m_wNumVertDeform; k++)
            if (pvd[k].dwDeform == dwMorph)
               break;
         if (k >= pvm->m_wNumVertDeform)
            continue;   // deform not found

         // adjust by the deform
         CPoint pMove;
         pMove.Copy (&pvd[k].pDeform);

         switch (dwType) {
         case 0:  // increase by fScale
            pMove.Scale ((fScale - 1.0) * pafStrength[i]);
            break;

         case 1:  // decrease by fScale
            pMove.Scale (((1.0 / fScale) - 1.0) * pafStrength[i]);
            break;

         case 2: // totally wipe out
         case 3:  // totall wipe out
            pMove.Scale (-1);
            break;
         }

         pvm->DeformChanged (dwMorph, &pMove);
      } // j
   } // i

   return TRUE;
}


/*****************************************************************************************
CPolyMesh::OrganicScale - Scales the entire object.

inputs
   PCPoint           pScale - Amount to scale by in xyz.
   PCObjectSocket    posMask - Use this object socket to get colors from when calculating
                        the mask
   DWORD             dwMaskColor - Color to use for mask. See VertTextureToScale
   BOOL              fMaskInvert - If TRUE then invert the meaning of the mask for scaling
returns
   BOOL - TRUE if success, FALSE if not because morphs going on something
*/
BOOL CPolyMesh::OrganicScale (PCPoint pScale,
                             PCObjectTemplate posMask, DWORD dwMaskColor, BOOL fMaskInvert)
{
   // calc symmetry if necessary
   SymmetryRecalc();

   // figure out scaling values if masked
   CPoint pMask;
   pMask.p[0] = (pScale->p[0] < 0) ? -1 : 1;
   pMask.p[1] = (pScale->p[1] < 0) ? -1 : 1;
   pMask.p[2] = (pScale->p[2] < 0) ? -1 : 1;

   // calculate the matrix for a def scale
   // BUGFIX - Dont pass in transpose inverse because not working, and dont
   // really need for scale
   CMatrix mScaleDef; //, mScaleDefInv;
   mScaleDef.Scale (pScale->p[0], pScale->p[1], pScale->p[2]);
   //mScaleDef.Invert (&mScaleDefInv);
   //mScaleDefInv.Transpose();

   // do scaling
   DWORD i;
   PCPMVert *ppv = (PCPMVert*)m_lVert.Get(0);
   fp f;
   for (i = 0; i < m_lVert.Num(); i++) {
      f = VertTextureToScale (i, 0, NULL, posMask, dwMaskColor, fMaskInvert);

      if (f == 1) {
         ppv[i]->Scale (&mScaleDef, &mScaleDef); //&mScaleDefInv);
         continue;
      }

      // else, new matrix
      CPoint pUse;
      pUse.Average (pScale, &pMask, f);
      CMatrix m;//, mInv;
      m.Scale (pUse.p[0], pUse.p[1], pUse.p[2]);
      //m.Invert (&mInv);
      //mInv.Transpose ();
      ppv[i]->Scale (&m, &m);//&mInv);
   } // i


   // flip vector directions
   f = pMask.p[0] * pMask.p[1] * pMask.p[2];
   PCPMSide *pps = (PCPMSide*)m_lSide.Get(0);
   if (f < 0) for (i = 0; i < m_lSide.Num(); i++)
      pps[i]->Reverse ();

   // dirty
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyEdge = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   return TRUE;
}



/*************************************************************************************
NumMorphIDs - Returns the maximum (+1) morph ID that's found. (Ignores combo-morphs)

inputs
   PCListFixed       plPCOEAttrib - Attrib
reutrns
   DWORD - Number of moph IDs, (maximum+1)
*/
DWORD NumMorphIDs (PCListFixed plPCOEAttrib)
{
   DWORD dwMax, i, j;
   dwMax = -1;
   PCOEAttrib *pap;
   pap = (PCOEAttrib*) plPCOEAttrib->Get(0);
   for (i = 0; i < plPCOEAttrib->Num(); i++) {
      // only allow the default settings
      if (pap[i]->m_dwType != 2)
         continue;

      PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) pap[i]->m_lCOEATTRIBCOMBO.Get(0);
      DWORD dwNum = pap[i]->m_lCOEATTRIBCOMBO.Num();

      for (j = 0; j < dwNum; j++, pc++)
         dwMax = (dwMax == -1) ? pc->dwMorphID : max(dwMax, pc->dwMorphID);
   }
   if (dwMax == -1)
      return 0;  // nothing
   return dwMax + 1;
}


/*****************************************************************************************
CPolyMesh::MergeNoSymmetry - Merges one polygon mesh into this one. The merge does
not create mirrors of merged-in object though.

inputs
   PCPolyMesh        pMerge - Merge this object into the current one
   PCMatrix          pmMergeToThis - Convert from the coords in pm to this coords
   BOOL              fFlip - If TRUE then will need to flip the order of polygons
   PWSTR             pszAppend - Append this on to the attribute names so morphs are merged
                        uniquely
   PCObjectTemplate  pThisTemp - Object template for this object - if expect to merge colors
   PCObjectTemplate  pMergeTemp - Object template for merge-from object, if expect to merge colors
returns
   BOOL - TRUE if exists
*/
BOOL CPolyMesh::MergeNoSymmetry (PCPolyMesh pMerge, PCMatrix pmMergeToThis, BOOL fFlip,
                                 PWSTR pszMorphAppend,
                                 PCObjectTemplate pThisTemp, PCObjectTemplate pMergeTemp)
{
   // merge in bones
   DWORD i, j;
   CListFixed lBoneRemapFrom, lBoneRemapTo;
   lBoneRemapFrom.Init (sizeof(DWORD));
   lBoneRemapTo.Init (sizeof(DWORD));
   if (IsEqualGUID (m_gBone, GUID_NULL))
      m_gBone = pMerge->m_gBone;
   if (pMergeTemp && pThisTemp && IsEqualGUID (m_gBone, pMerge->m_gBone)) {
      PPMBONEINFO pbiMerge = (PPMBONEINFO) pMerge->m_lPMBONEINFO.Get(0);
      for (i = 0; i < pMerge->m_lPMBONEINFO.Num(); i++) {
         // find a match in exsiting
         PPMBONEINFO pbi = (PPMBONEINFO) m_lPMBONEINFO.Get(0);
         for (j = 0; j < m_lPMBONEINFO.Num(); j++) {
            if (!_wcsicmp(pbiMerge[i].szName, pbi[j].szName))
               break;
         } // j

         if (j < m_lPMBONEINFO.Num()) {
            // already exists so easy remap
            lBoneRemapFrom.Add (&i);
            lBoneRemapTo.Add (&j);
         }
         else { // add
            j = m_lPMBONEINFO.Num();
            m_lPMBONEINFO.Add (&pbiMerge[i]);   // should be safe to add entire thing
            lBoneRemapFrom.Add (&i);
            lBoneRemapTo.Add (&j);
         }
      } // i
   }
   DWORD dwBoneRemapFromNum;
   DWORD *padwBoneRemapFrom, *padwBoneRemapTo;
   dwBoneRemapFromNum = lBoneRemapFrom.Num();
   padwBoneRemapFrom = (DWORD*)lBoneRemapFrom.Get(0);
   padwBoneRemapTo = (DWORD*)lBoneRemapTo.Get(0);

   // merge in colors...
   CListFixed lSurfRemapFrom, lSurfRemapTo;
   lSurfRemapFrom.Init (sizeof(DWORD));
   lSurfRemapTo.Init (sizeof(DWORD));
   if (pMergeTemp && pThisTemp) {
      for (i = 0; i < pMergeTemp->ObjectSurfaceNumIndex(); i++) {
         DWORD dwMergeSurf = pMergeTemp->ObjectSurfaceGetIndex(i);
         PCObjectSurface pMergeSurf = pMergeTemp->ObjectSurfaceFind (dwMergeSurf);
         if (!pMergeSurf)
            continue;

         // see if find a match in existing...
         for (j = 0; j < pThisTemp->ObjectSurfaceNumIndex(); j++) {
            DWORD dwThisSurf = pThisTemp->ObjectSurfaceGetIndex(j);
            PCObjectSurface pThisSurf = pThisTemp->ObjectSurfaceFind (dwThisSurf);
            if (!pThisSurf)
               continue;

            if (pMergeSurf->AreTheSame (pThisSurf)) {
               // found a match
               lSurfRemapFrom.Add (&dwMergeSurf);
               lSurfRemapTo.Add (&dwThisSurf);
               break;
            }
         }
         if (j < pThisTemp->ObjectSurfaceNumIndex())
            continue;   // found match so dont do anymore

         // else, need to add
         for (j = 0;; j++)
            if (!pThisTemp->ObjectSurfaceFind(j))
               break;

         // have a blank entry
         PCObjectSurface pNew;
         pNew = pMergeSurf->Clone();
         if (!pNew)
            continue;
         pNew->m_dwID = j;
         pThisTemp->ObjectSurfaceAdd (pNew);
         delete pNew;
         lSurfRemapFrom.Add (&dwMergeSurf);
         lSurfRemapTo.Add (&j);
      } // i
   } // if merge temp
   DWORD dwSurfRemapFromNum;
   DWORD *padwSurfRemapFrom, *padwSurfRemapTo;
   dwSurfRemapFromNum = lSurfRemapFrom.Num();
   padwSurfRemapFrom = (DWORD*)lSurfRemapFrom.Get(0);
   padwSurfRemapTo = (DWORD*)lSurfRemapTo.Get(0);

   // figure out new morphs...
   // figure out the max # of real morphs on side A and B
   DWORD dwMaxA, dwMaxB;
   dwMaxA = NumMorphIDs (&m_lPCOEAttrib);
   dwMaxB = NumMorphIDs (&pMerge->m_lPCOEAttrib);

   // create the remap list
   CListFixed lRemapFrom, lRemapTo;
   lRemapFrom.Init (sizeof(DWORD));
   lRemapTo.Init (sizeof(DWORD));

   // loop through all the morphs in side B and find the remap number
   PCOEAttrib *papB = (PCOEAttrib*) pMerge->m_lPCOEAttrib.Get(0);
   PCOEAttrib *papA = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   PCOEAttrib paFrom;
   DWORD k, dwFind;
   for (i = 0; i < dwMaxB; i++) {
      // find a match
      for (j = 0; j < pMerge->m_lPCOEAttrib.Num(); j++) {
         if ((papB[j]->m_dwType != 2) || !papB[j]->m_lCOEATTRIBCOMBO.Num())
            continue;   // must be of the right type
         PCOEATTRIBCOMBO pc;
         pc = (PCOEATTRIBCOMBO) papB[j]->m_lCOEATTRIBCOMBO.Get(0);
         dwFind = pc->dwMorphID;
         if (dwFind == i)
            break;
      }
      if (j >= pMerge->m_lPCOEAttrib.Num()) {
         // if find this delete it
         lRemapFrom.Add (&i);
         dwFind = -1;
         lRemapTo.Add (&dwFind);
         continue;
      }
      paFrom = papB[j];

      // know where it's from, figre out what it's new name will be
      WCHAR szTemp[64];
      wcscpy (szTemp, paFrom->m_szName);
      if (pszMorphAppend) {
         szTemp[63-wcslen(pszMorphAppend)] = 0; // make sure string doesnt get too long
         wcscat (szTemp, pszMorphAppend);
      }

      // see which one in the original that it matches
      DWORD dwStringMatch;
      BOOL fWasSubMorph;
      dwStringMatch = -1;
      fWasSubMorph = FALSE;
      for (j = 0; j < m_lPCOEAttrib.Num(); j++) {
         if (_wcsicmp(papA[j]->m_szName, szTemp))
            continue;   // strings don't match
         dwStringMatch = j;

         // is it a sub morph, or an combo-morph
         fWasSubMorph = (papA[j]->m_dwType == 2);
         break;
      }

      // if the strings matches BUT it wasn't a sub-morph then need to invent a new
      // name for this and pretend it doesn't match
      if ((dwStringMatch != -1) && !fWasSubMorph) {
         MakeAttribNameUnique (szTemp, &m_lPCOEAttrib);
         dwStringMatch = -1;
      }

      // if the strings match then use that for remap
      if (dwStringMatch != -1) {
         PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO)papA[dwStringMatch]->m_lCOEATTRIBCOMBO.Get(0);
         dwFind = pc->dwMorphID;
         lRemapFrom.Add (&i);
         lRemapTo.Add (&dwFind);
         continue;   // go on
      }

      // else, add a new one to A
      PCOEAttrib pNew;
      pNew = new COEAttrib;
      if (!pNew)
         continue;   // error
      lRemapFrom.Add (&i);
      lRemapTo.Add (&dwMaxA);
      paFrom->CloneTo (pNew);
      wcscpy (pNew->m_szName, szTemp);
      PCOEATTRIBCOMBO pc;
      pc = (PCOEATTRIBCOMBO) pNew->m_lCOEATTRIBCOMBO.Get(0);
      if (pc)
         pc->dwMorphID = dwMaxA;
      dwMaxA++;
      m_lPCOEAttrib.Add (&pNew);
      papA = (PCOEAttrib*) m_lPCOEAttrib.Get(0);  // reload because adding may cause ESCREALLOC
   }
   DWORD *padwRemapFrom, *padwRemapTo;
   padwRemapFrom = (DWORD*) lRemapFrom.Get(0);
   padwRemapTo = (DWORD*) lRemapTo.Get(0);

   // now, go through and transfer over all the unique combo attributes, with
   // a remap
   for (i = 0; i < pMerge->m_lPCOEAttrib.Num(); i++) {
      paFrom = papB[i];
      if (paFrom->m_dwType != 0)
         continue;   // only care about the combo

      WCHAR szTemp[64];
      wcscpy (szTemp, paFrom->m_szName);
      if (pszMorphAppend) {
         szTemp[63-wcslen(pszMorphAppend)] = 0; // make sure string doesnt get too long
         wcscat (szTemp, pszMorphAppend);
      }

      // find a match in A? Basically, tell it to create a unique name for A.
      // if the name changes then it matched
      WCHAR szUnique[64];
      wcscpy (szUnique, szTemp);
      MakeAttribNameUnique (szUnique, &m_lPCOEAttrib);

      // if the name changed then A already has the morph, so don't do anything else
      // to propogate this
      // NOTE: Assuming sub-morphs are the same. This is not necessarily the case.
      if (_wcsicmp(szTemp, szUnique))
         continue;

      // else, go on and create a new one
      PCOEAttrib pNew;
      pNew = new COEAttrib;
      if (!pNew)
         continue;
      paFrom->CloneTo (pNew);

      wcscpy (pNew->m_szName, szUnique);

      // need to remap the sub-morphs this references
      PCOEATTRIBCOMBO pc;
      DWORD dwTo;
      pc = (PCOEATTRIBCOMBO) pNew->m_lCOEATTRIBCOMBO.Get(0);
      for (j = 0; j < pNew->m_lCOEATTRIBCOMBO.Num(); j++, pc++) {
         for (k = 0; k < lRemapFrom.Num(); k++) {
            if (pc->dwMorphID == padwRemapFrom[k])
               break;
         } // k
         if (k < lRemapFrom.Num())
            dwTo = padwRemapTo[k];
         else
            dwTo = -1;
         // NOTE: Should be getting any deletion of morphs here, so if do,
         // just set to 0
         if (dwTo == -1)
            dwTo = 0;
         pc->dwMorphID = dwTo;
      }  // j

      // add the new one
      m_lPCOEAttrib.Add (&pNew);
   } // i - all morphs


   // figure out normal affect
   CMatrix mInv;
   pmMergeToThis->Invert (&mInv);
   mInv.Transpose ();

   // figure out rename scheme
   CListFixed lRenVert, lRenVertTo, lRenSide, lRenSideTo;
   lRenVert.Init (sizeof(DWORD));
   lRenVertTo.Init (sizeof(DWORD));
   lRenSide.Init (sizeof(DWORD));
   lRenSideTo.Init (sizeof(DWORD));
   lRenVert.Required (pMerge->m_lVert.Num());
   lRenVertTo.Required (pMerge->m_lVert.Num());
   for (i = 0; i < pMerge->m_lVert.Num(); i++) {
      j = i + m_lVert.Num();
      lRenVert.Add (&i);
      lRenVertTo.Add (&j);
   }
   lRenSide.Required (pMerge->m_lSide.Num());
   lRenSideTo.Required (pMerge->m_lSide.Num());
   for (i = 0; i < pMerge->m_lSide.Num(); i++) {
      j = i + m_lSide.Num();
      lRenSide.Add (&i);
      lRenSideTo.Add (&j);
   }

   // add in all points
   PCPMVert *ppv = (PCPMVert*) pMerge->m_lVert.Get(0);
   PCPMVert pv;
   m_lVert.Required (pMerge->m_lVert.Num());
   for (i = 0; i < pMerge->m_lVert.Num(); i++) {
      pv = ppv[i]->Clone();
      if (!pv)
         return FALSE;
      pv->SideRename (lRenSide.Num(), (DWORD*)lRenSide.Get(0), (DWORD*)lRenSideTo.Get(0));
      pv->SideTextRename (lRenSide.Num(), (DWORD*)lRenSide.Get(0), (DWORD*)lRenSideTo.Get(0), FALSE);
      pv->Scale (pmMergeToThis, &mInv);
      pv->EdgeClear();
      pv->MirrorClear();

      // rename morph
      pv->VertDeformRename (lRemapFrom.Num(), padwRemapFrom, padwRemapTo);

      // remap bones
      pv->BoneWeightRename (dwBoneRemapFromNum, padwBoneRemapFrom, padwBoneRemapTo);

      // special... if it's close to 0 then make 0, that way can merge in symmetry ok
      for (j = 0; j < 3; j++)
         if (fabs(pv->m_pLoc.p[j]) < CLOSE)
            pv->m_pLoc.p[j] = 0;

      m_lVert.Add (&pv);
   } // i

   // add in all the sides
   PCPMSide *pps = (PCPMSide*) pMerge->m_lSide.Get(0);
   PCPMSide ps;
   m_lSide.Required (pMerge->m_lSide.Num());
   for (i = 0; i < pMerge->m_lSide.Num(); i++) {
      ps = pps[i]->Clone();
      if (!ps)
         return FALSE;
      ps->MirrorClear();
      ps->VertRename (lRenVert.Num(), (DWORD*)lRenVert.Get(0), (DWORD*)lRenVertTo.Get(0));

      if (fFlip)
         ps->Reverse ();

      // convert the surface if
      for (j = 0; j < dwSurfRemapFromNum; j++)
         if (ps->m_dwSurfaceText == padwSurfRemapFrom[j]) {
            ps->m_dwSurfaceText = padwSurfRemapTo[j];
            break;
         }

      m_lSide.Add (&ps);
   } // i

   // all dirty
   m_fDirtyRender = TRUE;
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtySymmetry = TRUE;  // not dirty since nothing here
   m_fDirtyMorphCOM = TRUE;
   return TRUE;
}


/*****************************************************************************************
CPolyMesh::Merge - Merges one polygon mesh into this one. If the current polygon
mesh has symmetry then the merge takes the symmetry into account.

inputs
   PCPolyMesh        pMerge - Merge this object into the current one
   PCMatrix          pmMergeToThis - Convert from the coords in pm to this coords
   PCObjectTemplate  pThisTemp - Object template for this object - if expect to merge colors
   PCObjectTemplate  pMergeTemp - Object template for merge-from object, if expect to merge colors
returns
   BOOL - TRUE if exists
*/
BOOL CPolyMesh::Merge (PCPolyMesh pMerge, PCMatrix pmMergeToThis,
                       PCObjectTemplate pThisTemp, PCObjectTemplate pMergeTemp)
{
   if (!pMerge->m_lVert.Num())
      return TRUE;

   // figure out the bounding box when take all the points from pMerge and translate
   // into the current object
   CPoint apBound[2];
   PCPMVert *ppv = (PCPMVert*) pMerge->m_lVert.Get(0);
   DWORD i;
   CPoint p;
   for (i = 0; i < pMerge->m_lVert.Num(); i++) {
      p.Copy (&ppv[i]->m_pLoc);
      p.p[3] = 1;
      p.MultiplyLeft (pmMergeToThis);

      if (i) {
         apBound[0].Min(&p);
         apBound[1].Max(&p);
      }
      else {
         apBound[0].Copy(&p);
         apBound[1].Copy(&p);
      }
   } // i

   // what symmetry
   DWORD dwSym = m_dwSymmetry;
   if (apBound[0].p[0] * apBound[1].p[0] < 0)
      dwSym &= (~0x01); // crosses over x, so cant use x symmetry
   if (apBound[0].p[1] * apBound[1].p[1] < 0)
      dwSym &= (~0x02); // crosses over x, so cant use x symmetry
   if (apBound[0].p[2] * apBound[1].p[2] < 0)
      dwSym &= (~0x04); // crosses over x, so cant use x symmetry

   // loop
   DWORD x,y,z,xMax,yMax,zMax;
   xMax = (dwSym & 0x01) ? 2 : 1;
   yMax = (dwSym & 0x02) ? 2 : 1;
   zMax = (dwSym & 0x04) ? 2 : 1;
   WCHAR szAppend[32];

   for (x = 0; x < xMax; x++) for (y = 0; y < yMax; y++) for (z = 0; z < zMax; z++) {
      CPoint pScale;
      BOOL fFlip;
      pScale.p[0] = x ? -1 : 1;
      pScale.p[1] = y ? -1 : 1;
      pScale.p[2] = z ? -1 : 1;
      fFlip = (pScale.p[0] * pScale.p[1] * pScale.p[2] < 0);

      // matrix
      CMatrix m;
      m.Scale (pScale.p[0], pScale.p[1], pScale.p[2]);
      m.MultiplyLeft (pmMergeToThis);

      // symmetry string?
      szAppend[0] = 0;
      if ((xMax>1) || (yMax>1) || (zMax>1)) {
         wcscpy (szAppend, L" (");
         if (xMax>1) {
            if ((apBound[0].p[0] + apBound[1].p[0]) * (x ? -1 : 1) >= 0)
               wcscat (szAppend, L"R");
            else
               wcscat (szAppend, L"L");
         }
         if (yMax>1) {
            if ((apBound[0].p[1] + apBound[1].p[1]) * (y ? -1 : 1) >= 0)
               wcscat (szAppend, L"B");
            else
               wcscat (szAppend, L"F");
         }
         if (zMax>1) {
            if ((apBound[0].p[2] + apBound[1].p[2]) * (z ? -1 : 1) >= 0)
               wcscat (szAppend, L"T");
            else
               wcscat (szAppend, L"B");
         }
         wcscat (szAppend, L")");
      }

      // mirror
      if (!MergeNoSymmetry (pMerge, &m, fFlip, szAppend, pThisTemp, pMergeTemp))
         return FALSE;
   } // xyz

   // BUGFIX - Sort final morphs
   qsort (m_lPCOEAttrib.Get(0), m_lPCOEAttrib.Num(), sizeof(PCOEAttrib), OECALCompare);
   CalcMorphRemap(); // redo morph remap since may have changed
   m_fDirtyMorphCOM = TRUE;

   // done
   return TRUE;
}

/*****************************************************************************************
CPolyMesh::MirrorToOtherSide - This mirrors half of the model (defiend by the xy or z
plane) to the other side.

inputs
   DWORD          dwDim - Dimention, x =0, y=1, z=2
   BOOL           fKeepPos - If TRUE keep the positive side, else negative
returns
   BOOL - TRUE if success, FALSE if fail (perhaps because would have deleted all points)
*/
BOOL CPolyMesh::MirrorToOtherSide (DWORD dwDim, BOOL fKeepPos)
{
   // figure out what sides get deleted
   CListFixed lSideDel;
   lSideDel.Init (sizeof(DWORD));
   DWORD i, j, k;
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   PCPMSide ps;
   PCPMVert pv;
   for (i = 0; i < m_lSide.Num(); i++) {
      ps = pps[i];

      // get all the vertices and find min and max
      DWORD *padwVert = ps->VertGet();
      fp fMin, fMax;
      for (j = 0; j < ps->m_wNumVert; j++) {
         pv = ppv[padwVert[j]];
         if (j) {
            fMin = min(fMin, pv->m_pLoc.p[dwDim]);
            fMax = max(fMax, pv->m_pLoc.p[dwDim]);
         }
         else
            fMin = fMax = pv->m_pLoc.p[dwDim];
      } // j

      if (!fKeepPos) {
         fp f = fMin;
         fMin = -fMax;
         fMax = -f;
      }

      // if it's entirely on the wrong side then delete
      if (fMax <= 0) {
         lSideDel.Add (&i);
         continue;
      }

      // if it's entirely on right side then keep
      if (fMin >= 0)
         continue;

      // else, straddles... see if it's symmetrical...
      for (j = 0; j < ps->m_wNumVert; j++) {
         pv = ppv[padwVert[j]];
         if (fKeepPos && (pv->m_pLoc.p[dwDim] >= 0))
            continue;   // already on right side
         else if (!fKeepPos && (pv->m_pLoc.p[dwDim] <= 0))
            continue;   // already on right side

         // see if matches another side
         CPoint pWant;
         pWant.Copy (&pv->m_pLoc);
         pWant.p[dwDim] *= -1;
         for (k = 0; k < ps->m_wNumVert; k++) {
            if (j == k)
               continue;
            if (pWant.AreClose (&ppv[padwVert[k]]->m_pLoc))
               break;
         } // k
         if (k >= ps->m_wNumVert) {
            // couldnt find a symmetrical part. Since not really sure how to
            // mirror this, add to delete list
            lSideDel.Add (&i);
            break;
         }

         // else, to be paranoid need to mirro the new side over to this one,
         // so that keep textures symmetryical
         ppv[padwVert[k]]->CloneTo (pv);
         pv->m_pLoc.p[dwDim] *= -1;
      } // j
      if (j < ps->m_wNumVert)
         continue;   // not symmetrical, so continue

      // if it gets here then it's symmetrical, so dont delte
      continue;
   } // i

   // if deleting all the sides then fail
   if (lSideDel.Num() >= m_lSide.Num())
      return FALSE;

   // delete the sides
   SideDeleteMany (lSideDel.Num(), (DWORD*)lSideDel.Get(0));
   VertSideLinkRebuild ();
   VertDeleteDead();

   // mirror vertex
   CMatrix mMirror;
   mMirror.Scale ((dwDim == 0) ? -1 : 1, (dwDim==1) ? -1 : 1, (dwDim==2) ? -1 : 1);

   // go through all the points and mirror
   CListFixed lOrigVert, lMirrorVert;
   lOrigVert.Init (sizeof(DWORD));
   lMirrorVert.Init (sizeof(DWORD));
   DWORD dwNum = m_lVert.Num();
   DWORD dwOrigVert = dwNum;
   lOrigVert.Required (dwNum);
   lMirrorVert.Required (dwNum);
   for (i = 0; i < dwNum; i++) {
      CPoint pMirror;
      pv = VertGet(i);
      pMirror.Copy (&pv->m_pLoc);
      pMirror.p[dwDim] *= -1;

      lOrigVert.Add (&i);

      // trivial find
      if (pMirror.p[dwDim] == 0) {
         lMirrorVert.Add (&i);
         continue;
      }

      // find the vertex
      DWORD dwFind = VertFind (&pMirror);
      if (dwFind != -1) {
         lMirrorVert.Add (&dwFind);
         continue;
      }

      // else add
      pv = pv->Clone();
      if (!pv) {
         dwFind = -1;
         lMirrorVert.Add (&dwFind);
         continue;
      }
      pv->Scale (&mMirror, &mMirror);  // can use the same matrix for both
      
      dwFind = m_lVert.Num();
      lMirrorVert.Add (&dwFind);

      m_lVert.Add (&pv);
   }

   // loop through all the sides and mirror
   CListFixed lOrigSide, lMirrorSide;
   lOrigSide.Init (sizeof(DWORD));
   lMirrorSide.Init (sizeof(DWORD));
   dwNum = m_lSide.Num();
   for (i = 0; i < dwNum; i++) {
      ps = SideGet(i);

      // get all the vertices and find min and max
      DWORD *padwVert = ps->VertGet();
      fp fMin, fMax;
      for (j = 0; j < ps->m_wNumVert; j++) {
         pv = VertGet(padwVert[j]);
         if (j) {
            fMin = min(fMin, pv->m_pLoc.p[dwDim]);
            fMax = max(fMax, pv->m_pLoc.p[dwDim]);
         }
         else
            fMin = fMax = pv->m_pLoc.p[dwDim];
      } // j
      if (!fKeepPos) {
         fp f = fMin;
         fMin = -fMax;
         fMax = -f;
      }

      // if it's not entirely on one side then don't mirror because was left
      // behind
      if (fMin < 0)
         continue;

      // else, mirror
      ps = ps->Clone();
      if (!ps)
         continue;
      ps->VertRename (lOrigVert.Num(), (DWORD*)lOrigVert.Get(0), (DWORD*)lMirrorVert.Get(0));
      ps->Reverse(); // reverse normals

      DWORD dwFind;
      dwFind = m_lSide.Num();
      lMirrorSide.Add (&dwFind);
      m_lSide.Add (&ps);
      lOrigSide.Add (&i);
   } // i


   // go back over the new vertices and rename the sides
   for (i = dwOrigVert; i < m_lVert.Num(); i++) {
      pv = VertGet (i);

      // dont need to do this: pv->SideRename (lOrigSide.Num(), (DWORD*)lOrigSide.Get(0), (DWORD*)lMirrorSide.Get(0));
      pv->SideTextRename (lOrigSide.Num(), (DWORD*)lOrigSide.Get(0), (DWORD*)lMirrorSide.Get(0), FALSE);
   }

   // go over verts that aren't mirrored and which are on 0, and make sure that if connected
   // to one side, are also connected to other mirrored side
   DWORD *padwOrigSide, *padwMirrorSide;
   CListFixed lRemap;
   lRemap.Init (sizeof(SIDETEXT));
   padwOrigSide = (DWORD*)lOrigSide.Get(0);
   padwMirrorSide = (DWORD*)lMirrorSide.Get(0);
   for (i = 0; i < dwOrigVert; i++) {
      pv = VertGet(i);
      if (pv->m_pLoc.p[dwDim] != 0)
         continue;

      lRemap.Clear();
      SIDETEXT st;
      PSIDETEXT pst = pv->SideTextGet ();
      DWORD dwFind;
      for (j = 0; j < pv->m_wNumSideText; j++) {
         dwFind = DWORDSearch (pst[j].dwSide, lOrigSide.Num(), padwOrigSide);
         if (dwFind == -1)
            continue;

         // else match
         st = pst[j];
         st.dwSide = padwMirrorSide[dwFind];
         lRemap.Add (&st);
      } // j

      pst = (PSIDETEXT) lRemap.Get(0);
      for (j = 0; j < lRemap.Num(); j++)
         pv->TextureSet (pst[j].dwSide, &pst[j].tp, TRUE);
   }

   // rebuild side links
   VertSideLinkRebuild ();
   VertDeleteDead();

   // finally
   m_fDirtyRender = TRUE;
   m_fDirtyEdge = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtySymmetry = TRUE;  // not dirty since nothing here
   m_fDirtyMorphCOM = TRUE;
   return TRUE;
}

/*************************************************************************************
CPolyMesh::CalcMorphCOM - Calculates the center of mass for all the morphs. The
center of mass is used by attribute info.
*/
void CPolyMesh::CalcMorphCOM (void)
{
   if (!m_fDirtyMorphCOM)
      return;
   m_fDirtyMorphCOM = FALSE;
   m_lMorphCOM.Clear();

   // FUTURERELEASE - Morph COM may need to be moved base don the bone locations

   DWORD dwMax, i, j;
   dwMax = NumMorphIDs (&m_lPCOEAttrib);
   if (dwMax == 0)
      return;  // nothing

   CPoint p;
   p.Zero4();
   m_lMorphCOM.Required (dwMax);
   for (i = 0; i < dwMax; i++)
      m_lMorphCOM.Add (&p);
   PCPoint pCOM = (PCPoint) m_lMorphCOM.Get(0);

   // loop through all the vertices
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < m_lVert.Num(); i++) {
      PCPMVert pv = ppv[i];
      PVERTDEFORM pvd = pv->VertDeformGet();
      for (j = 0; j < pv->m_wNumVertDeform; j++) {
         pCOM[pvd[j].dwDeform].Add (&pv->m_pLoc);
         pCOM[pvd[j].dwDeform].p[3] += 1;
      } // j
   } // i

   // loop through all the COM and average
   for (i = 0; i < m_lMorphCOM.Num(); i++)
      if (pCOM[i].p[3])
         pCOM[i].Scale (1.0 / pCOM[i].p[3]);
}

/*************************************************************************************
CPolyMesh::CalcMorphRemap - Called when the morph parameters have changed.
This recalculates the values of all the morphs. It fills in m_lMorphRemapXXX
*/
void CPolyMesh::CalcMorphRemap (void)
{
   // set dirty flag
   m_fDirtyRender = TRUE;

   // clear old
   m_lMorphRemapID.Clear();
   m_lMorphRemapValue.Clear();

   // find number of morph IDs
   DWORD dwMax, i, j;
   dwMax = NumMorphIDs (&m_lPCOEAttrib);
   if (dwMax == 0)
      return;  // nothing

   // allocate enoug space
   CMem memMorph;
   if (!memMorph.Required ((dwMax + m_lPCOEAttrib.Num()) * sizeof(fp)))
      return;  // cant go furhter
   fp *pfMorph, *pfBoneMorph;
   pfMorph = (fp*) memMorph.p;
   pfBoneMorph = (fp*) pfMorph + dwMax;
restart:
   // do a memset on the memory - faster then looping through all floats and setting to 0
   memset (memMorph.p, 0, memMorph.m_dwAllocated);
   
   // look for bone morphs...
   PPMBONEINFO pbi = (PPMBONEINFO) m_lPMBONEINFO.Get(0);
   for (i = 0; i < m_lPMBONEINFO.Num(); i++, pbi++) {
      for (j = 0; j < NUMBONEMORPH; j++) {
         PPMBONEMORPH pbm = &pbi->aBoneMorph[j];
         if (!pbm->szName[0])
            continue;

         // figure out value...
         fp fValue;
         if (pbi->pBoneLastRender.p[pbm->dwDim] <= pbm->tpBoneAngle.h)
            fValue = pbm->tpMorphValue.h;
         else if (pbi->pBoneLastRender.p[pbm->dwDim] >= pbm->tpBoneAngle.v)
            fValue = pbm->tpMorphValue.v;
         else
            fValue = (pbi->pBoneLastRender.p[pbm->dwDim] - pbm->tpBoneAngle.h) /
               (pbm->tpBoneAngle.v - pbm->tpBoneAngle.h) *
               (pbm->tpMorphValue.v - pbm->tpMorphValue.h) + pbm->tpMorphValue.h;
         if (!fValue)
            continue;   // wouldnt make any difference

         // find the morph
         PCOEAttrib *pp;
         COEAttrib aFind, *paFind;
         PCOEAttrib *pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
         wcscpy (aFind.m_szName, pbm->szName);
         paFind = &aFind;
         pp = (PCOEAttrib*) bsearch (&paFind, pap, m_lPCOEAttrib.Num(), sizeof (PCOEAttrib), OECALCompare);
         if (!pp)
            continue;   // cant find

         DWORD dwIndex = (DWORD)((PBYTE)pp - (PBYTE)pap) / sizeof(PCOEAttrib);
         pfBoneMorph[dwIndex] += fValue;
      } // j
   } // i

   // loop over attributes and set morph accordingly
   PCOEAttrib *pap;
   pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   for (i = 0; i < m_lPCOEAttrib.Num(); i++) {
      PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) pap[i]->m_lCOEATTRIBCOMBO.Get(0);
      DWORD dwNum = pap[i]->m_lCOEATTRIBCOMBO.Num();

      for (j = 0; j < dwNum; j++, pc++) {
         if (pc->dwMorphID >= dwMax) {
            // out of range, so clear out. Shouldnt happen that ofen
            pap[i]->m_lCOEATTRIBCOMBO.Remove (j);
            goto restart;
         }

         // figure out the ramp...
         fp fIn, fVal, fDelta;
         DWORD k;
         fIn = pap[i]->m_fCurValue + pfBoneMorph[i];
            // BUGFIX - Include bone morph
         for (k = 0; k < pc->dwNumRemap; k++)
            if (fIn < pc->afpComboValue[k])
               break;
         if (k == 0) {
            // before the start
            if ((pc->dwCapEnds & 0x01) || (pc->afpComboValue[0] == pc->afpComboValue[1]))
               fVal = pc->afpObjectValue[0];
            else
               k++;   // so that uses that trend line
         }
         else if (k == pc->dwNumRemap) {
            if ((pc->dwCapEnds & 0x02) || (pc->afpComboValue[pc->dwNumRemap-1] == pc->afpComboValue[pc->dwNumRemap-2]))
               fVal = pc->afpObjectValue[pc->dwNumRemap-1];
            else
               k--;   // so that uses that trend line
         }
         if ((k > 0) && (k < pc->dwNumRemap)) {
            k--;
            fDelta = pc->afpComboValue[k+1] - pc->afpComboValue[k];
            if (fDelta == 0)
               fDelta = 1; // so no divide by zero
            fVal = (fIn - pc->afpComboValue[k]) / fDelta *
               (pc->afpObjectValue[k+1] - pc->afpObjectValue[k]) +
               pc->afpObjectValue[k];
         }

         // include the value in
         pfMorph[pc->dwMorphID] += fVal;

      } // j
   } // i

   // only take non-zero values
   for (i = 0; i < dwMax; i++)
      if (fabs(pfMorph[i]) >= CLOSE) {
         m_lMorphRemapValue.Add (&pfMorph[i]);
         m_lMorphRemapID.Add (&i);
      }

}


/*****************************************************************************************
CPolyMesh::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
BOOL CPolyMesh::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   // look through the attributes
   PCOEAttrib *pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   DWORD dwNum = m_lPCOEAttrib.Num();
   if (!dwNum)
      return FALSE;

   COEAttrib aFind, *paFind;
   PCOEAttrib *pp;
   wcscpy (aFind.m_szName, pszName);
   paFind = &aFind;
   pp = (PCOEAttrib*) bsearch (&paFind, pap, dwNum, sizeof (PCOEAttrib), OECALCompare);
   if (!pp)
      return FALSE;

   // else got it
   *pfValue = pp[0]->m_fCurValue;
   return TRUE;
}


/*****************************************************************************************
CPolyMesh::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CPolyMesh::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   PCOEAttrib *pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   DWORD dwNum = m_lPCOEAttrib.Num();
   DWORD i;
   if (!dwNum)
      return;

   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));
   plATTRIBVAL->Required (plATTRIBVAL->Num() + dwNum);
   for (i = 0; i < dwNum; i++) {
      wcscpy (av.szName, pap[i]->m_szName);
      av.fValue = pap[i]->m_fCurValue;
      plATTRIBVAL->Add (&av);
   }
}


/*****************************************************************************************
CPolyMesh::AttribSetGroupIntern - OVERRIDE THIS

inputs
   PCWorldSocket        pWorld - World to notify of attrib change
   PCObjectSocket       pos - Object to notify of attribute change
Like AttribSetGroup() except passing on non-template attributes.
*/
void CPolyMesh::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib,
                                      PCWorldSocket pWorld, PCObjectSocket pos)
{
   BOOL fSentChanged = FALSE;

   DWORD i;
   COEAttrib aFind, *paFind;
   PCOEAttrib *pp;
   paFind = &aFind;
   PCOEAttrib *pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   DWORD dwNumAttrib = m_lPCOEAttrib.Num();
   for (i = 0; i < dwNum; i++) {
      wcscpy (aFind.m_szName, paAttrib[i].szName);
      pp = (PCOEAttrib*) bsearch (&paFind, pap, dwNumAttrib, sizeof (PCOEAttrib), OECALCompare);
      if (!pp)
         continue;   // not known

      if (pp[0]->m_fCurValue == paAttrib[i].fValue)
         continue;   // no change

      // else, attribute
      if (!fSentChanged) {
         fSentChanged = TRUE;
         pWorld->ObjectAboutToChange (pos);
      }
      pp[0]->m_fCurValue = paAttrib[i].fValue;
   }

   if (fSentChanged) {
      m_fDirtyRender = TRUE;
      CalcMorphRemap();
      pWorld->ObjectChanged (pos);
   }
}


/*****************************************************************************************
CPolyMesh::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CPolyMesh::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   // look through the attributes
   PCOEAttrib *pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   DWORD dwNum = m_lPCOEAttrib.Num();
   if (!dwNum)
      return FALSE;

   COEAttrib aFind, *paFind;
   PCOEAttrib *pp;
   PCOEAttrib p;
   wcscpy (aFind.m_szName, pszName);
   paFind = &aFind;
   pp = (PCOEAttrib*) bsearch (&paFind, pap, dwNum, sizeof (PCOEAttrib), OECALCompare);
   if (!pp)
      return FALSE;
   p = pp[0];

   memset (pInfo, 0, sizeof(*pInfo));
   pInfo->dwType = p->m_dwInfoType;
   pInfo->fDefLowRank = p->m_fDefLowRank;
   pInfo->fDefPassUp = p->m_fDefPassUp;
   pInfo->fMax = p->m_fMax;
   pInfo->fMin = p->m_fMin;
   pInfo->pszDescription = ((p->m_memInfo.p) && ((PWSTR)p->m_memInfo.p)[0]) ?
      (PWSTR)p->m_memInfo.p : NULL;

   // do the COM
   CalcMorphCOM ();
   // already know that pLoc is all 0's because of memset
   PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) p->m_lCOEATTRIBCOMBO.Get(0);
   dwNum = p->m_lCOEATTRIBCOMBO.Num();
   DWORD dwMax = m_lMorphCOM.Num();
   PCPoint pCOM = (PCPoint)m_lMorphCOM.Get(0);
   DWORD j;
   for (j = 0; j < dwNum; j++, pc++) {
      if (pc->dwMorphID >= dwMax)
         continue;   // out of range

      pInfo->pLoc.Add (&pCOM[pc->dwMorphID]);
      pInfo->pLoc.p[3] += 1;
   } // j
   if (pInfo->pLoc.p[3])
      pInfo->pLoc.Scale (1.0 / pInfo->pLoc.p[3]);
   pInfo->pLoc.p[3] = 1;
   return TRUE;
}


typedef struct {
   PCPolyMesh     pm;
   PCWorldSocket  pWorld;
   PCObjectSocket pObject;
   WCHAR          szAttrib[64];       // filled in by ObjEditorAttribSel with the attribute selected
   DWORD          dwEdit;           // filled in to indicate which one is being edited
   int            iVScroll;         // so ObjEditorAttribEditPage retursn to the right place
   DWORD          dwMergeWith;      // element to merge with
} MD2PAGE, *PMD2PAGE;

/* PolyMeshAddMorphPage
*/
static BOOL PolyMeshAddMorphPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMD2PAGE po = (PMD2PAGE)pPage->m_pUserData;
   PCPolyMesh pm = po->pm;

   switch (dwMessage) {
   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"addmorphcombo")) {
            // if not morphs added then cant do this
            if (!NumMorphIDs(&pm->m_lPCOEAttrib)) {
               pPage->MBWarning (L"You must first add some morphs before you can add combo-morphs.");
               return TRUE;
            }

            pPage->Exit (L"addmorphcombo");
            return TRUE;
            }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Polygon mesh add morph";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}

/* PolyMeshMorphEditPage
*/
static BOOL PolyMeshMorphEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMD2PAGE po = (PMD2PAGE)pPage->m_pUserData;
   PCPolyMesh pm = po->pm;
   PCOEAttrib pa = (po->dwEdit < pm->m_lPCOEAttrib.Num()) ?
      *((PCOEAttrib*) pm->m_lPCOEAttrib.Get(po->dwEdit)) :
      NULL;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // if we have somplace to scroll to then do so
         if (po->iVScroll >= 0) {
            pPage->VScroll (po->iVScroll);

            // when bring up pop-up dialog often they're scrolled wrong because
            // iVScoll was left as valeu, and they use defpage
            po->iVScroll = 0;

            // BUGFIX - putting this invalidate in to hopefully fix a refresh
            // problem when add or move a task in the ProjectView page
            pPage->Invalidate();
         }

         // edit
         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), pa->m_szName);

         pControl = pPage->ControlFind (L"desc");
         if (pControl && pa->m_memInfo.p && ((PWSTR)pa->m_memInfo.p)[0])
            pControl->AttribSet (Text(), (PWSTR)pa->m_memInfo.p);

         pPage->Message (ESCM_USER+82);   // set the values based on infotype

         // combo
         ComboBoxSet (pPage, L"infotype", pa->m_dwInfoType);

         // bools
         pControl = pPage->ControlFind (L"deflowrank");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pa->m_fDefLowRank);

         pControl = pPage->ControlFind (L"defpassup");
         if (pControl)
            pControl->AttribSetBOOL (Checked(), pa->m_fDefPassUp);

         // do all the from (t:) values
         WCHAR szTemp[32];
         DWORD i, j;
         DWORD dwNum;
         PCOEATTRIBCOMBO pc;
         pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
         dwNum = pa->m_lCOEATTRIBCOMBO.Num();
         for (i = 0; i < dwNum; i++, pc++) {
            // set the bomxo
            swprintf (szTemp, L"cb:%d", (int) i);
            ComboBoxSet (pPage, szTemp, pc->dwMorphID);

            // set limit controls
            swprintf (szTemp, L"c0:%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), (pc->dwCapEnds & 0x01) ? TRUE : FALSE);
            swprintf (szTemp, L"c1:%d", (int) i);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetBOOL (Checked(), (pc->dwCapEnds & 0x02) ? TRUE : FALSE);

            DWORD dwFind;
            PCObjectSocket pos = NULL;
            ATTRIBINFO ai;
            memset (&ai, 0, sizeof(ai));
            dwFind = po->pWorld->ObjectFind (&pc->gFromObject);
            if (dwFind != -1)
               pos = po->pWorld->ObjectGet (dwFind);
            if (pos)
               pos->AttribInfo (pc->szName, &ai);

            for (j = 0; j < pc->dwNumRemap; j++) {
               swprintf (szTemp, L"t:%d%d", (int)j, (int)i);
               switch (ai.dwType) {
               case AIT_DISTANCE:
                  MeasureToString (pPage, szTemp, pc->afpObjectValue[j]);
                  break;
               case AIT_ANGLE:
                  AngleToControl (pPage, szTemp, pc->afpObjectValue[j]);
                  break;
               default:
                  DoubleToControl (pPage, szTemp, pc->afpObjectValue[j]);
                  break;
               }
            } // j
         } // i
      }
      break;

   case ESCM_USER+82:   // set the values based on m_dwInfoType
      {
         switch (pa->m_dwInfoType) {
         case AIT_DISTANCE:
            MeasureToString (pPage, L"min", pa->m_fMin);
            MeasureToString (pPage, L"max", pa->m_fMax);
            break;
         case AIT_ANGLE:
            AngleToControl (pPage, L"min", pa->m_fMin);
            AngleToControl (pPage, L"max", pa->m_fMax);
            break;
         default:
            DoubleToControl (pPage, L"min", pa->m_fMin);
            DoubleToControl (pPage, L"max", pa->m_fMax);
            break;
         }

         // do all the from (f:) values
         WCHAR szTemp[32];
         DWORD i, j;
         DWORD dwNum;
         PCOEATTRIBCOMBO pc;
         pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
         dwNum = pa->m_lCOEATTRIBCOMBO.Num();
         for (i = 0; i < dwNum; i++, pc++) {
            for (j = 0; j < pc->dwNumRemap; j++) {
               swprintf (szTemp, L"f:%d%d", (int)j, (int)i);
               switch (pa->m_dwInfoType) {
               case AIT_DISTANCE:
                  MeasureToString (pPage, szTemp, pc->afpComboValue[j]);
                  break;
               case AIT_ANGLE:
                  AngleToControl (pPage, szTemp, pc->afpComboValue[j]);
                  break;
               default:
                  DoubleToControl (pPage, szTemp, pc->afpComboValue[j]);
                  break;
               }
            }
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if ((psz[0] == L'f') && (psz[1] == L':')) {
            // change the from field
            DWORD i, j;
            PCOEATTRIBCOMBO pc;
            j = psz[2] - L'0';
            i = _wtoi(psz + 3);
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;
            if (j >= pc->dwNumRemap)
               return TRUE;

            po->pWorld->ObjectAboutToChange (po->pObject);

            switch (pa->m_dwInfoType) {
            case AIT_DISTANCE:
               MeasureParseString (pPage, p->pControl->m_pszName, &pc->afpComboValue[j]);
               break;
            case AIT_ANGLE:
               pc->afpComboValue[j] = AngleFromControl (pPage, p->pControl->m_pszName);
               break;
            default:
               pc->afpComboValue[j] = DoubleFromControl (pPage, p->pControl->m_pszName);
               break;
            }
            po->pWorld->ObjectChanged (po->pObject);
            return TRUE;
         }
         else if ((psz[0] == L't') && (psz[1] == L':')) {
            // change the from field
            DWORD i, j;
            PCOEATTRIBCOMBO pc;
            j = psz[2] - L'0';
            i = _wtoi(psz + 3);
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;
            if (j >= pc->dwNumRemap)
               return TRUE;

            DWORD dwFind;
            PCObjectSocket pos;
            ATTRIBINFO ai;
            pos = NULL;
            memset (&ai, 0, sizeof(ai));
            dwFind = po->pWorld->ObjectFind (&pc->gFromObject);
            if (dwFind != -1)
               pos = po->pWorld->ObjectGet (dwFind);
            if (pos)
               pos->AttribInfo (pc->szName, &ai);

            po->pWorld->ObjectAboutToChange (po->pObject);
            switch (ai.dwType) {
            case AIT_DISTANCE:
               MeasureParseString (pPage, p->pControl->m_pszName, &pc->afpObjectValue[j]);
               break;
            case AIT_ANGLE:
               pc->afpObjectValue[j] = AngleFromControl (pPage, p->pControl->m_pszName);
               break;
            default:
               pc->afpObjectValue[j] = DoubleFromControl (pPage, p->pControl->m_pszName);
               break;
            }
            po->pWorld->ObjectChanged (po->pObject);
            return TRUE;
         }

         // else cheap - since one edit changed just get all the values at once
         PCEscControl pControl;
         DWORD dwNeeded;

         po->pWorld->ObjectAboutToChange (po->pObject);
         // edit
         pControl = pPage->ControlFind (L"name");
         if (pControl) {
            pControl->AttribGet (Text(), pa->m_szName, sizeof(pa->m_szName), &dwNeeded);
         }

         pControl = pPage->ControlFind (L"desc");
         if (pControl && pa->m_memInfo.Required (256 * 2)) {
            ((PWSTR)pa->m_memInfo.p)[0] = 0; // so if attribget fails have null
            pControl->AttribGet (Text(), (PWSTR)pa->m_memInfo.p, (DWORD)pa->m_memInfo.m_dwAllocated, &dwNeeded);
         }

         switch (pa->m_dwInfoType) {
         case AIT_DISTANCE:
            MeasureParseString (pPage, L"min", &pa->m_fMin);
            MeasureParseString (pPage, L"max", &pa->m_fMax);
            break;
         case AIT_ANGLE:
            pa->m_fMin = AngleFromControl (pPage, L"min");
            pa->m_fMax = AngleFromControl (pPage, L"max");
            break;
         default:
            pa->m_fMin = DoubleFromControl (pPage, L"min");
            pa->m_fMax = DoubleFromControl (pPage, L"max");
            break;
         }

         // BUGFIX - Sort m_lPCOEAttrib list because name may have changed
         qsort (pm->m_lPCOEAttrib.Get(0), pm->m_lPCOEAttrib.Num(), sizeof(PCOEAttrib), OECALCompare);
         pm->CalcMorphRemap();
         pm->m_fDirtyMorphCOM = TRUE;

         po->pWorld->ObjectChanged (po->pObject);

         // refind the item
         PCOEAttrib *pap;
         DWORD i;
         pap = (PCOEAttrib*) pm->m_lPCOEAttrib.Get(0);
         for (i = 0; i < pm->m_lPCOEAttrib.Num(); i++)
            if (pap[i] == pa)
               break;
         po->dwEdit = i;
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"infotype")) {
            DWORD dw;
            dw = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;
            if (dw == pa->m_dwInfoType)
               break;   // no change

            po->pWorld->ObjectAboutToChange (po->pObject);
            pa->m_dwInfoType = dw;
            po->pWorld->ObjectChanged (po->pObject);

            // redisplay the values
            pPage->Message (ESCM_USER+82);   // set the values based on infotype
            return TRUE;
         }
         if (!_wcsicmp(psz, L"mergewith")) {
            // just remember the change
            po->dwMergeWith = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;
            return TRUE;
         }
         else if ((psz[0] == L'c') && (psz[1] == L'b') && (psz[2] == L':')) {
            DWORD dw = p->pszName ? (DWORD)_wtoi(p->pszName) : 0;
            DWORD dwElem = _wtoi(psz + 3);
            if (dwElem >= pa->m_lCOEATTRIBCOMBO.Num())
               return FALSE;

            // get the value
            PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(dwElem);
            if (pc->dwMorphID == dw)
               return TRUE;   // nothing to do

            po->pWorld->ObjectAboutToChange (po->pObject);
            pc->dwMorphID = dw;
            po->pWorld->ObjectChanged (po->pObject);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
      }
      break;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;


         if (psz[0] == L'r' && psz[1] == L'c' && psz[2] == L':') {
            DWORD i;
            i = _wtoi(psz + 3);
            PCOEATTRIBCOMBO pc;
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            if (IDYES != pPage->MBYesNo (L"Are you sure you want to remove the effect?"))
               return TRUE;

            po->pWorld->ObjectAboutToChange (po->pObject);
            pa->m_lCOEATTRIBCOMBO.Remove (i);
            po->pWorld->ObjectChanged (po->pObject);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (psz[0] == L'a' && psz[1] == L'm' && psz[2] == L':') {
            DWORD i;
            i = _wtoi(psz + 3);
            PCOEATTRIBCOMBO pc;
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            po->pWorld->ObjectAboutToChange (po->pObject);
            pc->afpComboValue[pc->dwNumRemap] = pc->afpComboValue[pc->dwNumRemap-1];
            pc->afpObjectValue[pc->dwNumRemap] = pc->afpObjectValue[pc->dwNumRemap-1];
            pc->dwNumRemap++;
            po->pWorld->ObjectChanged (po->pObject);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (psz[0] == L'r' && psz[1] == L'm' && psz[2] == L':') {
            DWORD i;
            i = _wtoi(psz + 3);
            PCOEATTRIBCOMBO pc;
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            po->pWorld->ObjectAboutToChange (po->pObject);
            pc->dwNumRemap--;
            po->pWorld->ObjectChanged (po->pObject);

            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if ((psz[0] == L'c') && ((psz[1] == L'0') || (psz[1] == L'1')) && (psz[2] == L':')) {
            DWORD i, j;
            PCOEATTRIBCOMBO pc;
            j = psz[1] - L'0';
            i = _wtoi(psz + 3);
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(i);
            if (!pc)
               return TRUE;

            po->pWorld->ObjectAboutToChange (po->pObject);
            if (p->pControl->AttribGetBOOL (Checked())) {
               pc->dwCapEnds |= (1 << j);
            }
            else {
               pc->dwCapEnds &= ~(1<<j);
            }
            po->pWorld->ObjectChanged (po->pObject);

            return TRUE;
         }
         else if (!_wcsicmp(psz, L"deflowrank")) {
            po->pWorld->ObjectAboutToChange (po->pObject);
            pa->m_fDefLowRank = p->pControl->AttribGetBOOL (Checked());
            po->pWorld->ObjectChanged (po->pObject);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"defpassup")) {
            po->pWorld->ObjectAboutToChange (po->pObject);
            pa->m_fDefPassUp = p->pControl->AttribGetBOOL (Checked());
            po->pWorld->ObjectChanged (po->pObject);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"addatt")) {
            po->pWorld->ObjectAboutToChange (po->pObject);

            COEATTRIBCOMBO c;
            memset (&c, 0, sizeof(c));
            c.afpComboValue[0] = c.afpObjectValue[0] = 0;
            c.afpComboValue[1] = c.afpObjectValue[1] = 1;
            c.dwNumRemap = 2;
            c.dwMorphID = 0;  // just start out with default value that can change later
            pa->m_lCOEATTRIBCOMBO.Add (&c);
            po->pWorld->ObjectChanged (po->pObject);
            pPage->Exit (RedoSamePage());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"removemorph")) {
            DWORD dwIndex;
            dwIndex = po->dwEdit;

            // verify
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete the morph?",
               L"Deleting it will permenantly remove the attribute."))
               return TRUE;

            // remove it
            po->pWorld->ObjectAboutToChange (po->pObject);
            pm->m_lPCOEAttrib.Remove (dwIndex);
            po->dwEdit = 0;
            if ((pa->m_dwType == 2) && pa->m_lCOEATTRIBCOMBO.Num()) {
               // which item is removed
               PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
               DWORD dwRemove = pc->dwMorphID;

               // remove this from all vertecies
               DWORD i;
               for (i = 0; ; i++) {
                  PCPMVert pv = pm->VertGet(i);
                  if (!pv)
                     break;
                  pv->VertDeformRemove (dwRemove);
               }
               //PCPMVertOld pVert;
               //pVert = (PCPMVertOld) pm->m_lCPMVert.Get(0);
               //for (i = 0; i < pm->m_lCPMVert.Num(); i++, pVert++)
               //   pVert->DeformRemove (dwRemove);


               // loop through all entries
               for (i = pm->m_lPCOEAttrib.Num()-1; i < pm->m_lPCOEAttrib.Num(); i--) {
                  PCOEAttrib p = *((PCOEAttrib*) pm->m_lPCOEAttrib.Get(i));
                  DWORD j;
                  for (j = p->m_lCOEATTRIBCOMBO.Num()-1; j < p->m_lCOEATTRIBCOMBO.Num(); j--) {
                     pc = (PCOEATTRIBCOMBO) p->m_lCOEATTRIBCOMBO.Get(j);
                     if (pc->dwMorphID < dwRemove)
                        continue;   // less than, so no change
                     if (pc->dwMorphID > dwRemove) {
                        // greater than so decrease
                        pc->dwMorphID--;
                        continue;
                     }

                     // else, match, so remove this
                     p->m_lCOEATTRIBCOMBO.Remove (j);
                  } // j - over attribcombos

                  // if nothing left in the combo then remove the combo
                  if (!p->m_lCOEATTRIBCOMBO.Num()) {
                     // know that it's a combo so dont have to check that deleing
                     // this will interfere with anything
                     delete p;
                     pm->m_lPCOEAttrib.Remove (i);
                  }
               } // i - over attributes
            }  // if delete non-combo
            delete pa;
            //pm->m_fDivDirty = TRUE; // since may change appearance
            pm->CalcMorphRemap(); // redo morph remap since may have changed
            pm->m_fDirtyMorphCOM = TRUE;
            po->pWorld->ObjectChanged (po->pObject);

            // quit
            pPage->Exit (Back());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"addmorphcopy") || !_wcsicmp(psz, L"addmorphmirrorlr") || !_wcsicmp(psz, L"addmorphmirrorfb") || !_wcsicmp(psz, L"addmorphmirrortb")) {
            DWORD dwDim;
            if (!_wcsicmp(psz, L"addmorphmirrorlr"))
               dwDim = 0;
            else if (!_wcsicmp(psz, L"addmorphmirrorfb"))
               dwDim = 1;
            else if (!_wcsicmp(psz, L"addmorphcopy"))
               dwDim = -1;
            else
               dwDim = 2;

            // get the one
            DWORD dwIndex;
            dwIndex = po->dwEdit;

            // get this morph
            if (pa->m_dwType != 2) {
               pPage->MBWarning (L"You cannot copy/mirror this morph since it is a combo-morph.");
               return TRUE;
            }

            // create a clone
            PCOEAttrib pNew;
            pNew = new COEAttrib;
            if (!pNew)
               return TRUE;
            pa->CloneTo (pNew);

            // unique name
            PWSTR pszMirror = (dwDim != -1) ? L" (Mirror)" : L" (Copy)";
            DWORD dwLenMirror = (DWORD)wcslen(pszMirror);
            pNew->m_szName[63-dwLenMirror] = 0; // so have enough space to append
            wcscat (pNew->m_szName, pszMirror);
            MakeAttribNameUnique (pNew->m_szName, &pm->m_lPCOEAttrib);

            // find the max
            DWORD dwMax;
            dwMax = NumMorphIDs (&pm->m_lPCOEAttrib);

            // this one will use that morh
            PCOEATTRIBCOMBO pc;
            DWORD dwOld;
            pc = (PCOEATTRIBCOMBO) pNew->m_lCOEATTRIBCOMBO.Get(0);
            dwOld = pc->dwMorphID;
            
            // add new attributes
            po->pWorld->ObjectAboutToChange (po->pObject);
            pc->dwMorphID = dwMax;
            dwMax++;
            pm->m_lPCOEAttrib.Add (&pNew);

            // loop through and do mirroring
            DWORD i,j,k;
            //PCPMVertOld pVert;
            PVERTDEFORM pvd;
            VERTDEFORM vd;
            CPoint   pLoc;
            //dwNum = pm->m_lCPMVert.Num();
            //pVert = (PCPMVertOld) pm->m_lCPMVert.Get(0);
            for (i = 0; ; i++) {
               PCPMVert pv = pm->VertGet(i);
               if (!pv)
                  break;

               // see if can find a copy of the old morph
               if (!pv->m_wNumVertDeform)
                  continue;
               pvd = pv->VertDeformGet();
               for (k = 0; k < pv->m_wNumVertDeform; k++)
                  if (pvd[k].dwDeform == dwOld)
                     break;
               if (k >= pv->m_wNumVertDeform)
                  continue;   // didn't have the deformation in it

               // remember this
               vd = pvd[k];
               pLoc.Copy (&pv->m_pLoc);
               if (dwDim != -1) {
                  vd.pDeform.p[dwDim] *= -1;
                  pLoc.p[dwDim] *= -1;
               }
               vd.dwDeform = pc->dwMorphID;

               // now find a match
               PCPMVert pMir;
               if (dwDim != -1) {
                  PMMIRROR *pMirror = pv->MirrorGet ();
                  for (j = 0; j < pv->m_wNumMirror; j++) {
                     pMir = pm->VertGet(pMirror[j].dwObject);
                     if (pMir->m_pLoc.AreClose (&pLoc))
                        break;
                  } // j
                  if (j >= pv->m_wNumMirror)
                     continue;   // no match found
                  j = pMirror[j].dwObject;
               }
               else
                  j = i;   // same object
               pMir = pm->VertGet(j);

               // else, have a match, so add it
               pMir->VertDeformAdd (&vd);
            }

            // done
            //pm->m_fDivDirty = TRUE; // since may change appearance
            qsort (pm->m_lPCOEAttrib.Get(0), pm->m_lPCOEAttrib.Num(), sizeof(PCOEAttrib), OECALCompare);
            pm->CalcMorphRemap(); // redo morph remap since may have changed
            pm->m_fDirtyMorphCOM = TRUE;
            po->pWorld->ObjectChanged (po->pObject);

            // quit
            pPage->Exit (Back());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"addmorphsplitlr") || !_wcsicmp(psz, L"addmorphsplitfb") || !_wcsicmp(psz, L"addmorphsplittb")) {
            DWORD dwDim;
            if (!_wcsicmp(psz, L"addmorphsplitlr"))
               dwDim = 0;
            else if (!_wcsicmp(psz, L"addmorphsplitfb"))
               dwDim = 1;
            else
               dwDim = 2;

            fp fDistance = 0;
            MeasureParseString (pPage, L"blenddist", &fDistance);
            fDistance /= 2.0;

            // get this morph
            if (pa->m_dwType != 2) {
               pPage->MBWarning (L"You cannot copy/mirror this morph since it is a combo-morph.");
               return TRUE;
            }

            // create two clones
            PCOEAttrib pNewA, pNewB;
            pNewA = new COEAttrib;
            if (!pNewA)
               return TRUE;
            pNewB = new COEAttrib;
            if (!pNewB) {
               delete pNewA;
               return TRUE;
            }
            pa->CloneTo (pNewA);
            pa->CloneTo (pNewB);

            // unique name
            PWSTR pszMirrorA = L" (Split A)";
            DWORD dwLenMirrorA = (DWORD)wcslen(pszMirrorA);
            pNewA->m_szName[63-dwLenMirrorA] = 0; // so have enough space to append
            wcscat (pNewA->m_szName, pszMirrorA);
            MakeAttribNameUnique (pNewA->m_szName, &pm->m_lPCOEAttrib);

            // unique name
            PWSTR pszMirrorB = L" (Split B)";
            DWORD dwLenMirrorB = (DWORD)wcslen(pszMirrorB);
            pNewB->m_szName[63-dwLenMirrorB] = 0; // so have enough space to append
            wcscat (pNewB->m_szName, pszMirrorB);
            MakeAttribNameUnique (pNewB->m_szName, &pm->m_lPCOEAttrib);

            // find the max
            DWORD dwMax;
            dwMax = NumMorphIDs (&pm->m_lPCOEAttrib);

            // this one will use that morh
            PCOEATTRIBCOMBO pcA, pcB;
            DWORD dwOld;
            pcA = (PCOEATTRIBCOMBO) pNewA->m_lCOEATTRIBCOMBO.Get(0);
            dwOld = pcA->dwMorphID;  // old for A and B are the same
            pcB = (PCOEATTRIBCOMBO) pNewB->m_lCOEATTRIBCOMBO.Get(0);
            
            // add new attributes
            po->pWorld->ObjectAboutToChange (po->pObject);
            pcA->dwMorphID = dwMax;
            pcB->dwMorphID = dwMax+1;
            dwMax += 2; // since 2 objects
            pm->m_lPCOEAttrib.Add (&pNewA);
            pm->m_lPCOEAttrib.Add (&pNewB);

            // loop through and do mirroring
            DWORD i,k;
            //PCPMVertOld pVert;
            PVERTDEFORM pvd;
            VERTDEFORM vdA, vdB;
            //dwNum = pm->m_lCPMVert.Num();
            //pVert = (PCPMVertOld) pm->m_lCPMVert.Get(0);
            for (i = 0; ; i++) {
               PCPMVert pv = pm->VertGet(i);
               if (!pv)
                  break;

               // see if can find a copy of the old morph
               if (!pv->m_wNumVertDeform)
                  continue;
               pvd = pv->VertDeformGet();
               for (k = 0; k < pv->m_wNumVertDeform; k++)
                  if (pvd[k].dwDeform == dwOld)
                     break;
               if (k >= pv->m_wNumVertDeform)
                  continue;   // didn't have the deformation in it

               // remember this
               vdA = pvd[k];
               vdA.dwDeform = pcA->dwMorphID;
               vdB = pvd[k];
               vdB.dwDeform = pcB->dwMorphID;
               fp fVal = pv->m_pLoc.p[dwDim];

               if (fVal <= -fDistance)
                  fVal = 0;   // all of one and not the other
               else if (fVal >= fDistance)
                  fVal = 1;   // all of one and not the other
               else {
                  fVal = sin((fVal + fDistance) / (2.0 * fDistance) * (PI / 2.0));
                  fVal *= fVal;  // use sin-square so sum to 1
               }

               if (fVal > 0) {
                  vdA.pDeform.Scale (fVal);
                  pv->VertDeformAdd (&vdA);
               }
               fVal = 1.0 - fVal;
               if (fVal > 0) {
                  vdB.pDeform.Scale (fVal);
                  pv->VertDeformAdd (&vdB);
               }
            }

            // done
            //pm->m_fDivDirty = TRUE; // since may change appearance
            qsort (pm->m_lPCOEAttrib.Get(0), pm->m_lPCOEAttrib.Num(), sizeof(PCOEAttrib), OECALCompare);
            pm->CalcMorphRemap(); // redo morph remap since may have changed
            pm->m_fDirtyMorphCOM = TRUE;
            po->pWorld->ObjectChanged (po->pObject);

            // quit
            pPage->Exit (Back());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"addmorphmerge")) {
            // get this morph
            if (pa->m_dwType != 2) {
               pPage->MBWarning (L"You cannot copy/mirror this morph since it is a combo-morph.");
               return TRUE;
            }


            // and the combo box
            PCOEAttrib *ppa = (PCOEAttrib*) pm->m_lPCOEAttrib.Get(0);
            PCOEAttrib paB = (po->dwMergeWith < pm->m_lPCOEAttrib.Num()) ? ppa[po->dwMergeWith] : NULL;
            if (!paB || (paB->m_dwType == 1)) { // BUGFIX - Was dwType != 2, but changed to == 1, for combo morph
               pPage->MBWarning (L"You cannot merge with this morph since it is a combo-morph.");
               return TRUE;
            }            // create a clone

            PCOEAttrib pNew;
            pNew = new COEAttrib;
            if (!pNew)
               return TRUE;
            pa->CloneTo (pNew);

            // unique name
            PWSTR pszMirror = L" (Merge)";
            DWORD dwLenMirror = (DWORD)wcslen(pszMirror);
            pNew->m_szName[63-dwLenMirror] = 0; // so have enough space to append
            wcscat (pNew->m_szName, pszMirror);
            MakeAttribNameUnique (pNew->m_szName, &pm->m_lPCOEAttrib);

            // find the max
            DWORD dwMax;
            dwMax = NumMorphIDs (&pm->m_lPCOEAttrib);

            // this one will use that morh
            PCOEATTRIBCOMBO pc;
            DWORD dwOld;
            pc = (PCOEATTRIBCOMBO) pNew->m_lCOEATTRIBCOMBO.Get(0);
            dwOld = pc->dwMorphID;
            
            // add new attributes
            po->pWorld->ObjectAboutToChange (po->pObject);
            pc->dwMorphID = dwMax;
            dwMax++;
            pm->m_lPCOEAttrib.Add (&pNew);

            // loop through and do mirroring
            DWORD i,k;
            //PCPMVertOld pVert;
            PVERTDEFORM pvd;
            VERTDEFORM vd;
            //dwNum = pm->m_lCPMVert.Num();
            //pVert = (PCPMVertOld) pm->m_lCPMVert.Get(0);
            for (i = 0; ; i++) {
               PCPMVert pv = pm->VertGet(i);
               if (!pv)
                  break;

               // see if can find a copy of the old morph
               if (!pv->m_wNumVertDeform)
                  continue;
               pvd = pv->VertDeformGet();
               DWORD dwOrig = (DWORD)-1, dwWith = (DWORD)-1;
               for (k = 0; k < pv->m_wNumVertDeform; k++) {
                  // see if original is there
                  if (pvd[k].dwDeform == dwOld)
                     dwOrig = k;
                  
                  // see if merge with is there
                  if (pvd[k].dwDeform == po->dwMergeWith)
                     dwWith = k;
               }
               if ((dwOrig == (DWORD)-1) && (dwWith == (DWORD)-1))
                  continue;   // neither has an entry
               
               // remember this
               memset (&vd, 0, sizeof(vd));
               vd.dwDeform = pc->dwMorphID;
               vd.pDeform.Zero();
               if (dwOrig != (DWORD)-1)
                  vd.pDeform.Add (&pvd[dwOrig].pDeform);
               if (dwWith != (DWORD)-1)
                  vd.pDeform.Add (&pvd[dwWith].pDeform);

               // add it
               pv->VertDeformAdd (&vd);
            }

            // done
            //pm->m_fDivDirty = TRUE; // since may change appearance
            qsort (pm->m_lPCOEAttrib.Get(0), pm->m_lPCOEAttrib.Num(), sizeof(PCOEAttrib), OECALCompare);
            pm->CalcMorphRemap(); // redo morph remap since may have changed
            pm->m_fDirtyMorphCOM = TRUE;
            po->pWorld->ObjectChanged (po->pObject);

            // quit
            pPage->Exit (Back());
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Modify a morph";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"COMBOMORPH")) {
            MemZero (&gMemTemp);

                     // define a macro
            DWORD j;
            DWORD dwFind;
            PCOEATTRIBCOMBO pc;
            DWORD dwNum;
            PCOEAttrib *pap = (PCOEAttrib*) pm->m_lPCOEAttrib.Get(0);
            MemCat (&gMemTemp, L"<!xComboMorph>"
               L"<combobox width=50%% cbheight=150 macroattribute=1>");
            for (j = 0; j < pm->m_lPCOEAttrib.Num(); j++) {
               if (pap[j]->m_dwType != 2)
                  continue;   // only care about built in
               dwNum = pap[j]->m_lCOEATTRIBCOMBO.Num();
               pc = (PCOEATTRIBCOMBO) pap[j]->m_lCOEATTRIBCOMBO.Get(0);
               if (!dwNum)
                  continue;   // not enough
               dwFind = pc[0].dwMorphID;
               MemCat (&gMemTemp, L"<elem name=");
               MemCat (&gMemTemp, (int) dwFind);
               MemCat (&gMemTemp, L">");
               MemCatSanitize (&gMemTemp, pap[j]->m_szName);
               MemCat (&gMemTemp, L"</elem>");
            }
            MemCat (&gMemTemp, L"</combobox>"
               L"</xComboMorph>");
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ATTRIBLIST")) {
            // only do this for combo attributes
            if (pa->m_dwType != 0)
               return FALSE;

            MemZero (&gMemTemp);

            MemCat (&gMemTemp, L"<xbr/><xSectionTitle>Affected sub-morphs</xSectionTitle>"
               L"<p>"
               L"Whenever you adjust the combo-morph's attribute, the polygon mesh will "
               L"in turn adjust one or more \"sub\" morphs within the mesh. You can use to "
               L"to combine two or more morphs into one easy slider. For example: If you "
               L"have a \"left eyebrow\" morp and a \"right eyebrow\" morph, you can make "
               L"a combo-morph called \"both eyebrows\" that uses both the left and right eyebrows as sub-morphs, so "
               L"that both move with the same slider."
               L"</p>"
               );


            DWORD i, j;
            DWORD dwFind;
            PCOEATTRIBCOMBO pc;
            DWORD dwNum;
            dwNum = pa->m_lCOEATTRIBCOMBO.Num();
            pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
            PCOEAttrib *pap = (PCOEAttrib*) pm->m_lPCOEAttrib.Get(0);
            for (i = 0; i < dwNum; i++, pc++) {
               // get the name
               PWSTR pszName;
               pszName = NULL;
               for (j = 0; j < pm->m_lPCOEAttrib.Num(); j++) {
                  if (pap[j]->m_dwType != 2)
                     continue;   // only care about built in
                  if (!pap[j]->m_lCOEATTRIBCOMBO.Num())
                     continue;   // not enough
                  PCOEATTRIBCOMBO p2;
                  p2 = (PCOEATTRIBCOMBO) pap[j]->m_lCOEATTRIBCOMBO.Get(0);
                  dwFind = p2->dwMorphID;
                  if (dwFind == pc->dwMorphID) {
                     pszName = pap[j]->m_szName;
                     break;
                  }
               }
               if (!pszName) {
                  pszName =  L"Unknown";
                  dwFind = 0;
               }

               MemCat (&gMemTemp, L"<xtablecenter width=100%%>");
               MemCat (&gMemTemp, L"<xtrheader>");
               MemCat (&gMemTemp, L"<xComboMorph name=cb:");
               MemCat (&gMemTemp, (int)i);
               MemCat (&gMemTemp, L"/>");
               MemCat (&gMemTemp, L"</xtrheader>");
               MemCat (&gMemTemp, L"<tr><td>");
               MemCat (&gMemTemp, L"Below is a table that describes how changes to "
                  L"the current attribute morph <italic>");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L"</italic>. The left column is the value of the edited combo-morph "
                  L"and the right is what the value is changed to for the sub-morph.<p/>");
               MemCat (&gMemTemp, L"<p align=center><table width=66%%>");
               MemCat (&gMemTemp, L"<tr><td><bold>This</bold></td><td><bold>");
               MemCatSanitize (&gMemTemp, pszName);
               MemCat (&gMemTemp, L"</bold></td></tr>");
               for (j = 0; j < pc->dwNumRemap; j++) {
                  MemCat (&gMemTemp, L"<tr>");
                  MemCat (&gMemTemp, L"<td><edit maxchars=32 width=100% selall=true name=f:");
                  MemCat (&gMemTemp, (int) j);  // since only one char
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");

                  MemCat (&gMemTemp, L"<td><edit maxchars=32 width=100% selall=true name=t:");
                  MemCat (&gMemTemp, (int) j);  // since only one char
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"/></td>");
                  MemCat (&gMemTemp, L"</tr>");
               }

               // button for add
               if (pc->dwNumRemap + 1 < COEMAXREMAP) {
                  MemCat (&gMemTemp, L"<tr><td><xChoiceButton style=righttriangle name=am:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"><bold>Add another row</bold><br/>"
                     L"Adds a row to the end of the list, providing you with more "
                     L"control over how changing this combo-morph affect the sub-morph."
                     L"</xChoiceButton></td></tr>");
               }
               if (pc->dwNumRemap > 2) {
                  MemCat (&gMemTemp, L"<tr><td><xChoiceButton style=righttriangle name=rm:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"><bold>Remove last row</bold><br/>"
                     L"Removes the last row."
                     L"</xChoiceButton></td></tr>");
               }

               MemCat (&gMemTemp, L"</table></p>");

               MemCat (&gMemTemp, L"<xChoiceButton checkbox=true style=x name=c0:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Limit low values</bold><br/>"
                  L"If checked, then the morph attribute cannot be set below the minimum value "
                  L"(first row, left column). If unchecked, the morph attribute can be set "
                  L"below the minimum value shown."
                  L"</xChoiceButton>");

               MemCat (&gMemTemp, L"<xChoiceButton checkbox=true style=x name=c1:");
               MemCat (&gMemTemp, (int) i);
               MemCat (&gMemTemp, L"><bold>Limit high values</bold><br/>"
                  L"If checked, then the morph attribute cannot be set above the maximum value "
                  L"(last row, left column). If unchecked, the morph attribute can be set "
                  L"above the maximum value shown."
                  L"</xChoiceButton>");

               if (dwNum > 1) {
                  MemCat (&gMemTemp, L"<xChoiceButton style=rightarrow name=rc:");
                  MemCat (&gMemTemp, (int) i);
                  MemCat (&gMemTemp, L"><bold>Delete this effect</bold><br/>"
                     L"If you do this changing this attribute will no longer affect ");
                  MemCatSanitize (&gMemTemp, pszName);
                  MemCat (&gMemTemp, L".</xChoiceButton>");
               }
               MemCat (&gMemTemp, L"</td></tr>");
               MemCat (&gMemTemp, L"</xtablecenter>");
            }

            if (NumMorphIDs(&pm->m_lPCOEAttrib)) {
               MemCat (&gMemTemp, L"<xChoiceButton name=addatt>"
                  L"<bold>Affect an additonal sub-morph</bold><br/>"
                  L"Press this to have this combo-morph affect one or more other "
                  L"sub-morphs.</xChoiceButton>");
            }
            p->pszSubString = (PWSTR) gMemTemp.p;
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}



/**********************************************************************************
CPolyMesh::DialogAddMorph - UI for adding a morph

inputs
   PCEscWindow       pWindow - Escarpment window to draw it in
   PCWorldSocket     pWorld - Notify when object changes
   PCObjectSocket    pObject - Notify when object changes
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CPolyMesh::DialogAddMorph (PCEscWindow pWindow, PCWorldSocket pWorld, PCObjectSocket pObject)
{
   PWSTR pszRet;
   DWORD i;
   MD2PAGE oe;
   memset (&oe, 0, sizeof(oe));
   oe.pm = this;
   oe.pWorld = pWorld;
   oe.pObject = pObject;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPOLYMESHADDMORPH, PolyMeshAddMorphPage, &oe);
   if (!pszRet)
      return FALSE;
   if (!_wcsicmp(pszRet, L"addmorph") || !_wcsicmp(pszRet, L"addmorphcombo")) {
      // create a new one
      PCOEAttrib pNew = new COEAttrib;
      if (!pNew)
         return FALSE;
      BOOL fCombo;
      fCombo = !_wcsicmp(pszRet, L"addmorphcombo");
      pNew->m_dwInfoType = AIT_NUMBER;
      pNew->m_dwType = fCombo ? 0 : 2;  // just a morph
      pNew->m_fAutomatic = FALSE;
      pNew->m_fCurValue = 0;
      pNew->m_fDefLowRank = FALSE;
      pNew->m_fDefPassUp = TRUE;
      pNew->m_fDefValue = 0;
      pNew->m_fMax = 1;
      pNew->m_fMin = 0;
      wcscpy (pNew->m_szName, fCombo ? L"Combo-morph" : L"New morph");
      MakeAttribNameUnique (pNew->m_szName, &m_lPCOEAttrib);

      COEATTRIBCOMBO ac;
      memset (&ac, 0, sizeof(ac));
      ac.afpComboValue[0] = ac.afpObjectValue[0] = 0;
      ac.afpComboValue[1] = ac.afpObjectValue[1] = 1;
      ac.dwCapEnds = 0;
      ac.dwNumRemap = 2;
      ac.dwMorphID = fCombo ? 0 : NumMorphIDs(&m_lPCOEAttrib);

      pNew->m_lCOEATTRIBCOMBO.Add (&ac);

      // add to list
      pWorld->ObjectAboutToChange (pObject);
      m_lPCOEAttrib.Add (&pNew);

      // Sort m_lPCOEAttrib list
      qsort (m_lPCOEAttrib.Get(0), m_lPCOEAttrib.Num(), sizeof(PCOEAttrib), OECALCompare);
      m_fDirtyMorphCOM = TRUE;

      PCOEAttrib *pap;
      pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
      for (i = 0; i < m_lPCOEAttrib.Num(); i++)
         if (pap[i] == pNew)
            break;
      oe.dwEdit = i;

      oe.iVScroll = 0;

      // set this to the one being worked on
      MorphStateSet (oe.dwEdit);

      // world changed
      pWorld->ObjectChanged (pObject);

      return DialogEditMorph (pWindow, pWorld, pObject, oe.dwEdit);
   }

   return !_wcsicmp(pszRet, Back());
}


/**********************************************************************************
CPolyMesh::DialogEditMorph - Causes the object to show a dialog box that allows
editing of the morph.

inputs
   PCEscWindow       pWindow - Escarpment window to draw it in
   PCWorldSocket     pWorld - Notify when object changes
   PCObjectSocket    pObject - Notify when object changes
   DWORD             dwMorph - Morph index to edit, from 0 to #morphs-1
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CPolyMesh::DialogEditMorph (PCEscWindow pWindow, PCWorldSocket pWorld, PCObjectSocket pObject,
                                 DWORD dwMorph)
{
   PWSTR pszRet;
   DWORD i;
   MD2PAGE oe;
   memset (&oe, 0, sizeof(oe));
   oe.pm = this;
   oe.pWorld = pWorld;
   oe.pObject = pObject;
   oe.dwEdit = dwMorph;

edit:
   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPOLYMESHMORPHEDIT, PolyMeshMorphEditPage, &oe);

   pWorld->ObjectAboutToChange (pObject);
   m_fDirtyRender = TRUE;  // since may change appearance
   m_fDirtyMorphCOM = TRUE;

   // make sure the name is unique
   PCOEAttrib pa;
   pa = *((PCOEAttrib*) m_lPCOEAttrib.Get(oe.dwEdit));
   MakeAttribNameUnique (pa->m_szName, &m_lPCOEAttrib, oe.dwEdit);

   // repick th edit
   PCOEAttrib *pap;
   pap = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   for (i = 0; i < m_lPCOEAttrib.Num(); i++)
      if (pap[i] == pa)
         break;
   oe.dwEdit = i;

   // make sure the mapped values are sequential
   PCOEATTRIBCOMBO pc;
   DWORD j;
   pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
   for (i = 0; i < pa->m_lCOEATTRIBCOMBO.Num(); i++, pc++) {
      for (j = 1; j < pc->dwNumRemap; j++)
         pc->afpComboValue[j] = max(pc->afpComboValue[j], pc->afpComboValue[j-1]);
   }

   // BUGFIX - while at it rebuild the sub-morph changed list
   CalcMorphRemap();
   m_fDirtyRender = TRUE;

   pWorld->ObjectChanged (pObject);

   if (!pszRet)
      return FALSE;
   oe.iVScroll = pWindow->m_iExitVScroll;
   if (pszRet && !_wcsicmp(pszRet, RedoSamePage()))
      goto edit;

   return !_wcsicmp(pszRet, Back());
}

/*****************************************************************************************
CPolyMesh::CanModify - Returns TRUE if can modify. FALSE if cant modify. Wont be
able to modify if there's more than one morph active.
*/
BOOL CPolyMesh::CanModify (void)
{
   // if multiple morphs then cant modify
   if (m_lMorphRemapID.Num() >= 2)
      return FALSE;

   // If bones on then dont do
   if (m_fBoneChangeShape)
      return FALSE;

   return TRUE;
}


/*****************************************************************************************
CPolyMesh::MorphStateGet - If no morphs are active, then this returns the number of
morphs. If only one is active it returns that morph number. If more than one is
active it returns -1
*/
DWORD CPolyMesh::MorphStateGet (void)
{
   DWORD dwActive = -1;
   PCOEAttrib *ppa = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCOEAttrib.Num(); i++) {
      if (!ppa[i]->m_fCurValue)
         continue;

      // else active
      if (dwActive != -1)
         return -1;  // already had acive
      dwActive = i;
   }

   return (dwActive == -1) ? m_lPCOEAttrib.Num() : dwActive;
}


/*****************************************************************************************
CPolyMesh::MorphStateSet - Changes the current morph state. Like an attribset but
faster.

inputs
   DWORD       dwState - New State, 0..#attrib-1 to indicate that one is active,
                  #attrib to clear out all active
returns
   none
*/
void CPolyMesh::MorphStateSet (DWORD dwState)
{
   PCOEAttrib *ppa = (PCOEAttrib*) m_lPCOEAttrib.Get(0);
   DWORD i;
   for (i = 0; i < m_lPCOEAttrib.Num(); i++) {
      ppa[i]->m_fCurValue = ((i == dwState) ? 1 : 0);
   }

   CalcMorphRemap();
}



/*****************************************************************************************
CPolyMesh::MorphScale - Scales all the active morphs.

inputs
   PCPoint           pScale - Amount to scale by in xyz.
returns
   BOOL - TRUE if success, FALSE if not because morphs going on something
*/
BOOL CPolyMesh::MorphScale (PCPoint pScale)
{
   // figure out what gets scaled
   DWORD dwNum = m_lMorphRemapID.Num();
   DWORD *padw = (DWORD*)m_lMorphRemapID.Get(0);
   if (!dwNum)
      return FALSE;  // must have something active

   // do scaling
   DWORD i, j;
   PCPMVert *ppv = (PCPMVert*)m_lVert.Get(0);
   for (i = 0; i < m_lVert.Num(); i++) {
      PCPMVert pv = ppv[i];
      PVERTDEFORM pvd = pv->VertDeformGet();
      for (j = 0; j < pv->m_wNumVertDeform; j++) {
         // if this morph isnt on the list of ones to modify then ignore
         if (-1 == DWORDSearch(pvd[j].dwDeform, dwNum, padw))
            continue;

         // scale
         pvd[j].pDeform.p[0] *= pScale->p[0];
         pvd[j].pDeform.p[1] *= pScale->p[1];
         pvd[j].pDeform.p[2] *= pScale->p[2];
      } // j
   } // i


   // dirty
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtyMorphCOM = TRUE;

   return TRUE;
}


/*****************************************************************************************
CPolyMesh::SurfaceInMetersGet - Returns TRUE if the surface should be treated as though
the measurement is in meters, FALSE if it should just be stretched
*/
BOOL CPolyMesh::SurfaceInMetersGet (DWORD dwSurface)
{
   return (-1 == DWORDSearch (dwSurface, m_lSurfaceNotInMeters.Num(), (DWORD*)m_lSurfaceNotInMeters.Get(0)));
}


/*****************************************************************************************
CPolyMesh::SurfaceInMetersSet - Sets whether a texture index for a surface is treated
as though it's in meters (default) or if it's in HV texture units.

inputs
   DWORD       dwSurface - Surface number
   BOOL        fInMeters - If TRUE then set to to using meters, FALSE not
*/
void CPolyMesh::SurfaceInMetersSet (DWORD dwSurface, BOOL fInMeters)
{
   // find it
   DWORD dwFind = DWORDSearch (dwSurface, m_lSurfaceNotInMeters.Num(), (DWORD*)m_lSurfaceNotInMeters.Get(0));

   // did find?
   if (dwFind == -1) {
      // not on the list, so it's in meters
      if (fInMeters)
         return;  // do nothing

      // else, add
      m_lSurfaceNotInMeters.Add (&dwSurface);
      qsort (m_lSurfaceNotInMeters.Get(0), m_lSurfaceNotInMeters.Num(), sizeof(DWORD), BDWORDCompare);

      m_fDirtyRender = TRUE;
   }
   else {
      // on the list so it's not in meters
      if (!fInMeters)
         return;  // do nothing

      // else, remove
      m_lSurfaceNotInMeters.Remove (dwFind);
      m_fDirtyRender = TRUE;
   }
}


/*****************************************************************************************
CPolyMesh::SurfacePaint - Paints a given texture onto the object.

inputs
   DWORD             dwNumSide - Number of sides to limit to. If NULL then allow all sides
   DWORD             *padwSide - Array of dwNumSide sides
   DWORD             dwSurface - Surface to set to. (Linked to CObjectTemplate::ObjectSurfaceFind())
returns
   BOOL - TRUE if success, FALSE if not because morphs going on something
*/
BOOL CPolyMesh::SurfacePaint (DWORD dwNumSide, DWORD *padwSide, DWORD dwSurface)
{
   // calc symmetry if necessary
   SymmetryRecalc();

   // since the sides may not be sorted or reduced, do so
   CListFixed lTemp, lSides;
   lTemp.Init (sizeof(DWORD), padwSide, dwNumSide);
   SortListAndRemoveDup (1, &lTemp, &lSides);

   // include symmetry
   lTemp.Init (sizeof(DWORD));
   MirrorSide ((DWORD*)lSides.Get(0), lSides.Num(), &lTemp);
   dwNumSide = lTemp.Num();
   padwSide = (DWORD*) lTemp.Get(0);

   // change over
   DWORD i;
   for (i = 0; i < dwNumSide; i++) {
      PCPMSide ps = SideGet(padwSide[i]);
      ps->m_dwSurfaceText = dwSurface;
   }

   m_fDirtyRender = TRUE;
   return TRUE;
}

/*****************************************************************************************
CPolyMesh::SurfaceRename - Rename all sides using one surface to another surface

inputs
   DWORD       dwOrig - Oringial surfwce number
   DWORD       dwNew - New surface number
*/
void CPolyMesh::SurfaceRename (DWORD dwOrig, DWORD dwNew)
{
   PCPMSide *pps = (PCPMSide*) m_lSide.Get(0);
   DWORD i;

   for (i = 0; i < m_lSide.Num(); i++)
      if (pps[i]->m_dwSurfaceText == dwOrig)
         pps[i]->m_dwSurfaceText = dwNew;

   m_fDirtyRender = TRUE;
}

/*****************************************************************************************
CPolyMesh::SurfaceFilterSides - Given a list of sides, this filters them down so only
those sides on the surface are kept.

input
   DWORD          dwNumSide - Number of sides
   DWORD          *padwSide - Pointer to an array of dwNumSide. If NULL then all sides used.
                              Must be sorted.
   DWORD          dwSurface - Surface to keep
   PCListFixed    plFilter - Initialized to sizeof(DWORD) and filled in. Sorted.
*/
void CPolyMesh::SurfaceFilterSides (DWORD dwNumSide, DWORD *padwSide, DWORD dwSurface,
                                    PCListFixed plFilter)
{
   plFilter->Init (sizeof(DWORD));
   DWORD i;
   PCPMSide *pps = (PCPMSide*)m_lSide.Get(0);
   for (i = 0; i < (padwSide ? dwNumSide : m_lSide.Num()); i++) {
      DWORD dwSide = (padwSide ? padwSide[i] : i);
      if (pps[dwSide]->m_dwSurfaceText == dwSurface)
         plFilter->Add (&dwSide);
   }
}

/*****************************************************************************************
CPolyMesh::SurfaceFilterVert - Filter the vertices so they are only kept if they're
attached to one of the given sides.

inputs
   DWORD          dwNumVert - Number of vertices
   DWORD          *padwVert - Pointer to an array of dwNumVert elements. Must be sorted.
   DWORD          dwNumSide - Number of sides
   DWORD          *padwSide - Pointer to an array of dwNumSide. If NULL then all sides used.
                              Must be sorted.
   PCListFixed    plFilter - Initialized to sizeof(DWORD) and filled in with vertcies. Sorted.
*/
void CPolyMesh::SurfaceFilterVert (DWORD dwNumVert, DWORD *padwVert,
                                   DWORD dwNumSide, DWORD *padwSide,
                                    PCListFixed plFilter)
{
   plFilter->Init (sizeof(DWORD));
   DWORD i, j;
   PCPMVert *ppv = (PCPMVert*)m_lVert.Get(0);
   for (i = 0; i < dwNumVert; i++) {
      PCPMVert pv = ppv[padwVert[i]];
      DWORD *padwSidePoly = pv->SideGet();
      for (j = 0; j < pv->m_wNumSide; j++)
         if (-1 != DWORDSearch (padwSidePoly[j], dwNumSide, padwSide)) {
            plFilter->Add (&padwVert[i]);
            break;   // found
         }
   }
}


/*****************************************************************************************
CPolyMesh::TextureMove - Moves a group of textures (defined by their vertices).
   NOTE: Unlike the other move functions this doesnt do mirroing because mirroring
   in textures depends a lot upon the users idea of what mirroring in, and cant
   be done well automatically.

inputs
   DWORD          dwNumVert - Number of vertices
   DWORD          *padwVert - Array of dwNumVert vertices. This must be sorted. If a vertex
                        does NOT appear in the final list of sides then it won't be moved
   DWORD          dwNumSide - Number of sides to begin with
   DWORD          *padwSide - Array of dwNumSide sides. This must be sorted. If NULL then
                        assume that start with a list of all sides.
   DWORD          dwSurface - Surface. If a side/vertex is not a member of this surface
                        then it won't be moved
   DWORD          dwType - Type of movement:
                        0 - For translation
                        1 - For rotation around .h
                        2 - For scaling in .h and .v
   PTEXTUREPOINT  pMove - Amount of movmenet
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::TextureMove (DWORD dwNumVert, DWORD *padwVert, DWORD dwNumSide, DWORD *padwSide,
                             DWORD dwSurface, DWORD dwType, PTEXTUREPOINT pMove)
{
   // reduce sides to those using surface
   CListFixed lSides;
   SurfaceFilterSides (dwNumSide, padwSide, dwSurface, &lSides);
   dwNumSide = lSides.Num();
   padwSide = (DWORD*)lSides.Get(0);
   if (!dwNumSide)
      return FALSE;  // should be something

   // reduce vertices to those on the sides
   CListFixed lVert;
   SurfaceFilterVert (dwNumVert, padwVert, dwNumSide, padwSide, &lVert);
   dwNumVert = lVert.Num();
   padwVert = (DWORD*) lVert.Get(0);
   if (!padwVert)
      return FALSE;  // should be something

   // calculate center of mass
   TEXTUREPOINT tpCOM;
   PTEXTUREPOINT ptp;
   DWORD i,j;
   tpCOM.h = tpCOM.v = 0;
   if ((dwType == 1) || (dwType == 2)) {
      DWORD dwCount = 0;
      for (i = 0; i < dwNumVert; i++) {
         PCPMVert pv = VertGet(padwVert[i]);
         DWORD *padwSidePoly = pv->SideGet();
         for (j = 0; j < pv->m_wNumSide; j++)
            if (-1 != DWORDSearch (padwSidePoly[j], dwNumSide, padwSide)) {
               // found side this is attached to
               ptp = pv->TextureGet (padwSidePoly[j]);
               tpCOM.h += ptp->h;
               tpCOM.v += ptp->v;
               dwCount++;
               break;
            }
      } // i

      if (dwCount) {
         tpCOM.h /= (fp)dwCount;
         tpCOM.v /= (fp)dwCount;
      }
   } // generate COM

   // figure out rotation matrix...
   fp afRot[2][2];
   if (dwType == 1)
      TextureMatrixRotate (afRot, pMove->h);

   // loop through all the points and modify
   PTEXTUREPOINT ptpMain;
   BOOL fDidMain;
   for (i = 0; i < dwNumVert; i++) {
      PCPMVert pv = VertGet(padwVert[i]);
      DWORD *padwSidePoly = pv->SideGet();

      ptpMain = pv->TextureGet(-1);
      fDidMain = FALSE;

      for (j = 0; j < pv->m_wNumSide; j++) {
         // if side is not on list of valid sides then ignore
         if (-1 == DWORDSearch (padwSidePoly[j], dwNumSide, padwSide))
            continue;

         ptp = pv->TextureGet(padwSidePoly[j]);
         if (ptp == ptpMain) {
            // modifying the main texture. If already modified it once then dont
            // do it again
            if (fDidMain)
               continue;

            // else, note that modified
            fDidMain = TRUE;
         }

         switch (dwType) {
         case 0:  // move
            ptp->h += pMove->h;
            ptp->v += pMove->v;
            break;
         case 1:  // rotate
            ptp->h -= tpCOM.h;
            ptp->v -= tpCOM.v;
            TextureMatrixMultiply2D (afRot, ptp);
            ptp->h += tpCOM.h;
            ptp->v += tpCOM.v;
            break;
         case 2:  // scale
            ptp->h -= tpCOM.h;
            ptp->v -= tpCOM.v;
            ptp->h *= pMove->h;
            ptp->v *= pMove->v;
            ptp->h += tpCOM.h;
            ptp->v += tpCOM.v;
            break;
         }
      } // j
   } // i

   m_fDirtyRender = TRUE;

   return TRUE;
}



/*****************************************************************************************
CPolyMesh::TextureDisconnect - Takes a list of sides and disconnects the border vertices
of the sides with other vertices. That way the set of sides can be moved in
one chunk.

inputs
   DWORD          dwNumSide - Number of sides to begin with
   DWORD          *padwSide - Array of dwNumSide sides. This must be sorted. If NULL then
                        assume that start with a list of all sides.
   DWORD          dwSurface - Surface. If a side/vertex is not a member of this surface
                        then it won't be moved
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::TextureDisconnect (DWORD dwNumSide, DWORD *padwSide, DWORD dwSurface)
{
   // reduce sides to those using surface
   CListFixed lSides;
   SurfaceFilterSides (dwNumSide, padwSide, dwSurface, &lSides);
   dwNumSide = lSides.Num();
   padwSide = (DWORD*)lSides.Get(0);
   if (!dwNumSide)
      return FALSE;  // should be something

   // generate a list of vertices from these
   CListFixed lVert;
   SidesToVert (padwSide, dwNumSide, &lVert);
   DWORD dwNumVert = lVert.Num();
   DWORD *padwVert = (DWORD*)lVert.Get(0);
   if (!dwNumVert)
      return FALSE;

   // loop though all the vertices and disconnecting them
   DWORD i, j;
   for (i = 0; i < dwNumVert; i++) {
      PCPMVert pv = VertGet(padwVert[i]);
      DWORD *padwSidePoly = pv->SideGet();
      
      PTEXTUREPOINT ptMain;
      ptMain = pv->TextureGet(-1);

      // see if all the points are in the list of approved sides
      for (j = 0; j < pv->m_wNumSide; j++)
         if (-1 == DWORDSearch (padwSidePoly[j], dwNumSide, padwSide))
            break;
      if (j >= pv->m_wNumSide)
         continue;   // all the sides are on the list of approved sides, so nothing to disconnect from

      // loop through again, looking for sides in the approved list
      for (j = 0; j < pv->m_wNumSide; j++) {
         // if its not in the list of sides we're interest in then dont disconnect
         if (-1 == DWORDSearch (padwSidePoly[j], dwNumSide, padwSide))
            continue;

         // disconnect
         if (pv->TextureGet(padwSidePoly[j]) == ptMain) {
            TEXTUREPOINT tp;
            tp = *ptMain;
            pv->TextureSet (padwSidePoly[j], &tp, TRUE);
            padwSidePoly = pv->SideGet(); // reload since may have changed
         }
      } // j
   } // i

   // dont need to redraw because will look the same from the outside
   // m_fDirtyRender = TRUE;
   return TRUE;
}


/*****************************************************************************************
CPolyMesh::TextureDisconnectVert - Takes a list of vertices and disconnects the border vertices
of the sides with other vertices. That way the vertcies can move individually

inputs
   DWORD          dwNumVert - Number of vertices
   DWORD          *padwVert - Array of dwNumVert vertices. This must be sorted. If a vertex
                        does NOT appear in the final list of sides then it won't be moved
   DWORD          dwNumSide - Number of sides to begin with
   DWORD          *padwSide - Array of dwNumSide sides. This must be sorted. If NULL then
                        assume that start with a list of all sides.
   DWORD          dwSurface - Surface. If a side/vertex is not a member of this surface
                        then it won't be moved
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::TextureDisconnectVert (DWORD dwNumVert, DWORD *padwVert, 
                                       DWORD dwNumSide, DWORD *padwSide, DWORD dwSurface)
{
   // reduce sides to those using surface
   CListFixed lSides;
   SurfaceFilterSides (dwNumSide, padwSide, dwSurface, &lSides);
   dwNumSide = lSides.Num();
   padwSide = (DWORD*)lSides.Get(0);
   if (!dwNumSide)
      return FALSE;  // should be something

   // reduce vertices to those on the sides
   CListFixed lVert;
   SurfaceFilterVert (dwNumVert, padwVert, dwNumSide, padwSide, &lVert);
   dwNumVert = lVert.Num();
   padwVert = (DWORD*) lVert.Get(0);
   if (!padwVert)
      return FALSE;  // should be something

   // loop though all the vertices and disconnecting them
   DWORD i, j;
   for (i = 0; i < dwNumVert; i++) {
      PCPMVert pv = VertGet(padwVert[i]);
      DWORD *padwSidePoly = pv->SideGet();
      
      PTEXTUREPOINT ptMain;
      ptMain = pv->TextureGet(-1);

      // loop through again, looking for sides in the approved list
      for (j = 0; j < pv->m_wNumSide; j++) {
         // if its not in the list of sides we're interest in then dont disconnect
         if (-1 == DWORDSearch (padwSidePoly[j], dwNumSide, padwSide))
            continue;

         // disconnect
         if (pv->TextureGet(padwSidePoly[j]) == ptMain) {
            TEXTUREPOINT tp;
            tp = *ptMain;
            pv->TextureSet (padwSidePoly[j], &tp, TRUE);
            padwSidePoly = pv->SideGet(); // reload since may have changed
         }
      } // j
   } // i

   // dont need to redraw because will look the same from the outside
   // m_fDirtyRender = TRUE;
   return TRUE;
}



/*****************************************************************************************
CPolyMesh::TextureCollapse - Collapses all the texture points of the vertices (assuming
they're part of the same surface) into one. This also gets rid of extra sidetext entries
that are no longer relevent.

   NOTE: Unlike the other move functions this doesnt do mirroing because mirroring
   in textures depends a lot upon the users idea of what mirroring in, and cant
   be done well automatically.

inputs
   DWORD          dwNumVert - Number of vertices
   DWORD          *padwVert - Array of dwNumVert vertices. This must be sorted. If a vertex
                        does NOT appear in the final list of sides then it won't be collapsed.
   DWORD          dwSurface - Surface. If a side/vertex is not a member of this surface
                        then it won't be moved
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::TextureCollapse (DWORD dwNumVert, DWORD *padwVert, DWORD dwSurface)
{
   // reduce sides to those using surface
   CListFixed lSides;
   SurfaceFilterSides (0, NULL, dwSurface, &lSides);
   DWORD dwNumSide = lSides.Num();
   DWORD *padwSide = (DWORD*)lSides.Get(0);
   if (!dwNumSide)
      return FALSE;  // should be something

   // reduce vertices to those on the sides
   CListFixed lVert;
   SurfaceFilterVert (dwNumVert, padwVert, dwNumSide, padwSide, &lVert);
   dwNumVert = lVert.Num();
   padwVert = (DWORD*) lVert.Get(0);
   if (!padwVert)
      return FALSE;  // should be something

   // calculate center of mass
   TEXTUREPOINT tpCOM;
   PTEXTUREPOINT ptp;
   DWORD i,j;
   tpCOM.h = tpCOM.v = 0;
   DWORD dwCount = 0;
   for (i = 0; i < dwNumVert; i++) {
      PCPMVert pv = VertGet(padwVert[i]);
      DWORD *padwSidePoly = pv->SideGet();
      for (j = 0; j < pv->m_wNumSide; j++)
         if (-1 != DWORDSearch (padwSidePoly[j], dwNumSide, padwSide)) {
            // found side this is attached to
            ptp = pv->TextureGet (padwSidePoly[j]);
            tpCOM.h += ptp->h;
            tpCOM.v += ptp->v;
            dwCount++;
            break;
         }
   } // i

   if (dwCount) {
      tpCOM.h /= (fp)dwCount;
      tpCOM.v /= (fp)dwCount;
   }

   // loop through all the points and modify
   for (i = 0; i < dwNumVert; i++) {
      PCPMVert pv = VertGet(padwVert[i]);
      DWORD *padwSidePoly = pv->SideGet();

      // see if all the sides associated with the vertex are on the list.
      // if so, then just remove any extra sidetext elems because they'll
      // all be collapsed eventually
      for (j = 0; j < pv->m_wNumSide; j++)
         if (-1 == DWORDSearch (padwSidePoly[j], dwNumSide, padwSide))
            break;
      if (j >= pv->m_wNumSide)
         pv->SideTextClear(); // ok to clear all
      padwSidePoly = pv->SideGet();

      for (j = 0; j < pv->m_wNumSide; j++) {
         // if side is not on list of valid sides then ignore
         if (-1 == DWORDSearch (padwSidePoly[j], dwNumSide, padwSide))
            continue;

         ptp = pv->TextureGet(padwSidePoly[j]);

         // may end up resetting same memory a few times, but no big deal
         // since setting all to the same value anyway
         ptp->h = tpCOM.h;
         ptp->v = tpCOM.v;
      } // j
   } // i

   m_fDirtyRender = TRUE;

   return TRUE;
}


/*****************************************************************************************
CPolyMesh::TextureMirror - Mirrors the texture from one side of the object to the other.

inputs
   DWORD          dwSurface - Surface to mirror
   DWORD          dwDim - Dimension, 0 for x, etc.
   BOOL           fFromPos - If TRUE then take the original data from the positive
                  side, FALSE take from negative
   PTEXTUREPOINT  ptAxis - h and v axis for mirroring around
   BOOL           fMirrorH - if TRUE then mirros around h axis, else just copies
   BOOL           fMirrorV - if TRUE then mirrors around v axis
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::TextureMirror (DWORD dwSurface, DWORD dwDim, BOOL fFromPos, PTEXTUREPOINT ptAxis,
                               BOOL fMirrorH, BOOL fMirrorV)
{
   SymmetryRecalc();

   // figure out the list of sides to mirror
   CListFixed lSideFrom, lSideTo;
   DWORD i, j, k;
   PCPMSide *pps = (PCPMSide*)m_lSide.Get(0);
   lSideFrom.Init (sizeof(DWORD));
   lSideTo.Init (sizeof(DWORD));
   DWORD dwWant = (1 << dwDim);
   for (i = 0; i < m_lSide.Num(); i++) {
      PCPMSide ps = pps[i];

      // ignore if it's not the right surface
      if (ps->m_dwSurfaceText != dwSurface)
         continue;

      // find the min and max
      fp fMin, fMax;
      DWORD *padwVert = ps->VertGet();
      fMin = fMax = 0;
      for (j = 0; j < ps->m_wNumVert; j++) {
         PCPMVert pv = VertGet(padwVert[j]);
         if (j) {
            fMin = min(fMin, pv->m_pLoc.p[dwDim]);
            fMax = max(fMax, pv->m_pLoc.p[dwDim]);
         }
         else {
            fMin = fMax = pv->m_pLoc.p[dwDim];
         }
      } // j

      // if taking from negative then flip meaning
      if (!fFromPos) {
         fp f = fMin;
         fMin = -fMax;
         fMax = -f;
      }

      if (fMax <= 0)
         continue;   // from the side that ignoring

      if (fMin >= 0) {
         // take entire side

         // find the mirror
         PPMMIRROR pm = ps->MirrorGet();
         for (j = 0; j < ps->m_wNumMirror; j++)
            if (pm[j].dwType == dwWant)
               break;
         if (j >= ps->m_wNumMirror)
            continue;   // cant find a mirror

         lSideFrom.Add (&i);
         lSideTo.Add (&(pm[j].dwObject));
      }
      else {
         // take partial
         lSideFrom.Add (&i);
         lSideTo.Add (&i);
      }
   } // i
   DWORD dwNumSideFrom = lSideFrom.Num();
   DWORD *padwSideFrom = (DWORD*)lSideFrom.Get(0);
   DWORD *padwSideTo = (DWORD*)lSideTo.Get(0);
   if (!dwNumSideFrom)
      return FALSE;

   // the from list is already sorted, but create a sideto that's sorted
   CListFixed lSideToSort, lTemp;
   lTemp.Init (sizeof(DWORD), padwSideTo, dwNumSideFrom);
   SortListAndRemoveDup (1, &lTemp, &lSideToSort);
   DWORD dwNumSideToSort = lSideToSort.Num();
   DWORD *padwSideToSort = (DWORD*)lSideToSort.Get(0);

   // create a list of vertices on the new side
   CListFixed lVertFrom, lVertTo;
   PCPMVert *ppv = (PCPMVert*)m_lVert.Get(0);
   SidesToVert (padwSideFrom, dwNumSideFrom, &lVertFrom);
   DWORD dwNumVertFrom = lVertFrom.Num();
   DWORD *padwVertFrom = (DWORD*)lVertFrom.Get(0);
   lVertTo.Init (sizeof(DWORD), padwVertFrom, dwNumVertFrom);
   DWORD *padwVertTo = (DWORD*)lVertTo.Get(0);
   for (i = 0; i < dwNumVertFrom; i++) {
      PCPMVert pv = ppv[padwVertFrom[i]];

      // look through the mirrors
      PPMMIRROR pm = pv->MirrorGet();
      for (j = 0; j < pv->m_wNumMirror; j++)
         if (pm[j].dwType == dwWant)
            break;
      if (j < pv->m_wNumMirror)
         padwVertTo[i] = pm[j].dwObject;
      // else, if didnt find use current as mirror
   } // i

   // loop through all the new sides and set their surface
   for (i = 0; i < dwNumSideFrom; i++)
      pps[padwSideTo[i]]->m_dwSurfaceText = dwSurface;

   // loop through all the vertices that add
   CListFixed lSideText;
   lSideText.Init (sizeof(SIDETEXT));
   SIDETEXT st, *pst;
   for (i = 0; i < dwNumVertFrom; i++) {
      PCPMVert pvFrom = ppv[padwVertFrom[i]];
      PCPMVert pvTo = ppv[padwVertTo[i]];

      // if the vertex is on the wrong side of the mirror then ignore. This can
      // happen when have a polygon that straddles the mirror
      if (fFromPos && (pvFrom->m_pLoc.p[dwDim] < 0))
         continue;
      if (!fFromPos && (pvFrom->m_pLoc.p[dwDim] > 0))
         continue;

      // note: If vertex is equal to its mirror do care

      // loop through all the vertices in the from point. Go through all its sides
      // and find those which are in the sidefrom list. If they are, then mirror it
      // over
      DWORD *padwSidePoly = pvFrom->SideGet();
      lSideText.Clear();
      for (j = 0; j < pvFrom->m_wNumSide; j++) {
         DWORD dwSide = padwSidePoly[j];
         DWORD dwFind;

         // see if this is in the from list
         dwFind = DWORDSearch (dwSide, dwNumSideFrom, padwSideFrom);
         if (-1 == dwFind)
            continue;   // not one that care about

         // else, its from the original list, so find the mirror
         DWORD dwMirror = padwSideTo[dwFind];

         // if the side is its mirror, and the points are the same then do nothing
         if ((dwMirror == dwSide) && (padwVertFrom[i] == padwVertTo[i]))
            continue;

         // else, need to mirror this
         st.dwSide = dwMirror;
         st.tp = *(pvFrom->TextureGet (dwSide));

         // mirror?
         if (fMirrorH)
            st.tp.h = ptAxis->h - (st.tp.h - ptAxis->h);
         if (fMirrorV)
            st.tp.v = ptAxis->v - (st.tp.v - ptAxis->v);

         lSideText.Add (&st);
      } // j

      // needed to create list of mods so that if was modifying itself (pvFrom == pvTo) wouldnt
      // muck of pointers
      pst = (PSIDETEXT) lSideText.Get(0);
      padwSidePoly = pvTo->SideGet();
      for (j = 0; j < pvTo->m_wNumSide; j++) {
         DWORD dwSide = padwSidePoly[j];

         // find this side in the list of textures to write
         for (k = 0; k < lSideText.Num(); k++)
            if (pst[k].dwSide == dwSide)
               break;
         if (k >= lSideText.Num())
            continue;   // nothing to change

         // set the new texture
         pvTo->TextureSet (dwSide, &pst[k].tp, TRUE);
         padwSidePoly = pvTo->SideGet();  // in case caused a ESCREALLOC
      } // j
   } // i

   // go through all the new points and remove any redundent SIDETEXT
   for (i = 0; i < dwNumVertFrom; i++) {
      PCPMVert pvTo = ppv[padwVertTo[i]];

      // find out if all the sides are on the list of sides we started with or created
      DWORD *padwSidePoly = pvTo->SideGet();
      for (j = 0; j < pvTo->m_wNumSide; j++)
         if ((-1 == DWORDSearch (padwSidePoly[j], dwNumSideFrom, padwSideFrom)) &&
            (-1 == DWORDSearch (padwSidePoly[j], dwNumSideToSort, padwSideToSort)))
            break;
      if (j < pvTo->m_wNumSide)
         continue;   // cant do anything here because attached to an external side

      // else, all the sides are attached within the mirror. Go through all the
      // sides and see if they all have the same texture...
      TEXTUREPOINT tp;
      PTEXTUREPOINT ptp;
      tp = *(pvTo->TextureGet(padwSidePoly[0]));
      for (j = 1; j < pvTo->m_wNumSide; j++) {
         ptp = pvTo->TextureGet (padwSidePoly[j]);
         if (!AreClose (&tp, ptp))
            break;
      }
      if (j < pvTo->m_wNumSide)
         continue;   // cant do anything because one of the points contains a different side

      // if get here, then all the sides are the same, and all the sidetext are the
      // same, so clear it out and set it
      pvTo->SideTextClear();
      pvTo->TextureSet (-1, &tp);
   } // i

   // done
   m_fDirtyRender = TRUE;
   return TRUE;
}



/*****************************************************************************************
CPolyMesh::PolyRenderToMesh - This takes the output of a polygon render call and
uses it to add vertices and sides to the polygon mesh.

inputs
   PPOLYRENDERINFO         pInfo - Information passed into PolyRender. This will
                           be modifies in the process... since normals will be rotated,etc.
                           This must contain valid normals, colors, and textures.
   PCMatrix                pmObjectToMesh - Matrix that converts from the object coords
                           into the mesh's coords
   PCObjectTemplate        pTemplate - Needed so the surfaces can be added to the template
returns
   BOOL - TRUE if success
*/
BOOL CPolyMesh::PolyRenderToMesh (PPOLYRENDERINFO pInfo, PCMatrix pmObjectToMesh,
                                  PCObjectTemplate pTemplate)
{
   // first off, set some dirty flags
   m_fDirtyEdge = TRUE;
   m_fDirtyMorphCOM = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyRender = TRUE;
   m_fDirtyScale = TRUE;
   m_fDirtySymmetry = TRUE;
   m_dwSymmetry = 0; // assume no symmetry

   // keep track of vertices that appear at start of this object so dont
   // get into really large loop. Only combine over current sub-object
   DWORD dwVertAtStart = m_lVert.Num();

   // track of last surface
   CObjectSurface Surf;
   DWORD dwLastSurfInPoly = -1;
   DWORD dwLastSurfInMesh = -1;

   // neet to convert all the points and normals
   DWORD i, j;
   CMatrix mInv;
   pmObjectToMesh->Invert (&mInv);  // only need 3x3 invert
   mInv.Transpose ();
   PCPoint p;
   for (i = 0, p = pInfo->paNormals; i < pInfo->dwNumNormals; i++, p++) {
      p->p[3] = 1;
      p->MultiplyLeft (&mInv);
      if (!pInfo->fAlreadyNormalized)
         p->Normalize();
   };
   for (i = 0, p = pInfo->paPoints; i < pInfo->dwNumPoints; i++, p++) {
      p->p[3] = 1;
      p->MultiplyLeft (pmObjectToMesh);
   }

   // loop through all the polygons
   PPOLYDESCRIPT pCur;
   PRENDERSURFACE pSurf;
   DWORD dwCurPoly;
   BOOL fNewVert;
   for (pCur = pInfo->paPolygons, dwCurPoly = 0; dwCurPoly < pInfo->dwNumPolygons;
      pCur = (PPOLYDESCRIPT) ((DWORD*)(pCur+1) + pCur->wNumVertices), dwCurPoly++) {

      // vertex
      DWORD *padwVert = (DWORD*)(pCur+1);

      // figure out the color... since polymesh cant have different colors per vertex
      // just pick one
      COLORREF cr;
      cr = pInfo->paColors[pInfo->paVertices[padwVert[0]].dwColor];

      // get the surface
      pSurf = &pInfo->paSurfaces[pCur->dwSurface];

      if (dwLastSurfInPoly != pCur->dwSurface) {
         memcpy (Surf.m_afTextureMatrix, pSurf->afTextureMatrix, sizeof(Surf.m_afTextureMatrix));
         memcpy (&Surf.m_mTextureMatrix, pSurf->abTextureMatrix, sizeof(Surf.m_mTextureMatrix));
         Surf.m_cColor = cr;
         Surf.m_dwID = 0;
         Surf.m_fUseTextureMap = pSurf->fUseTextureMap;
         Surf.m_gTextureCode = pSurf->gTextureCode;
         Surf.m_gTextureSub = pSurf->gTextureSub;
         memcpy (&Surf.m_Material, &pSurf->Material, sizeof(Surf.m_Material));
         wcscpy (Surf.m_szScheme, pSurf->szScheme);
         memcpy (&Surf.m_TextureMods, &pSurf->TextureMods, sizeof(Surf.m_TextureMods));

         dwLastSurfInPoly = pCur->dwSurface;
         dwLastSurfInMesh = -1;
      }
      else {
         // same surface as the last polygon. As long as the colors havent changed then
         // there are no dramas
         if ((dwLastSurfInMesh == -1) || (Surf.m_cColor != cr)) {
            Surf.m_cColor = cr;
            dwLastSurfInMesh = -1;  // so recheck
         }
      }
      
      // if dwLastSurfInMesh is -1 then need to see if have a match in the template
      if (dwLastSurfInMesh == -1) {
         for (i = 0; i < pTemplate->ObjectSurfaceNumIndex(); i++) {
            Surf.m_dwID = pTemplate->ObjectSurfaceGetIndex (i);
            PCObjectSurface pComp = pTemplate->ObjectSurfaceFind (Surf.m_dwID);

            if (pComp->AreTheSame (&Surf)) {
               // match
               dwLastSurfInMesh = Surf.m_dwID;
               break;
            }
         } // i

         // if still -1 then need to add
         if (dwLastSurfInMesh == -1) {
            for (i = 0; ; i++)
               if (!pTemplate->ObjectSurfaceFind (i))
                  break;
            Surf.m_dwID = dwLastSurfInMesh = i;
            pTemplate->ObjectSurfaceAdd (&Surf);

            // also, since all texture matrices already have applied
            // scale, need to set flag
            SurfaceInMetersSet (Surf.m_dwID, FALSE);
         } // still -1
      } // if dwLastSurfInMesh==-1


      // if can backface the original then can backface this
      m_fCanBackface &= pCur->fCanBackfaceCull;

      // create a new side
      PCPMSide ps = new CPMSide;
      if (!ps)
         return FALSE;
      ps->m_dwSurfaceText = dwLastSurfInMesh;
      DWORD dwSide = m_lSide.Num();
      m_lSide.Add (&ps);

      // loop through all the vertices
      for (i = 0; i < pCur->wNumVertices; i++) {
         PVERTEX pvert = &pInfo->paVertices[padwVert[i]];

         // get the point and normal
         PCPoint pPoint, pNorm;
         pPoint = &pInfo->paPoints[pvert->dwPoint];
         pNorm = &pInfo->paNormals[pvert->dwNormal];

         // loop through all the points, looking for a match with the point and
         // the normal
         PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
         PCPMVert pv;
         fNewVert = FALSE;
         for (j = dwVertAtStart; j < m_lVert.Num(); j++) {
            pv = ppv[j];
            if (pv->m_pLoc.AreClose (pPoint) && pv->m_pNorm.AreClose (pNorm))
               break;   // found match
         } // j

         // if didn't find match then add
         if (j >= m_lVert.Num()) {
            pv = new CPMVert;
            if (!pv)
               return FALSE;
            pv->m_pLoc.Copy (pPoint);
            pv->m_pNorm.Copy (pNorm);  // using this so can identify creases
            
            j = m_lVert.Num();
            m_lVert.Add (&pv);
            fNewVert = TRUE;
         } // if adding

         // add the vertex to the side
         ps->VertAdd (&j);

         // add the side to the vertex
         pv->SideAdd (&dwSide);

         // get the texture
         if (pvert->dwTexture < pInfo->dwNumTextures)
            pv->TextureSet (fNewVert ? -1 : dwSide,
               (PTEXTUREPOINT)(&pInfo->paTextures[pvert->dwTexture].hv) );
      } // i, vertices
   } // pCur

   // done
   return TRUE;
}


/*****************************************************************************************
CPolyMesh::BoneGet - Looks through the current pWorld and finds the bone with the bone
object's GUID. Returns a pointer to the bone, or NULL if there isn't a bone specified
for the object or if the specified bone can't be found.

inputs
   PCWorldSocket           pWorld - World to use
returns
   PCObjectBone - Pointer to a bone, or NULL if none or cant find
*/
PCObjectBone CPolyMesh::BoneGet (PCWorldSocket pWorld)
{
   if (IsEqualGUID(m_gBone, GUID_NULL))
      return NULL;

   // find it
   DWORD dwFind = pWorld->ObjectFind (&m_gBone);
   if (dwFind == -1)
      return NULL;
   PCObjectSocket pos = pWorld->ObjectGet(dwFind);
   if (!pos)
      return NULL;

   OSMBONE ob;
   memset (&ob, 0, sizeof(ob));
   if (!pos->Message (OSM_BONE, &ob))
      return NULL;
   return ob.pb;
}

/*****************************************************************************************
CPolyMesh::BoneSyncNames - Syncrhonize the names stored in the m_lPMBONEINFO with
the names in the bones. This also looks for new bones that have appeared, or bones
which have disappeared, and does a remap of all the vertices.

inputs
   PCObjectBone         pBone - Bone that attached to (from bone get)
   PCObjectTemplate       pObject - Use this to make a worldabouttochange call.
returns
   BOOL - TRUE if changed, FALSE if not
*/
BOOL CPolyMesh::BoneSyncNames (PCObjectBone pBone, PCObjectTemplate pObject)
{
   // get the list of bones
   PCBone *ppb = (PCBone*) pBone->m_lBoneList.Get(0);
   PPMBONEINFO pbi = (PPMBONEINFO)m_lPMBONEINFO.Get(0);

   // if they're exactly the same then trivial accept
   DWORD i, j;
   if (m_lPMBONEINFO.Num() == pBone->m_lBoneList.Num()) {
      for (i = 0; i < m_lPMBONEINFO.Num(); i++)
         if ((ppb[i] != pbi[i].pBone) || _wcsicmp(ppb[i]->m_szName, pbi[i].szName))
            break;
      if (i >= m_lPMBONEINFO.Num())
         return FALSE;  // no changes
   } // same number

   // else, something has changed, so first figure out remap
   CMem memRemap;
   if (!memRemap.Required (2 * sizeof(DWORD) * m_lPMBONEINFO.Num()))
      return FALSE;
   DWORD *padwFrom = (DWORD*)memRemap.p;
   DWORD *padwTo = padwFrom + m_lPMBONEINFO.Num();
   for (i = 0; i < m_lPMBONEINFO.Num(); i++) {
      padwFrom[i] = i;
      padwTo[i] = -1;
   }

   // go through each one and find remap
   for (i = 0; i < m_lPMBONEINFO.Num(); i++) {
      if (pbi[i].pBone) {
         for (j = 0; j < pBone->m_lBoneList.Num(); j++)
            if (ppb[j] == pbi[i].pBone) {
               padwTo[i] = j;
               break;
            }
         if (j < pBone->m_lBoneList.Num())
            continue;   // BUGFIX - Was break
      } // if already know a pointer to a bone

      // else, dont have a pointer, so look for name
      // only know the name...
      for (j = 0; j < pBone->m_lBoneList.Num(); j++)
         if (!_wcsicmp(ppb[j]->m_szName, pbi[i].szName)) {
            pbi[i].pBone = ppb[j];
            padwTo[i] = j;
            break;
         }
   } // i

   if (pObject)
      pObject->m_pWorld->ObjectAboutToChange (pObject);

   // will need to remap all vertices, unless really isnt a remap
   for (i = 0; i < m_lPMBONEINFO.Num(); i++)
      if (padwFrom[i] != padwTo[i])
         break;
   if (i < m_lPMBONEINFO.Num()) {
      // need to remap
      PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
      for (i = 0; i < m_lVert.Num(); i++)
         ppv[i]->BoneWeightRename (m_lPMBONEINFO.Num(), padwFrom, padwTo);
   } // need to remap

   // get new values
   CListFixed lOld;
   lOld.Init (sizeof(PMBONEINFO), m_lPMBONEINFO.Get(0), m_lPMBONEINFO.Num());
   m_lPMBONEINFO.Clear();
   m_fBoneMirrorValid = FALSE;   // mirroring no longer valid
   m_dwBoneDisplay = -1;
   PMBONEINFO bi;
   memset (&bi, 0, sizeof(bi));
   m_lPMBONEINFO.Required (pBone->m_lBoneList.Num());
   for (i = 0; i < pBone->m_lBoneList.Num(); i++) {
      wcscpy (bi.szName, ppb[i]->m_szName);
      bi.pBone = ppb[i];
      bi.pBoneLastRender.Copy (&ppb[i]->m_pMotionCur);
      m_lPMBONEINFO.Add (&bi);
   }

   // transfer over the old morphs
   PPMBONEINFO pOld = (PPMBONEINFO) lOld.Get(0);
   PPMBONEINFO pNew = (PPMBONEINFO) m_lPMBONEINFO.Get(0);
   for (i = 0; i < lOld.Num(); i++) {
      // if deleted ignore
      if (padwTo[i] == -1)
         continue;
      PPMBONEINFO pTo = pNew + padwTo[i];

      // transfer
      for (j = 0; j < NUMBONEMORPH; j++)
         if (pOld[i].aBoneMorph[j].szName[0])
            pTo->aBoneMorph[j] = pOld[i].aBoneMorph[j];
   }

   // dirty flags
   m_fDirtyRender = TRUE;
   m_fDirtyNorm = TRUE;
   m_fDirtyMorphCOM = TRUE;

   if (pObject)
      pObject->m_pWorld->ObjectChanged (pObject);
   return TRUE;
}


/*****************************************************************************************
CPolyMesh::BoneClear - Clears out the current bone and forgets all the boneweights.

NOTE: Does NOT call WorldAboutToChange()
*/
void CPolyMesh::BoneClear (void)
{
   // first thing, clear out all the bone weights
   DWORD i;
   PCPMVert *ppv = (PCPMVert*) m_lVert.Get(0);
   for (i = 0; i < m_lVert.Num(); i++)
      ppv[i]->BoneWeightClear ();

   m_lPMBONEINFO.Clear();
   memset (&m_gBone, 0 ,sizeof(m_gBone));
}

/*****************************************************************************************
CPolyMesh::BoneSyncNamesOrFindOne - Called when switching to bone editing mode.
This call sees if the bone exists. If no bone is assigned, or it can no longer exists
then one is found. If one exists, the sames are synced just in case the bone
has changed.

inputs
   PCObjectTemplate       pObject - Use this to make a worldabouttochange call.
returns
   none
*/
void CPolyMesh::BoneSyncNamesOrFindOne (PCObjectTemplate pObject)
{
   PCObjectBone pBone = BoneGet(pObject->m_pWorld);

   if (pBone) {
      // found bone, so make sure it's synced
      BoneSyncNames (pBone, pObject);
      return;
   }

   // else, didn't find bone....

   pObject->m_pWorld->ObjectAboutToChange (pObject);

   // first thing, clear out all the bone weights
   DWORD i;
   BoneClear();

   // loop through all the objects in the world trying to find one
   PCWorldSocket pWorld = pObject->m_pWorld;
   OSMBONE ob;
   PCObjectSocket pos;
   for (i = 0; i < pWorld->ObjectNum(); i++) {
      pos = pWorld->ObjectGet(i);
      if (!pos)
         continue;

      ob.pb = NULL;
      if (!pos->Message (OSM_BONE, &ob))
         continue;
      if (ob.pb)
         break;
   } // i

   // if didn't find any bone at all then set to NULL
   if (i >= pWorld->ObjectNum()) {
      pObject->m_pWorld->ObjectChanged (pObject);
      return;
   }

   // found it...
   pos->GUIDGet (&m_gBone);
   pos->ObjectMatrixGet (&m_mBoneLocLastRender);
   pObject->m_pWorld->ObjectChanged (pObject);

   // sync up
   BoneSyncNames (ob.pb, pObject);

   // fill in the weights with auomatic weighting
   pBone = ob.pb;
   if (pBone && m_lPMBONEINFO.Num()) {
      CMatrix mMeshToWorld;
      pObject->ObjectMatrixGet (&mMeshToWorld);
      // BUGFIX - Pass TRUE into ObjEditBoneSetup because if dont, then if move
      // the bone in the editor won't realize that has moved and wont recalc properly
      // BUGFIX - Changed back to FALSE (no force) since fixed problems in bone
      if (!pBone->ObjEditBoneSetup(FALSE)) // no force change
         return;
      pBone->ObjEditBonesTestObject (pObject->m_pWorld->ObjectFind (&pObject->m_gGUID));

      pObject->m_pWorld->ObjectAboutToChange (pObject);
      PCPMVert *ppv = (PCPMVert*)m_lVert.Get(0);
      BONEWEIGHT bw;
      CListFixed lBONEWEIGHT;
      lBONEWEIGHT.Init (sizeof(BONEWEIGHT));
      memset (&bw, 0, sizeof(bw));
      PPMBONEINFO ppbi = (PPMBONEINFO) m_lPMBONEINFO.Get(0);
      for (i = 0; i < m_lVert.Num(); i++) {
         // get point and convert to world space
         CPoint pLoc, pNew;
         pLoc.Copy (&ppv[i]->m_pLoc);
         pLoc.p[3] = 1;
         pLoc.MultiplyLeft (&mMeshToWorld);

         pBone->ObjEditBonePointConvert (&pLoc, NULL, &pNew, NULL);

         // dont really care about new point, but care about strength...

         // find the strength
         DWORD dwNum = pBone->m_plBoneAffect->Num();
         POEBONE *ppb = (POEBONE*) pBone->m_plBoneAffect->Get(0);
         POEBW pbw = (POEBW) pBone->m_plOEBW->Get(0);
         DWORD j, k;
         lBONEWEIGHT.Clear();
         fp fSum;
         fSum = 0;
         for (j = 0; j < dwNum; j++) {
            if (!pbw[j].fWeight)
               continue;
            
            // find the match of this bone to our list
            for (k = 0; k < m_lPMBONEINFO.Num(); k++)
               if (ppbi[k].pBone == ppb[j]->pBone)
                  break;
            if (k >= m_lPMBONEINFO.Num())
               continue;   // not found, shouldnt happen

            // else use
            bw.dwBone = k;
            bw.fWeight = pbw[j].fWeight;
            fSum += pbw[j].fWeight;
            lBONEWEIGHT.Add (&bw);
         } // j

         PBONEWEIGHT pbwt = (PBONEWEIGHT) lBONEWEIGHT.Get(0);
         if (fSum) for (j = 0; j < lBONEWEIGHT.Num(); j++)
            pbwt[j].fWeight /= fSum;

         if (lBONEWEIGHT.Num())
            ppv[i]->BoneWeightAdd (pbwt, lBONEWEIGHT.Num());
      }
      pObject->m_pWorld->ObjectChanged (pObject);
   }
}


/*****************************************************************************************
CPolyMesh::BoneMirrorCalc - Calculates the bone mirroring if it isn't already valid.

inputs
   PCObjectBone         pBone - Bone
*/
void CPolyMesh::BoneMirrorCalc (PCObjectBone pBone)
{
   if (m_fBoneMirrorValid || !pBone || !m_lPMBONEINFO.Num())
      return;
   m_fBoneMirrorValid = TRUE;

   // pointers
   PCBone *ppb = (PCBone*) pBone->m_lBoneList.Get(0);
   PPMBONEINFO pbi = (PPMBONEINFO) m_lPMBONEINFO.Get(0);

   // clear the current mirror and fill in
   DWORD i, j;
   CListFixed lMirrorBone, lMirrorFlag;
   lMirrorBone.Init (sizeof(PCBone));
   lMirrorFlag.Init (sizeof(DWORD));
   for (i = 0; i < m_lPMBONEINFO.Num(); i++) {
      memset (pbi[i].apMirror, 0, sizeof(pbi[i].apMirror));

      // make sure this bone still exists
      for (j = 0; j < pBone->m_lBoneList.Num(); j++)
         if (pbi[i].pBone == ppb[j])
            break;
      if (j >= pBone->m_lBoneList.Num())
         continue;   // doesn't exist

      // look for mirrors
      lMirrorBone.Clear();
      lMirrorFlag.Clear();
      pBone->FindMirrors (pbi[i].pBone, &lMirrorBone, &lMirrorFlag);

      PCBone *pbMirror = (PCBone*) lMirrorBone.Get(0);
      DWORD *padwFlag = (DWORD*) lMirrorFlag.Get(0);
      for (j = 0; j < lMirrorBone.Num(); j++) {
         if ((padwFlag[j] == 0) || (padwFlag[j] > 7))
            continue;

         pbi[i].apMirror[padwFlag[j]-1] = pbMirror[j];
      } // j
   } // i

}



/*****************************************************************************************
CPolyMesh::BoneDisplaySet - Changes which bone is to be colored when the special
render mode is set to painting bones.

inputs
   DWORD          dwBone - Bone number, from 0..m_lPMBONEINFO.Num()-1. If it's
                  out of range then will display all bones as colored
*/
void CPolyMesh::BoneDisplaySet (DWORD dwBone)
{
   if (dwBone >= m_lPMBONEINFO.Num())
      dwBone = -1;
   if (dwBone != m_dwBoneDisplay) {
      m_dwBoneDisplay = dwBone;
      m_fDirtyRender = TRUE;
   }
}

/*****************************************************************************************
CPolyMesh::BoneDisplayGet - Returns value for bone display. (See BoneDisplaySet()), or
-1 if all bones colored.
*/
DWORD CPolyMesh::BoneDisplayGet (void)
{
   if (m_dwBoneDisplay < m_lPMBONEINFO.Num())
      return m_dwBoneDisplay;
   else
      return -1;
}



/**********************************************************************************
CPolyMesh::CalcBoneColors - This is called if dwSpecial is set to
ORSPECIAL_POLYMESH_EDITBONE. If so, then the m_pLocSubdivide will be filled with
the RGB values to be displayed.

inputs
   PCPolyMesh     pUse - Polymesh being modified, just cloned from the current polygon
                  mesh so there's a 1:1 corresponence of the vertices
returns
   none
*/
void CPolyMesh::CalcBoneColors (PCPolyMesh pUse)
{
   fp fLen;
   DWORD i, j;
   PCPMVert *ppo = (PCPMVert*) m_lVert.Get(0);
   PCPMVert *ppn = (PCPMVert*) pUse->m_lVert.Get(0);
   BOOL fUseAll = (m_dwBoneDisplay >= m_lPMBONEINFO.Num());
   COLORREF cr;
   WORD aw[3];
   cr = MapColorPicker (m_dwBoneDisplay); // just in case not using all
   Gamma (cr, aw);

   for (i = 0; i < m_lVert.Num(); i++) {
      PCPMVert pvo = ppo[i];
      PCPMVert pvn = ppn[i];

      // zero out subdivide while here
      pvn->m_pLocSubdivide.Zero();

      PBONEWEIGHT pbw = pvo->BoneWeightGet();
      fLen = 0;
      for (j = 0; j < pvo->m_wNumBoneWeight; j++) {
         // if only looking at a specific deform...
         if (!fUseAll && (pbw[j].dwBone != m_dwBoneDisplay))
            continue;

         // calc the color
         if (fUseAll) {
            cr = MapColorPicker (pbw[j].dwBone);
            Gamma (cr, aw);
         }

         // include the color
         pvn->m_pLocSubdivide.p[0] += (fp)aw[0] * pbw[j].fWeight;
         pvn->m_pLocSubdivide.p[1] += (fp)aw[1] * pbw[j].fWeight;
         pvn->m_pLocSubdivide.p[2] += (fp)aw[2] * pbw[j].fWeight;

         fLen += pbw[j].fWeight;
      }

      // else, scale
      if ((fLen > CLOSE) && fUseAll)
         pvn->m_pLocSubdivide.Scale (1.0 / fLen);
   } // i
}


/**********************************************************************************
CPolyMesh::BoneEnum - Returns a pointer to the list of bones that this mesh use.
Do NOT change the data. This data is only valid until the next bone call.

inputs
   DWORD          *pdwNum - Filled with the number of bones in th elist
returns
   PPMBONEINFO - List
*/
PPMBONEINFO CPolyMesh::BoneEnum (DWORD *pdwNum)
{
   *pdwNum = m_lPMBONEINFO.Num();
   return (PPMBONEINFO) m_lPMBONEINFO.Get(0);
}

/**********************************************************************************
CPolyMesh::BoneSeeIfRenderDirty - This looks through the bone. If anything has
changed since last time it sets dirty render to TRUE.

inputs
   PCWorldSocket           pWorld - Wolrd to look in
   BOOL                    *pfHasBoneMorph - Set to TRUE if the bone has a bone morph
                              AND the bone values have changed. If this is the case
                              will need to re-evaluate the attributes.
returns
   PCObjectBone - Bone, to save time later.
*/
PCObjectBone CPolyMesh::BoneSeeIfRenderDirty (PCWorldSocket pWorld, BOOL *pfHasBoneMorph)
{
   PCObjectBone pBone = BoneGet (pWorld);
   *pfHasBoneMorph = FALSE;
   if (!pBone)
      return NULL;  // no bone so ignore

   // get the list of bones
   PCBone *ppb = (PCBone*) pBone->m_lBoneList.Get(0);
   PPMBONEINFO pbi = (PPMBONEINFO)m_lPMBONEINFO.Get(0);
   DWORD dwNumBone = pBone->m_lBoneList.Num();
   DWORD dwNumInfo = m_lPMBONEINFO.Num();

   // go through each one and see if changed
   DWORD i,j;
   for (i = 0; i < dwNumInfo; i++) {
      if (pbi[i].pBone && (i < dwNumBone) && (pbi[i].pBone == ppb[i])) {
         // matches
         goto compare;
      }
      else if (pbi[i].pBone) {
         for (j = 0; j < dwNumBone; j++)
            if (ppb[j] == pbi[i].pBone) {
               pbi[i].pBone = ppb[j];
               break;
            }
         if (j < dwNumBone)
            goto compare;
      } // if already know a pointer to a bone

      // else, dont have a pointer, so look for name
      // only know the name...
      for (j = 0; j < dwNumBone; j++)
         if (!_wcsicmp(ppb[j]->m_szName, pbi[i].szName)) {
            pbi[i].pBone = ppb[j];
            m_fDirtyRender = TRUE;  // automatically mark this as dirty since just added
            m_fDirtyMorphCOM = TRUE;   // if bones move then morph COM dirty
            break;
         }
      if (j >= dwNumBone)
         break;   // cant find the bone at all, so assume hasn't changed

compare:
      // else, if pbi[i].pBone is the bone
      if (!pbi[i].pBone->m_pMotionCur.AreClose (&pbi[i].pBoneLastRender)) {
         m_fDirtyRender = TRUE;  // have changed so dirty
         m_fDirtyMorphCOM = TRUE;   // if bones move then morph COM dirty
         pbi[i].pBoneLastRender.Copy (&pbi[i].pBone->m_pMotionCur);

         // see if need to set bone morph flag
         if (!(*pfHasBoneMorph))
            for (j = 0; j < NUMBONEMORPH; j++)
               if (pbi[i].aBoneMorph[j].szName[0])
                  *pfHasBoneMorph = TRUE;
      }

   } // i

   return pBone;
}

/**********************************************************************************
CPolyMesh::BoneApplyToRender - This sees if a bone is to be used. If it is then
it will apply the bone deformations to all the points before actually rendering.

inputs
   PCPolyMesh        pTo - Apply to this mesh. Use the bones from the original mesh.
   PCObjectBone      pBone - Bone to use
   PCMatrix          pmObjectToWorld - Matrix that converts from this object to world coords
*/
void CPolyMesh::BoneApplyToRender (PCPolyMesh pUse, PCObjectBone pBone, PCMatrix pmObjectToWorld)
{
   // figure out inverse matrix that goes back to object space
   CMatrix mInv;
   pmObjectToWorld->Invert4 (&mInv);

   // get the bone up to date
   if (!pBone->ObjEditBoneSetup ())
      return;
   pBone->ObjEditBonesTestObject (-1, TRUE);

   // loop through all the points
   PCPMVert *ppvUse = (PCPMVert*) pUse->m_lVert.Get(0);
   PCPMVert *ppvOrig = (PCPMVert*) m_lVert.Get(0);
   DWORD dwNum = m_lVert.Num();
   DWORD i, j;
   CListFixed lBone, lWeight;
   lBone.Init (sizeof(PCBone));
   lWeight.Init (sizeof(fp));
   PPMBONEINFO pbi = (PPMBONEINFO)m_lPMBONEINFO.Get(0);
   DWORD dwNumInfo = m_lPMBONEINFO.Num();
   for (i = 0; i < dwNum; i++) {
      PCPMVert pvo = ppvOrig[i];
      PCPMVert pvn = ppvUse[i];

      // if no bone weights then ignore
      if (!pvo->m_wNumBoneWeight)
         continue;

      lBone.Clear();
      lWeight.Clear();
      PBONEWEIGHT pbw = pvo->BoneWeightGet ();
      for (j = 0; j < pvo->m_wNumBoneWeight; j++) {
         if ((pbw[j].dwBone >= dwNumInfo) || !pbi[pbw[j].dwBone].pBone)
            continue;   // shouldn happen

         lBone.Add (&pbi[pbw[j].dwBone].pBone);
         lWeight.Add (&pbw[j].fWeight);
      } // j
      if (!lBone.Num())
         continue;

      // convert the point to world space
      CPoint p;
      p.Copy (&pvn->m_pLoc);
      p.p[3] = 1;
      p.MultiplyLeft (pmObjectToWorld);
      pBone->ObjEditBonePointConvert (&p, NULL, &pvn->m_pLoc, NULL,
         lBone.Num(), (PCBone*)lBone.Get(0), (fp*)lWeight.Get(0));

      // Need to remember if any of the points actually converted, since
      // if they are can't modify by dragging
      if (!m_fBoneChangeShape && !p.AreClose (&pvn->m_pLoc))
         m_fBoneChangeShape = TRUE;

      pvn->m_pLoc.p[3] = 1;
      pvn->m_pLoc.MultiplyLeft (&mInv);
   } // i

   m_fBonesAlreadyApplied = TRUE;   // since applied bones
}

/*****************************************************************************************
CPolyMesh::BonePaint - Deals with painting a morph area

inputs
   PCPoint           pCenter - Center of the organic movement, in object space
   fp                fRadius - Radius of the movement, in meters
   fp                fBrushPow - Raise the brush shape to this power, higher values are pointier
   fp                fBrushStrength - From 0 to 1. Exact meaning varies based upon dwType
   DWORD             dwNumSide - Number of sides to limit to. If NULL then allow all sides
   DWORD             *padwSide - Array of dwNumSide sides
   DWORD             dwType - Type of movement:
                        0 - increase strength of active bone
                        1 - decrease strength of actibe bone
                        2 - set strength of active bone to fBrushStrength
                        3 - zero strength of active bone
   PCWorldSocket     pWorld - World. Needed to sync up with skeleton
returns
   BOOL - TRUE if success, FALSE if not because morphs going on something
*/
BOOL CPolyMesh::BonePaint (PCPoint pCenter, fp fRadius, fp fBrushPow, fp fBrushStrength,
                             DWORD dwNumSide, DWORD *padwSide, DWORD dwType, PCWorldSocket pWorld)
{
   // NOTE: Take check out for CanModify() since dont really care

   // if have set to view all bones then cant modify
   if (m_dwBoneDisplay >= m_lPMBONEINFO.Num())
      return FALSE;

   // calculate bone mirrors
   PCObjectBone pBone = BoneGet(pWorld);
   if (!pBone)
      return FALSE;
   BoneMirrorCalc (pBone);
   PPMBONEINFO pbi = (PPMBONEINFO)m_lPMBONEINFO.Get(0);
   DWORD dwNumBone = m_lPMBONEINFO.Num();

   // calc symmetry if necessary
   SymmetryRecalc();

   // since the sides may not be sorted or reduced, do so
   CListFixed lTemp, lSides;
   lTemp.Init (sizeof(DWORD), padwSide, dwNumSide);
   SortListAndRemoveDup (1, &lTemp, &lSides);
   dwNumSide = lSides.Num();
   padwSide = (DWORD*) lSides.Get(0);

   // figure out the vertss to look through
   CListFixed lVertLook, lVertIn, lVertStrength, lVertScale;
   lVertLook.Init (sizeof(DWORD));
   lVertIn.Init (sizeof(DWORD));
   lVertStrength.Init (sizeof(fp));
   lVertScale.Init (sizeof(fp));
   if (dwNumSide)
      SidesToVert (padwSide, dwNumSide, &lVertLook);
   DWORD i, dwVert;
   PCPMVert pv;
   DWORD *padw = (DWORD*) lVertLook.Get(0);
   CPoint p;
   fp f;
   for (i = 0; i < (dwNumSide ? lVertLook.Num() : m_lVert.Num()); i++) {
      pv = VertGet(dwVert = (dwNumSide ? padw[i] : i));
      p.Subtract (&pv->m_pLocSubdivide, pCenter);
      if ((fabs(p.p[0]) > fRadius) || (fabs(p.p[1]) > fRadius) ||(fabs(p.p[2]) > fRadius))
         continue;
      f = p.Length();
      if (f >= fRadius)
         continue;

      // compute the strength
      if (dwType == 2)
         f = fBrushStrength; // set
      else if (dwType == 3)
         f = 0;   // erase
      else {
         f = cos (f / fRadius * PI/2);
         f = pow (f, fBrushPow);
         f *= fBrushStrength;
      }


      // add vertex
      lVertIn.Add (&dwVert);
      lVertStrength.Add (&f);
   } // i

   // loop and modify the points
   fp *pafStrength = (fp*)lVertStrength.Get(0);
   fp *pafScale = (fp*)lVertScale.Get(0);
   padw = (DWORD*)lVertIn.Get(0);
   DWORD j, k;
   for (i = 0; i < lVertIn.Num(); i++) {
      pv = VertGet(padw[i]);

      // if any of its mirrors exist on the list and they have a lower number
      // than the current one then skip. So if paint across mirror will only
      // affect equally
      PMMIRROR *ppm = pv->MirrorGet();
      for (j = 0; j < pv->m_wNumMirror; j++)
         if ((ppm[j].dwObject < i) && (-1 != DWORDSearch (ppm[j].dwObject, lVertIn.Num(), padw)))
            break;
      if (j < pv->m_wNumMirror)
         continue;

      // loop through this and all its mirrors...
      for (j = 0; j <= pv->m_wNumMirror; j++) {
         PCPMVert pvm = j  ? VertGet(ppm[j-1].dwObject) : pv;
         DWORD dwWant = j ? ppm[j-1].dwType : 0;

         // figure out where to write to
         DWORD dwMod = m_dwBoneDisplay;
         if (dwWant) {
            if (pbi[m_dwBoneDisplay].apMirror[dwWant-1] == NULL)
               continue;   // nothing to mirror there

            // find match
            for (k = 0; k < dwNumBone; k++)
               if (pbi[k].pBone == pbi[m_dwBoneDisplay].apMirror[dwWant-1]) {
                  dwMod = k;  // found a mirror
                  break;
               }
            // NOTE: Mirroring works well only as long as mesh and bones have
            // same orientation
         }

         // find the entry for the original so can mirror this exactly
         PBONEWEIGHT pbwOrig = pv->BoneWeightGet();
         for (k = 0; k < pv->m_wNumBoneWeight; k++)
            if (pbwOrig[k].dwBone == m_dwBoneDisplay)
               break;
         DWORD dwBoneOrig = (k < pv->m_wNumBoneWeight) ? k : -1;

         // get the bone looking for an entry
         PBONEWEIGHT pbwMirror = pvm->BoneWeightGet();
         for (k = 0; k < pvm->m_wNumBoneWeight; k++)
            if (pbwMirror[k].dwBone == dwMod)
               break;
         DWORD dwBoneMirror = (k < pvm->m_wNumBoneWeight) ? k : -1;

         // if we're modifying a mirror then just copy the mirrored bit over
         if (m_dwBoneDisplay != dwMod) {
            if (dwBoneOrig == -1) {
               // setting no longer exists in orig, so remove from this
               if (dwBoneMirror != -1) {
                  k = -1;
                  pvm->BoneWeightRename (1, &dwMod, &k);
               }
            }
            else {
               // setting exists in orig, so make sure to set
               if (dwBoneMirror != -1)
                  pbwMirror[dwBoneMirror].fWeight = pbwOrig[dwBoneOrig].fWeight;
               else {
                  // need to add
                  BONEWEIGHT bw = pbwOrig[dwBoneOrig];
                  bw.dwBone = dwMod;
                  pvm->BoneWeightAdd (&bw);
               }
            }
            continue;
         }

         // else, modifying the original

         // get the value
         fp fVal;
         fVal = (dwBoneOrig != -1) ? pbwOrig[dwBoneOrig].fWeight : 0;

         switch (dwType) {
         case 0:  // increase by fScale
            fVal += pafStrength[i];
            break;

         case 1:  // decrease by fScale
            fVal -= pafStrength[i];
            fVal = max(fVal, 0);
            break;

         case 2: // set
            fVal = pafStrength[i];
            break;

         case 3:  // totall wipe out
            fVal = 0;
            break;
         }

         if (fVal > CLOSE) {
            // want it added
            if (dwBoneOrig != -1)
               pbwOrig[dwBoneOrig].fWeight = fVal;
            else {
               BONEWEIGHT bw;
               memset (&bw, 0, sizeof(bw));
               bw.dwBone = m_dwBoneDisplay;
               bw.fWeight = fVal;
               pv->BoneWeightAdd (&bw);
            }
         }
         else {
            if (dwBoneOrig != -1) {
               k = -1;
               pv->BoneWeightRename (1, &m_dwBoneDisplay, &k);
            }
         }

      // reload mirror because might be changed
      ppm = pv->MirrorGet();

      } // j
   } // i

   return TRUE;
}


/*****************************************************************************************
CPolyMesh::BoneMirror - Mirrors the bone weights from one side to another.

inputs
   DWORD             dwDim - Dimension, 0 = x, 1 = y, 2 = z
   BOOL              fKeepPos - If TRUE keep the positive info, else the negative
   PCWorldSocket     pWorld - World. Needed to sync up with skeleton
   BOOL              fCopy - If TRUE just copy (dont actually mirror bone)
returns
   BOOL - TRUE if success, FALSE if not because doesnt support symmetry in that direction
*/
BOOL CPolyMesh::BoneMirror (DWORD dwDim, BOOL fKeepPos, PCWorldSocket pWorld, BOOL fCopy)
{
   // if symmetry wrong then fail
   DWORD dwSym = (1 << dwDim);
   if (!(m_dwSymmetry & dwSym))
      return FALSE;

   // calculate bone mirrors
   PCObjectBone pBone = BoneGet(pWorld);
   if (!pBone)
      return FALSE;
   BoneMirrorCalc (pBone);
   PPMBONEINFO pbi = (PPMBONEINFO)m_lPMBONEINFO.Get(0);
   DWORD dwNumBone = m_lPMBONEINFO.Num();

   // calc symmetry if necessary
   SymmetryRecalc();

   DWORD dwBoneMirror = BoneDisplayGet();

   // figure out mirror for every boneinfo
   CListFixed lBoneFrom, lBoneTo;
   lBoneFrom.Init (sizeof(DWORD));
   lBoneTo.Init (sizeof(DWORD));
   DWORD i, j;
   for (i = 0; i < dwNumBone; i++) {
      for (j = 0; j < 7; j++)
         if (pbi[i].apMirror[j])
            break;
      if ((j >= 7) || fCopy) {   // if copying then dont change which bone is used
         // no mirrors for this
         lBoneFrom.Add (&i);
         lBoneTo.Add (&i);
         continue;
      }

      // find this..
      PCBone pLook = pbi[i].apMirror[j];
      for (j = 0; j < dwNumBone; j++)
         if (pbi[j].pBone == pLook)
            break;
      if (j >= dwNumBone) {
         // no mirrors for this
         lBoneFrom.Add (&i);
         lBoneTo.Add (&i);
         continue;
      }

      // else mirror
      lBoneFrom.Add (&i);
      lBoneTo.Add (&j);
   }
   DWORD dwBoneFromNum = lBoneFrom.Num();
   DWORD *padwBoneFrom = (DWORD*)lBoneFrom.Get(0);
   DWORD *padwBoneTo = (DWORD*)lBoneTo.Get(0);

   // loop throgh all vertices
   PCPMVert *ppv = (PCPMVert*)m_lVert.Get(0);
   for (i = 0; i < m_lVert.Num(); i++) {
      PCPMVert pv = ppv[i];
      if (pv->m_pLoc.p[dwDim] * (fKeepPos ? 1 : -1) <= 0)
         continue;   // on wrong side

      // else, could mirror this... find all mirrors
      PPMMIRROR pmm = pv->MirrorGet();
      for (j = 0; j < pv->m_wNumMirror; j++)
         if (pmm[j].dwType == dwSym)
            break;
      if (j >= pv->m_wNumMirror)
         continue;   // no mirror found

      // else, have mirror, so get it
      PCPMVert pvm = ppv[pmm[j].dwObject];
      PBONEWEIGHT pbw = pv->BoneWeightGet();

      if (dwBoneMirror != -1) {
         // mirroring only one bone
         // remove the to-be mirrored value
         j = -1;
         pvm->BoneWeightRename (1, &padwBoneTo[dwBoneMirror], &j);

         // add it in
         for (j = 0; j < pv->m_wNumBoneWeight; j++)
            if (pbw[j].dwBone == padwBoneFrom[dwBoneMirror]) {
               BONEWEIGHT bw;
               memset (&bw, 0, sizeof(bw));
               bw.dwBone = padwBoneTo[dwBoneMirror];
               bw.fWeight = pbw[j].fWeight;
               pvm->BoneWeightAdd (&bw);
               break;
            }
      }
      else {
         // mirror al the bones

         // clear out original values
         pvm->BoneWeightClear ();

         // add in exisintg
         pvm->BoneWeightAdd (pbw, pv->m_wNumBoneWeight);

         // rename
         pvm->BoneWeightRename (dwBoneFromNum, padwBoneFrom, padwBoneTo);
      }
   } // i

   m_fDirtyRender = TRUE;

   return TRUE;
}




/* PolyMeshBoneEditPage
*/
static BOOL PolyMeshBoneEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMD2PAGE po = (PMD2PAGE)pPage->m_pUserData;
   PCPolyMesh pm = po->pm;
   DWORD dwNum;
   PPMBONEINFO pbiAll = pm->BoneEnum (&dwNum);
   PPMBONEINFO pbi = pbiAll + po->dwEdit;

   switch (dwMessage) {

   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         // fill in the comboboxes...
         DWORD dwIndex[NUMBONEMORPH];
         DWORD i, j;
         WCHAR szTemp[64];
         for (i = 0; i < NUMBONEMORPH; i++)
            dwIndex[i] = -1;
         MemZero (&gMemTemp);
         PCOEAttrib *pap = (PCOEAttrib*) pm->m_lPCOEAttrib.Get(0);
         for (i = 0; i <= pm->m_lPCOEAttrib.Num(); i++) {
            MemCat (&gMemTemp, L"<elem name=");
            MemCat (&gMemTemp, (int)i);
            MemCat (&gMemTemp, L">");
            if (i < pm->m_lPCOEAttrib.Num()) {
               MemCatSanitize (&gMemTemp, pap[i]->m_szName);

               for (j = 0; j < NUMBONEMORPH; j++)
                  if (!_wcsicmp(pbi->aBoneMorph[j].szName, pap[i]->m_szName))
                     dwIndex[j] = i;
            }
            else
               MemCat (&gMemTemp, L"<italic>None</italic>");
            MemCat (&gMemTemp, L"</elem>");
         } // i
         for (j = 0; j < NUMBONEMORPH; j++) {
            if (dwIndex[j] == -1) {
               dwIndex[j] = pm->m_lPCOEAttrib.Num();
               pbi->aBoneMorph[j].szName[0] = 0;   // just in case was old attribute name
            }

            ESCMCOMBOBOXADD add;
            memset (&add, 0, sizeof(add));
            add.pszMML = (PWSTR) gMemTemp.p;
            add.dwInsertBefore = 0;
            swprintf (szTemp, L"morph%d", (int)j);
            pControl = pPage->ControlFind (szTemp);
            if (pControl) {
               pControl->Message (ESCM_COMBOBOXADD, &add);
               pControl->AttribSetInt (CurSel(), dwIndex[j]);
            }

            // other values
            swprintf (szTemp, L"dim%d", (int)j);
            pControl = pPage->ControlFind (szTemp);
            if (pControl)
               pControl->AttribSetInt (CurSel(), pbi->aBoneMorph[j].dwDim);

            if (pbi->aBoneMorph[j].dwDim < 3) {
               swprintf (szTemp, L"anglelow%d", (int)j);
               AngleToControl (pPage, szTemp, pbi->aBoneMorph[j].tpBoneAngle.h, TRUE);
               swprintf (szTemp, L"anglehi%d", (int)j);
               AngleToControl (pPage, szTemp, pbi->aBoneMorph[j].tpBoneAngle.v, TRUE);
            }
            else {
               swprintf (szTemp, L"anglelow%d", (int)j);
               DoubleToControl (pPage, szTemp, pbi->aBoneMorph[j].tpBoneAngle.h);
               swprintf (szTemp, L"anglehi%d", (int)j);
               DoubleToControl (pPage, szTemp, pbi->aBoneMorph[j].tpBoneAngle.v);
            }

            swprintf (szTemp, L"morphlow%d", (int)j);
            DoubleToControl (pPage, szTemp, pbi->aBoneMorph[j].tpMorphValue.h);
            swprintf (szTemp, L"morphhi%d", (int)j);
            DoubleToControl (pPage, szTemp, pbi->aBoneMorph[j].tpMorphValue.v);
         }
      }
      break;

   case ESCM_USER+82:   // get the values based on m_dwInfoType
      {
         po->pWorld->ObjectAboutToChange (po->pObject);

         // get values
         DWORD j;
         WCHAR szTemp[64];
         PCEscControl pControl;
         PWSTR psz;

         for (j = 0; j < NUMBONEMORPH; j++) {
            swprintf (szTemp, L"morph%d", (int)j);
            pControl = pPage->ControlFind (szTemp);
            if (!pControl)
               continue;
            DWORD dwVal = pControl->AttribGetInt (CurSel());
            PCOEAttrib *pap = (PCOEAttrib*) pm->m_lPCOEAttrib.Get(0);
            if (dwVal < pm->m_lPCOEAttrib.Num())
               psz = pap[dwVal]->m_szName;
            else
               psz = L"";
            wcscpy (pbi->aBoneMorph[j].szName, psz);
            if (!psz[0])
               continue;   // dont get anymore

            swprintf (szTemp, L"dim%d", (int)j);
            pControl = pPage->ControlFind (szTemp);
            if (!pControl)
               continue;
            pbi->aBoneMorph[j].dwDim = (DWORD) pControl->AttribGetInt (CurSel());

            if (pbi->aBoneMorph[j].dwDim < 3) {
               swprintf (szTemp, L"anglelow%d", (int)j);
               pbi->aBoneMorph[j].tpBoneAngle.h = AngleFromControl (pPage, szTemp);
               swprintf (szTemp, L"anglehi%d", (int)j);
               pbi->aBoneMorph[j].tpBoneAngle.v = AngleFromControl (pPage, szTemp);
            }
            else {
               swprintf (szTemp, L"anglelow%d", (int)j);
               pbi->aBoneMorph[j].tpBoneAngle.h = DoubleFromControl (pPage, szTemp);
               swprintf (szTemp, L"anglehi%d", (int)j);
               pbi->aBoneMorph[j].tpBoneAngle.v = DoubleFromControl (pPage, szTemp);
            }

            swprintf (szTemp, L"morphlow%d", (int)j);
            pbi->aBoneMorph[j].tpMorphValue.h = DoubleFromControl (pPage, szTemp);
            swprintf (szTemp, L"morphhi%d", (int)j);
            pbi->aBoneMorph[j].tpMorphValue.v = DoubleFromControl (pPage, szTemp);

         } // j

         // remap so can see changes
         pm->CalcMorphRemap();
         pm->m_fDirtyRender = TRUE;
         po->pWorld->ObjectChanged (po->pObject);
      }
      break;

   case ESCN_EDITCHANGE:
      // since any edit message is for one of the values, just get all
      pPage->Message (ESCM_USER+82);
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz;
         psz = p->pControl->m_pszName;

         PWSTR pszMorph = L"morph";
         DWORD dwMorphLen = (DWORD)wcslen(pszMorph);
         PWSTR pszDim = L"dim";
         DWORD dwDimLen = (DWORD)wcslen(pszDim);
         if (!wcsncmp(psz, pszMorph, dwMorphLen)) {
            DWORD j = _wtoi(psz + dwMorphLen);
            if (j >= NUMBONEMORPH)
               break;   // shouldnt happen

            DWORD dwVal = p->dwCurSel;
            PCOEAttrib *pap = (PCOEAttrib*) pm->m_lPCOEAttrib.Get(0);
            if (dwVal < pm->m_lPCOEAttrib.Num())
               psz = pap[dwVal]->m_szName;
            else
               psz = L"";
            if (!_wcsicmp(psz, pbi->aBoneMorph[j].szName))
               break;   // not really changed

            // else, get new
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
         else if (!wcsncmp(psz, pszDim, dwDimLen)) {
            DWORD j = _wtoi(psz + dwDimLen);
            if (j >= NUMBONEMORPH)
               break;   // shouldnt happen

            if (p->dwCurSel == pbi->aBoneMorph[j].dwDim)
               break;

            // else, get new
            pPage->Message (ESCM_USER+82);
            return TRUE;
         }
      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Bone affects morph";
            return TRUE;
         }
      }
      break;
   };


   return DefPage (pPage, dwMessage, pParam);
}


/**********************************************************************************
CPolyMesh::DialogEditBone - Causes the object to show a dialog box that allows
editing of the morph.

inputs
   PCEscWindow       pWindow - Escarpment window to draw it in
   PCWorldSocket     pWorld - Notify when object changes
   PCObjectSocket    pObject - Notify when object changes
   DWORD             dwBone - Bone index to edit, from 0 to #bones-1
returns
   BOOL - TRUE if user pressed "Back", FALSE if user pressed "Close"
*/
BOOL CPolyMesh::DialogEditBone (PCEscWindow pWindow, PCWorldSocket pWorld, PCObjectSocket pObject,
                                 DWORD dwBone)
{
   PWSTR pszRet;
   MD2PAGE oe;
   memset (&oe, 0, sizeof(oe));
   oe.pm = this;
   oe.pWorld = pWorld;
   oe.pObject = pObject;
   oe.dwEdit = dwBone;

   pszRet = pWindow->PageDialog (ghInstance, IDR_MMLPOLYMESHBONEEDIT, PolyMeshBoneEditPage, &oe);

   return pszRet && !_wcsicmp(pszRet, Back());
}

