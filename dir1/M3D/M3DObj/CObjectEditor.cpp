/************************************************************************
CObjectEditor.cpp - Draws a Editor.

begun 17/8/02 by Mike Rozak
Copyright 2002 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"

// OBJWORLDCACHE - Keep track of which worlds are around to be used for objects
typedef struct {
   GUID        gCode;      // object code
   GUID        gSub;       // object sub code
   PCWorld     pWorld;     // world. Must be freed when this structure is deleted
   PCListFixed plCutout;   // list of CPoints that are the cutout for the shape. Must be deleted
   DWORD       dwLocation; // from gpszWSObjLocation, 0 for ground (default), 1 for wall, 2 for ceiling
   DWORD       dwEmbed;    // from gpszWSObjEmbed object embedding - 0 for none, 1 for just stick, 2 for one layers, 3 for two layers
   DWORD       dwCount;    // usage count
   DWORD       dwRenderShow;  // what to show renderer
   GUID        gLastInstanceUsed;   // keep track of last instance used. Don't need to reset attributes if instances
                                    // unchanged
   PCObjectSocket pLastInstanceUsed;   // pointer to the object for last instance used, incase undo/redo
                                    // causes to have same gLastInstanceUsed
   PCListFixed plCOEAttrib;      // pointer to a list of COEAttrib's. Must be deleted

   PCListFixed plCPEXPORT;    // pointer to a list of control-panel exports

   DWORD       dwRenderDepth; // starts at 0 and increases every time render. Make sure not rendering too many iterations
                           // of iteself and getting in infinite loop
   BOOL        fLoading;   // set the TRUE while the world is loading - ensures that no recursions
   CMatrix     mRot;       // extra rotation of object, used if dwEmbed, based on dwLocation

   // bones
   PCObjectBone pObjectBone;   // bones object
   GUID        gObjectBone;   // guid of the bone
} OBJWORLDCACHE, *POBJWORLDCACHE;

// CPEXPORT - Structure to remember which control points are exported
typedef struct {
   PCObjectSocket    pos;     // object that's exporting it
   DWORD             dwID;    // ID in the exported object
} CPEXPORT, *PCPEXPORT;

#define MAXRENDERDEPTH     5

static CListVariable galOBJWORLDCACHE[MAXRENDERSHARDS];     // list of object world information


typedef struct {
   PWSTR       pszName;    // name of the move point
   int         iX, iY; // -1 if for left/top, 1 for right/bottom
} SPLINEMOVEP, *PSPLINEMOVEP;

static SPLINEMOVEP   gaObjectMove[] = {
   L"Object base", 0, 0
};


/**********************************************************************************
COEAttrib::Constructor and destructor */
COEAttrib::COEAttrib (void)
{
   m_fAutomatic = FALSE;
   m_dwType = 0;
   m_szName[0] = 0;
   m_lCOEATTRIBCOMBO.Init (sizeof(COEATTRIBCOMBO));
   memset (&m_gRemove, 0, sizeof(m_gRemove));
   m_fDefValue = 0;
   m_fMin = 0;
   m_fMax = 1;
   m_dwInfoType = AIT_NUMBER;
   m_fDefLowRank = FALSE;
   m_fDefPassUp = FALSE;
   m_fCurValue = m_fMin;
}

COEAttrib::~COEAttrib (void)
{
   // do nothing for now
}


static PWSTR gpszType = L"Type";
static PWSTR gpszName = L"Name";
static PWSTR gpszRemove = L"Remove";
static PWSTR gpszDefValue = L"DefVal";
static PWSTR gpszInfo = L"Info";
static PWSTR gpszMin = L"Min";
static PWSTR gpszMax = L"Max";
static PWSTR gpszInfoType = L"InfoType";
static PWSTR gpszCOEATTRIBCOMBO = L"COAC";
static PWSTR gpszFromObject = L"FromObject";
static PWSTR gpszNumRemap = L"NumRemap";
static PWSTR gpszCapEnds = L"CapEnds";
static PWSTR gpszAttribList = L"AttribList";
static PWSTR gpszDefLowRank = L"DefLowRank";
static PWSTR gpszDefPassUp = L"DefPassUp";
static PWSTR gpszMorphID = L"MorphID";

/**********************************************************************************
COEAttrib::MMLFrom - Standard MMLFrom.
*/
BOOL COEAttrib::MMLFrom (PCMMLNode2 pNode)
{
   m_fAutomatic = FALSE;

   m_dwType = (int) MMLValueGetInt (pNode, gpszType, 0);
   PWSTR pszName;
   pszName = MMLValueGet (pNode, gpszName);
   if (pszName)
      wcscpy (m_szName, pszName);
   else
      wcscpy (m_szName, L"Unknown");

   // if it's a remove then just remember this
   if (m_dwType == 1) {
      MMLValueGetBinary (pNode, gpszRemove, (PBYTE) &m_gRemove, sizeof(m_gRemove));
      return TRUE;
   }

   // else, new attribute, so info
   m_fDefValue = MMLValueGetDouble (pNode, gpszDefValue, 0);
   m_fCurValue = m_fDefValue;
   PWSTR psz;
   psz = MMLValueGet (pNode, gpszInfo);
   if (psz) {
      if (m_memInfo.Required ((wcslen(psz)+1)*2))
         wcscpy ((PWSTR)m_memInfo.p, psz);
   }
   m_fMin = MMLValueGetDouble (pNode, gpszMin, 0);
   m_fMax = MMLValueGetDouble (pNode, gpszMax, 1);
   m_dwInfoType = (DWORD) MMLValueGetInt (pNode, gpszInfoType, AIT_NUMBER);
   m_fDefLowRank = (BOOL) MMLValueGetInt (pNode, gpszDefLowRank, FALSE);
   m_fDefPassUp = (BOOL) MMLValueGetInt (pNode, gpszDefPassUp, FALSE);

   // attribute combo
   m_lCOEATTRIBCOMBO.Clear();
   DWORD i;
   PCMMLNode2 pSub;
   for (i = 0; i < pNode->ContentNum(); i++) {
      pSub = NULL;
      pNode->ContentEnum (i, &psz, &pSub);
      if (!pSub)
         continue;
      psz = pSub->NameGet();
      if (!psz || _wcsicmp(psz, gpszCOEATTRIBCOMBO))
         continue;

      // new entry
      COEATTRIBCOMBO co;
      memset (&co, 0, sizeof(co));

      MMLValueGetBinary (pSub, gpszFromObject, (PBYTE) &co.gFromObject, sizeof(co.gFromObject));
      psz = MMLValueGet (pSub, gpszName);
      if (psz) // BUGFIX - If no namethen just leave blank string
         wcscpy (co.szName, psz);
      co.dwMorphID = (DWORD) MMLValueGetInt (pSub, gpszMorphID, 0);
      co.dwNumRemap = (DWORD) MMLValueGetInt (pSub, gpszNumRemap, 0);
      if (!co.dwNumRemap)
         continue;
      co.dwNumRemap = min(COEMAXREMAP-1, co.dwNumRemap);
      co.dwCapEnds = (DWORD) MMLValueGetInt (pSub, gpszCapEnds, 0);

      DWORD j;
      WCHAR szTemp[64];
      TEXTUREPOINT tp;
      for (j = 0; j < co.dwNumRemap; j++) {
         swprintf (szTemp, L"CToE%d", (int) j);
         tp.h = tp.v = 0;
         MMLValueGetTEXTUREPOINT (pSub, szTemp, &tp);
         co.afpComboValue[j] = tp.h;
         co.afpObjectValue[j] = tp.v;
      }
      m_lCOEATTRIBCOMBO.Add (&co);
   } // over all attributes

   return TRUE;
}


/**********************************************************************************
COEAttrib::CloneTo - Clones this object over the existing one.

inputs
   COEAttrib         *pTo - Clone over this one
returns
   none
*/
void COEAttrib::CloneTo (COEAttrib *pTo)
{
   pTo->m_dwInfoType = m_dwInfoType;
   pTo->m_dwType = m_dwType;
   pTo->m_fAutomatic = m_fAutomatic;
   pTo->m_fCurValue = m_fCurValue;
   pTo->m_fDefLowRank = m_fDefLowRank;
   pTo->m_fDefPassUp = m_fDefPassUp;
   pTo->m_fDefValue = m_fDefValue;
   pTo->m_fMax = m_fMax;
   pTo->m_fMin = m_fMin;
   pTo->m_gRemove = m_gRemove;
   wcscpy (pTo->m_szName, m_szName);
   pTo->m_lCOEATTRIBCOMBO.Init (sizeof(COEATTRIBCOMBO), m_lCOEATTRIBCOMBO.Get(0),m_lCOEATTRIBCOMBO.Num());
   if (m_memInfo.p && pTo->m_memInfo.Required ((wcslen((PWSTR)m_memInfo.p)+1)*2))
      wcscpy ((PWSTR)pTo->m_memInfo.p, (PWSTR)m_memInfo.p);
}

/**********************************************************************************
COEAttrib::MMLTo - Standard MMLTo. EXCEPT: If automatically generated then MMLTo()
returns NULL.
*/
PCMMLNode2 COEAttrib::MMLTo (void)
{
   PCMMLNode2 pNode;
   if (m_fAutomatic)
      return NULL;

   pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (Attrib());

   MMLValueSet (pNode, gpszType, (int) m_dwType);
   MMLValueSet (pNode, gpszName, m_szName);

   // if it's a remove then just remember this
   if (m_dwType == 1) {
      MMLValueSet (pNode, gpszRemove, (PBYTE) &m_gRemove, sizeof(m_gRemove));
      return pNode;
   }

   // else, new attribute, so info
   MMLValueSet (pNode, gpszDefValue, m_fDefValue);
   if (m_memInfo.p && ((PWSTR)m_memInfo.p)[0])
      MMLValueSet (pNode, gpszInfo, (PWSTR) m_memInfo.p);
   MMLValueSet (pNode, gpszMin, m_fMin);
   MMLValueSet (pNode, gpszMax, m_fMax);
   MMLValueSet (pNode, gpszInfoType, (int)m_dwInfoType);
   MMLValueSet (pNode, gpszDefLowRank, (int)m_fDefLowRank);
   MMLValueSet (pNode, gpszDefPassUp, (int)m_fDefPassUp);

   // attribute combo
   DWORD i;
   PCMMLNode2 pSub;
   PCOEATTRIBCOMBO pc;
   pc = (PCOEATTRIBCOMBO) m_lCOEATTRIBCOMBO.Get(0);
   for (i = 0; i < m_lCOEATTRIBCOMBO.Num(); i++, pc++) {
      pSub = pNode->ContentAddNewNode ();
      if (!pSub)
         continue;
      pSub->NameSet (gpszCOEATTRIBCOMBO);

      MMLValueSet (pSub, gpszFromObject, (PBYTE) &pc->gFromObject, sizeof(pc->gFromObject));
      if (pc->szName[0])   // BUGFIX - If empty name dont write in
         MMLValueSet (pSub, gpszName, pc->szName);
      if (pc->dwMorphID)
         MMLValueSet (pSub, gpszMorphID, (int) pc->dwMorphID);
      MMLValueSet (pSub, gpszNumRemap, (int)pc->dwNumRemap);
      MMLValueSet (pSub, gpszCapEnds, (int)pc->dwCapEnds);

      DWORD j;
      WCHAR szTemp[64];
      TEXTUREPOINT tp;
      for (j = 0; j < pc->dwNumRemap; j++) {
         swprintf (szTemp, L"CToE%d", (int) j);
         tp.h = pc->afpComboValue[j];
         tp.v = pc->afpObjectValue[j];
         MMLValueSet (pSub, szTemp, &tp);
      }
   } // over all attributes

   return pNode;
}

/**********************************************************************************
ObjEditClearPCOEAttribList - Give a CListFixed of pointers of PCOEAttrib, this frees up
all contained objects. It then wipes the list. Must do this to avoid memory leaks

inputs
   PCListFixed    plPCOE - List to clear
returns
   none
*/
void ObjEditClearPCOEAttribList (PCListFixed plPCOE)
{
   PCOEAttrib *pap = (PCOEAttrib*) plPCOE->Get(0);
   DWORD i;
   for (i = 0; i < plPCOE->Num(); i++)
      delete pap[i];
   plPCOE->Clear();
}


/**********************************************************************************
ObjEditCreatePCOEAttribList - Looks through the given world and pulls out all the
attribute information, filling the list of PCOEAttrib. Call this from
ObjEditAnalyzeCacheItem()

Some tricks-
   - first get the automatic ones from the objects in the world
   - then read in the mml
   - make sure not duplicates (EXCEPT maybe a duplicate of a remove with an exising
      attribute).
   - sorts the list for faster attribute settings

inputs
   PCWorldSocket     pWorld - World to use
   PCListFixed       plPCOE - List to add to with PCOEAttrib.
returns
   none
*/
static int _cdecl OECALCompare (const void *elem1, const void *elem2)
{
   PCOEAttrib *pdw1, *pdw2;
   pdw1 = (PCOEAttrib*) elem1;
   pdw2 = (PCOEAttrib*) elem2;

   int iRet;
   iRet = _wcsicmp(pdw1[0]->m_szName, pdw2[0]->m_szName);
   if (iRet)
      return iRet;

   return (int) pdw1[0]->m_dwType - (int) pdw2[0]->m_dwType;
}

void ObjEditCreatePCOEAttribList (PCWorldSocket pWorld, PCListFixed plPCOE)
{
   // NOTE: Just adding to plPCOE

   // loop through all the existing objects and see what exported
   DWORD i, j, k;
   PCObjectSocket pos;
   CListFixed  lAll;
   GUID gObject;
   PATTRIBVAL pav;
   ATTRIBINFO ai;
   PCOEAttrib *pap;
   lAll.Init (sizeof(ATTRIBVAL));
   for (i = 0; i < pWorld->ObjectNum(); i++) {
      pos = pWorld->ObjectGet (i);
      if (!pos)
         continue;
      pos->GUIDGet (&gObject);

      // get the attributes
      lAll.Clear();
      pos->AttribGetAll (&lAll);
      pav = (PATTRIBVAL) lAll.Get(0);

      // loop through all the attributes
      for (j = 0; j < lAll.Num(); j++, pav++) {
         // get the info on this
         if (!pos->AttribInfo (pav->szName, &ai))
            continue;

         // if it's not automatic up then ignore
         if (!ai.fDefPassUp)
            continue;

         // if it's a template attribute ignoe
         if (IsTemplateAttribute (pav->szName, FALSE))
            continue;

         // try to find an existing match
         DWORD dwMatch;
         dwMatch = -1;
         pap = (PCOEAttrib*) plPCOE->Get(0);
         for (k = 0; k < plPCOE->Num(); k++) {
            // check for name match
            if (_wcsicmp(pap[k]->m_szName, pav->szName))
               continue;

            // found a match if it's automatic and NOT a remove
            if (pap[k]->m_fAutomatic && (pap[k]->m_dwType == 0))
               dwMatch = k;
            else
               dwMatch = -2;  // if found the name, but wasnt automatic or right type then dont add at all
            break;
         } // k, sub elem
         if (dwMatch == -2)
            continue;   // dont add this one

         // fill in the remap info
         COEATTRIBCOMBO oeac;
         memset (&oeac, 0, sizeof(oeac));
         oeac.afpComboValue[0] = 0;
         oeac.afpComboValue[1] = 1;
         oeac.afpObjectValue[0] = 0;
         oeac.afpObjectValue[1] = 1;
         // dont cap ends
         oeac.dwNumRemap = 2;
         oeac.gFromObject = gObject;
         wcscpy (oeac.szName, pav->szName);

         // new entry?
         if (dwMatch == -1) {
            PCOEAttrib pa = new COEAttrib;
            if (!pa)
               continue;    //error
            wcscpy (pa->m_szName, pav->szName);
            pa->m_dwInfoType = ai.dwType;
            pa->m_dwType = 0; // attrib set
            pa->m_fAutomatic = TRUE;
            pa->m_fCurValue = pav->fValue;   // use the value of the first one that find
            pa->m_fDefValue = pa->m_fCurValue;
            pa->m_fMax = ai.fMax;
            pa->m_fMin = ai.fMin;
            pa->m_fDefLowRank = ai.fDefLowRank;
            pa->m_fDefPassUp = ai.fDefPassUp;
            if (ai.pszDescription && ai.pszDescription[0])
               if (pa->m_memInfo.Required ((wcslen(ai.pszDescription)+1)*2))
                  wcscpy ((PWSTR)pa->m_memInfo.p, ai.pszDescription);
            pa->m_lCOEATTRIBCOMBO.Add (&oeac);   // add this remap

            // add this to list
            plPCOE->Add (&pa);
         }
         else {   // have an existing entry
            pap[dwMatch]->m_lCOEATTRIBCOMBO.Add (&oeac);
         }

      } // j, all attributes
   } // i, all objects

   // get the attrib remap values written into the world
   PCMMLNode2 pAux, pNode, pSub;
   PWSTR psz;
   pAux = pWorld->AuxGet ();
   pNode = NULL;
   pAux->ContentEnum (pAux->ContentFind (gpszAttribList), &psz, &pNode);
   if (pNode) {
      for (i = 0; i < pNode->ContentNum(); i++) {
         pSub = NULL;
         pNode->ContentEnum (i, &psz, &pSub);
         if (!pSub)
            continue;
         psz = pSub->NameGet();
         if (!psz || _wcsicmp(psz, Attrib()))
            continue;

         PCOEAttrib pNew;
         pNew = new COEAttrib;
         if (!pNew)
            continue;   // errorv
         if (!pNew->MMLFrom (pSub)) {
            delete pNew;
            continue;   // error
         }

         // if it's a template attribute then ignore
         if (IsTemplateAttribute (pNew->m_szName, FALSE)) {
            delete pNew;
            continue;
         }

         // if this is a remove type and either the object doesn't exist or the attribute
         // that is to be removed is not automatic, then don't bother adding this since
         // it's a dud
         if (pNew->m_dwType == 1) {
            // see if the object to remove exists
            DWORD dwFind;
            PCObjectSocket pos = NULL;
            dwFind = pWorld->ObjectFind (&pNew->m_gRemove);
            if (dwFind != -1)
               pos = pWorld->ObjectGet (dwFind);
            if (!pos) {
               delete pNew;
               continue;
            }

            // make sure the attribute exists
            ATTRIBINFO ai;
            memset (&ai, 0, sizeof(ai));
            if (!pos->AttribInfo (pNew->m_szName, &ai)) {
               delete pNew;
               continue;
            }
            if (!ai.fDefPassUp) {
               delete pNew;
               continue;
            }
         }

         // if it already exsits in the list then remove the one that exists
         for (k = plPCOE->Num() - 1; k < plPCOE->Num(); k--) {
            pap = (PCOEAttrib*) plPCOE->Get(0);

            // check for name match
            if (_wcsicmp(pap[k]->m_szName, pNew->m_szName))
               continue;

            // if this is a remove and the first is an automatic variable then
            // remove reference from list
            if ((pNew->m_dwType == 1) && (pap[k]->m_dwType == 0) && (pap[k]->m_fAutomatic)) {
               PCOEATTRIBCOMBO pc = (PCOEATTRIBCOMBO) pap[k]->m_lCOEATTRIBCOMBO.Get(0);
               for (j = 0; j < pap[k]->m_lCOEATTRIBCOMBO.Num(); j++) {
                  if (IsEqualGUID(pc[j].gFromObject, pNew->m_gRemove) && !_wcsicmp(pc[j].szName, pNew->m_szName)) {
                     pap[k]->m_lCOEATTRIBCOMBO.Remove (j);
                     break;
                  }
               }  // j - remove from remap

               // if nothing is left in remap then remove entirely
               if (!pap[k]->m_lCOEATTRIBCOMBO.Num()) {
                  delete pap[k];
                  plPCOE->Remove (k);
               }
               continue;
            }
         } // k - see if exsits

         // add it in
         plPCOE->Add (&pNew);
      } // over all content
   } // if pNode


   // sort
   qsort (plPCOE->Get(0), plPCOE->Num(), sizeof(PCOEAttrib), OECALCompare);

   // done
}


/**********************************************************************************
ObjEditWritePCOEAttribList - Given a list of attributes, this writes the values
out to the world. (Used in the object editor when saving an object.)

inputs
   PCWorldWorld      pWorld - World to use
   PCListFixed       plPCOE - List of PCOEAttrib
returns
   BOOl - TRUE if succes
*/
BOOL ObjEditWritePCOEAttribList (PCWorldSocket pWorld, PCListFixed plPCOE)
{
   PCMMLNode2 pAux, pNode, pSub;
   pAux = pWorld->AuxGet();
   if (!pAux)
      return FALSE;

   // remove existing one
   DWORD dwFind;
   dwFind = pAux->ContentFind (gpszAttribList);
   if (dwFind != -1)
      pAux->ContentRemove (dwFind);

   // add new one
   pNode = pAux->ContentAddNewNode ();
   if (!pNode)
      return FALSE;
   pNode->NameSet (gpszAttribList);

   // add items
   DWORD i;
   PCOEAttrib *pap;
   pap = (PCOEAttrib*) plPCOE->Get(0);
   for (i = 0; i < plPCOE->Num(); i++) {
      pSub = pap[i]->MMLTo ();
      if (pSub)
         pNode->ContentAdd (pSub);
      // NOTE: Dont need to do name set because already have one set by MMLTo()
   }

   // set dirty flag
   pWorld->DirtySet (TRUE);

   // done
   return TRUE;
}


/**********************************************************************************
ObjEditAttribGet - Gets an attribute using the plPCOE. Note: Will use the current
values stored in PCOE.

inputs
   PCListFixed       plPCOE - List of attributes.
   PWSTR             pszName - Attribute looking for
   fp                *pfValue - filled with value
returns
   BOOL - TRUE if success
*/
BOOL ObjEditAttribGet (PCListFixed plPCOE, PWSTR pszName, fp *pfValue)
{
   // find with own binary search
   DWORD dwCur, i, dwNum;
   PCOEAttrib *pap;
   pap = (PCOEAttrib*) plPCOE->Get(0);
   dwNum = plPCOE->Num();
   for (dwCur = 1; dwCur < dwNum; dwCur *= 2);
   for (i = 0; dwCur; dwCur /= 2) {
      if (i + dwCur >= dwNum)
         continue;   // too large

      if (_wcsicmp(pap[i+dwCur]->m_szName, pszName) < 0)
         i += dwCur;
   }

   // know that i is someplace before the match
   int iRet;
   for (; i < dwNum; i++) {
      iRet = _wcsicmp(pap[i]->m_szName, pszName);
      if (iRet < 0)
         continue;   // move on
      if (iRet > 0)
         break;   // went too high

      // else match, only care about non-delete
      if (pap[i]->m_dwType == 0) {
         *pfValue = pap[i]->m_fCurValue;
         return TRUE;
      }
   }

   // else, not found
   return FALSE;
}


/**********************************************************************************
ObjEditAttribGetAll - Like AttribGetAll() for CObjectSocket, except gets passed
in a PCListFixed oc PCOEAttrib. NOTE: This does NOT initialize the get-all list
or clear it.

inputs
   PCListFixed       plPCOE - List of attributes.
   BOOL              fDefaults - If TRUE, returns defaults. Else, returns current value stored
   BOOL              fOnlyIfDifferent - Fills in attributes only if the current value != default
                        Use when writing out instance data to only store changed values
   BOOL              fIncludeOpenOn - If TRUE include the open/close on/off attributes in the
                        enumeration. Else, ignore them
   PCListFixed       plATTRIBVAL - Attribval elements added
returns
   none
*/
void ObjEditAttribGetAll (PCListFixed plPCOE, BOOL fDefaults, BOOL fOnlyIfDifferent,
                          BOOL fIncludeOpenOn, PCListFixed plATTRIBVAL)
{
   DWORD i, dwNum;
   PCOEAttrib *pap;
   ATTRIBVAL av;
   pap = (PCOEAttrib*) plPCOE->Get(0);
   dwNum = plPCOE->Num();
   memset (&av, 0, sizeof(av));

   // loop
   for (i = 0; i < dwNum; i++) {
      if (pap[i]->m_dwType != 0)
         continue;   // only care about attributes

      if (fOnlyIfDifferent && (pap[i]->m_fDefValue == pap[i]->m_fCurValue))
         continue;

      if (IsTemplateAttribute (pap[i]->m_szName, !fIncludeOpenOn))
         continue;

      wcscpy (av.szName, pap[i]->m_szName);
      av.fValue = fDefaults ? pap[i]->m_fDefValue : pap[i]->m_fCurValue;
      plATTRIBVAL->Add (&av);
   }
}


/**********************************************************************************
ObjEditAttribSetGroup - Like AttribSetGroup() in CObjectSocket, except gets
passed in PCListFixed for list of attributes, and the global world to modify.

tricks:
   - Merges new values in, only noting if change an existing value
   - Figures which sub-objects will have attributes set and which of their attributes
      will be set, and what they'll be set to.
   - Sums the values together because with remaps may get more than one attribute in the
      OE object affecting a sub-object
   - Be very fast about this
   - Note which object had its attributes set last so can speedily ignore changes in future
      
inputs
   POBJWORLDCACHE    pWC - The CListFixed of COEAttrib, CWorldSocket are used from here.
                        Additionally, the gLastInstanceUsed is changed to the current
                        guid.
   GUID              *pgObject - Instance GUID of the object setting it
   DWORD             dwNum - Number of paAttrib
   PATTRIBVAL        paAttrib - List of attributes (NOT necessarily sorted)
   BOOL              fIgnoreTemplate - If TRUE then ignore template attributes
   PCMem             pMemSort - For scratch memory optimizations
   PCListFixed       plChange - For scratch memory optimizations
returns
   BOOL - TRUE if an attribute in the bones was changed, otherwise FALSE
*/
typedef struct {
   GUID        *pgObject;   // pointer to an object guid
   PWSTR       pszAttrib;  // pointer to an attribute name in that object
   fp          fVal;       // value
} OACHANGE, *POACHANGE;

static int _cdecl OACHANGECompare (const void *elem1, const void *elem2)
{
   OACHANGE *pdw1, *pdw2;
   pdw1 = (OACHANGE*) elem1;
   pdw2 = (OACHANGE*) elem2;

   int iRet;
   iRet = memcmp (pdw1->pgObject, pdw2->pgObject, sizeof(GUID));
   if (iRet)
      return iRet;

   return _wcsicmp(pdw1->pszAttrib, pdw2->pszAttrib);
}

static int _cdecl PATTRIBVALCompare (const void *elem1, const void *elem2)
{
   PATTRIBVAL *pdw1, *pdw2;
   pdw1 = (PATTRIBVAL*) elem1;
   pdw2 = (PATTRIBVAL*) elem2;

   return _wcsicmp(pdw1[0]->szName, pdw2[0]->szName);
}

int _cdecl ATTRIBVALCompare (const void *elem1, const void *elem2)
{
   ATTRIBVAL *pdw1, *pdw2;
   pdw1 = (ATTRIBVAL*) elem1;
   pdw2 = (ATTRIBVAL*) elem2;

   return _wcsicmp(pdw1->szName, pdw2->szName);
}

BOOL ObjEditAttribSetGroup (POBJWORLDCACHE pWC, GUID *pgObject, PCObjectSocket pos,
                            DWORD dwNum, PATTRIBVAL paAttrib, BOOL fIgnoreTemplate,
                            PCMem pMemSort, PCListFixed plChange)
{
   BOOL fBoneAttrChanged = FALSE;   // since if change bone attribe will need to change attached

   // note that this is set
   pWC->gLastInstanceUsed = *pgObject;
   pWC->pLastInstanceUsed = pos;

   if (!pWC->plCOEAttrib || !pWC->plCOEAttrib->Num()) {
      return FALSE;  // no remaps
   }

   // sort the attributes so it's faster to set
   // CMem memSort;
   // CListFixed lChange;
   if (!pMemSort->Required (sizeof(PATTRIBVAL) * dwNum))
      return FALSE;
   DWORD i;
   PATTRIBVAL *papVal;
   papVal = (PATTRIBVAL*) pMemSort->p;
   for (i = 0; i < dwNum; i++)
      papVal[i] = paAttrib + i;
   qsort (papVal, dwNum, sizeof(PATTRIBVAL), PATTRIBVALCompare);

   // keep a list of content-attributes that have changed, by
   // object and attribute
   OACHANGE oac;
   memset (&oac, 0, sizeof(oac));
   plChange->Init (sizeof (OACHANGE));
   PCOEAttrib *papAt;
   DWORD dwNumAt;
   papAt = (PCOEAttrib*) pWC->plCOEAttrib->Get(0);
   dwNumAt = pWC->plCOEAttrib->Num();
   DWORD dwCur;   // location in the papVal list
   DWORD dwMaster;   // location in the pWC->plCOEAttrib
   int iRet;
   for (dwCur = dwMaster = 0; (dwCur < dwNum) && (dwMaster < dwNumAt);) {
      if (fIgnoreTemplate && IsTemplateAttribute (papVal[dwCur]->szName, TRUE)) {
         dwCur++;
         continue;
      }

      // advance master until have a match
      iRet = _wcsicmp(papAt[dwMaster]->m_szName, papVal[dwCur]->szName);
      if (iRet < 0) {
         dwMaster++;
         continue;
      }
      if (iRet > 0) {
         // just pased beyond current name, so no mach with this attribute
         dwCur++;
         continue;
      }

      // if this isn't a dwType==0 then ignore since it's a remove
      if (papAt[dwMaster]->m_dwType != 0) {
         dwMaster++;
         continue;
      }

      // if the attributes are the same then ignore both
      if (papAt[dwMaster]->m_fCurValue == papVal[dwCur]->fValue) {
         dwMaster++;
         dwCur++;
         continue;
      }

      // else, attribute changed
      papAt[dwMaster]->m_fCurValue = papVal[dwCur]->fValue;
      PCOEATTRIBCOMBO pc;
      pc = (PCOEATTRIBCOMBO) papAt[dwMaster]->m_lCOEATTRIBCOMBO.Get(0);
      plChange->Required ( papAt[dwMaster]->m_lCOEATTRIBCOMBO.Num());
      for (i = 0; i < papAt[dwMaster]->m_lCOEATTRIBCOMBO.Num(); i++, pc++) {
         oac.pgObject = &pc->gFromObject;
         oac.pszAttrib = pc->szName;
         plChange->Add (&oac);
      }

      // found it, go on
      dwMaster++;
      dwCur++;
   }

   // if nothing changed then fast exit
   if (!plChange->Num())
      return FALSE;

   // sort the list of attributes changed by their guids, and then attributes so that
   // if one attribute changed several times then can restore
   qsort (plChange->Get(0), plChange->Num(), sizeof(OACHANGE), OACHANGECompare);

   // eliminate duplicates
   POACHANGE paoc;
   DWORD dwNumC;
   for (i = plChange->Num()-1; i && (i < plChange->Num()); i--) {
      dwNumC = plChange->Num();
      paoc = (POACHANGE) plChange->Get(0);

      // if i matches i-1 then remove i
      if (!memcmp(paoc[i].pgObject, paoc[i-1].pgObject, sizeof(GUID)) &&
         !_wcsicmp(paoc[i].pszAttrib, paoc[i-1].pszAttrib)) {
            plChange->Remove (i);
         }
   }
   // what's left is a list of attributes in sub-objects which have changes AND
   // which is sorted by object and id

   // go back through list and zero out values
   dwNumC = plChange->Num();
   paoc = (POACHANGE) plChange->Get(0);
   for (i = 0; i < dwNumC; i++)
      paoc[i].fVal = 0;

   // go back through the list and compute the values
   OACHANGE oaFind, *pFind;
   memset (&oaFind, 0, sizeof(oaFind));
   for (dwMaster = 0; dwMaster < dwNumAt; dwMaster++) {
      // if this isn't a dwType==0 then ignore since it's a remove
      if (papAt[dwMaster]->m_dwType != 0)
         continue;

      PCOEATTRIBCOMBO pc;
      fp fIn, fDelta, fVal;
      DWORD j;
      pc = (PCOEATTRIBCOMBO) papAt[dwMaster]->m_lCOEATTRIBCOMBO.Get(0);
      for (i = 0; i < papAt[dwMaster]->m_lCOEATTRIBCOMBO.Num(); i++, pc++) {
         oaFind.pgObject = &pc->gFromObject;
         oaFind.pszAttrib =pc->szName;

         // find this in the list
         pFind = (POACHANGE) bsearch (&oaFind, paoc, dwNumC, sizeof(OACHANGE), OACHANGECompare);
         if (!pFind)
            continue;   // wasnt on the list of attributes changed

         // else, change

         // figure out the ramp...
         fIn = papAt[dwMaster]->m_fCurValue;
         for (j = 0; j < pc->dwNumRemap; j++)
            if (fIn < pc->afpComboValue[j])
               break;
         if (j == 0) {
            // before the start
            if ((pc->dwCapEnds & 0x01) || (pc->afpComboValue[0] == pc->afpComboValue[1]))
               fVal = pc->afpObjectValue[0];
            else
               j++;   // so that uses that trend line
         }
         else if (j == pc->dwNumRemap) {
            if ((pc->dwCapEnds & 0x02) || (pc->afpComboValue[pc->dwNumRemap-1] == pc->afpComboValue[pc->dwNumRemap-2]))
               fVal = pc->afpObjectValue[pc->dwNumRemap-1];
            else
               j--;   // so that uses that trend line
         }
         if ((j > 0) && (j < pc->dwNumRemap)) {
            j--;
            fDelta = pc->afpComboValue[j+1] - pc->afpComboValue[j];
            if (fDelta == 0)
               fDelta = 1; // so no divide by zero
            fVal = (fIn - pc->afpComboValue[j]) / fDelta *
               (pc->afpObjectValue[j+1] - pc->afpObjectValue[j]) +
               pc->afpObjectValue[j];
         }

         // include the value in
         pFind->fVal += fVal;
      }
   }  // over all attribute remaps

   // now that know what sub-objects (and their attributes) get changed,
   // and what they're changed to, go change them
   CListFixed lNew;
   lNew.Init (sizeof(ATTRIBVAL));
   ATTRIBVAL av;
   DWORD j, dwFind;
   PCObjectSocket pos;
   memset (&av, 0, sizeof(av));
   for (i = 0; i < dwNumC;) {
      lNew.Clear();

      // repeat, adding this one and future ones until guids different
      for (j = i; j < dwNumC; j++) {
         if ((j != i) && memcmp(paoc[i].pgObject, paoc[j].pgObject, sizeof(GUID)))
            break;


         // add this
         wcscpy (av.szName, paoc[j].pszAttrib);// BUGIX - Was incorrectly using i
         av.fValue = paoc[j].fVal; // BUGIFIX - Was incorrectly using i
         lNew.Add (&av);
      }

      // send down the changes to all the attributes at once
      dwFind = pWC->pWorld->ObjectFind (paoc[i].pgObject);
      if (dwFind == -1)
         pos = NULL;
      else
         pos = pWC->pWorld->ObjectGet (dwFind);
      if (pos)
         pos->AttribSetGroup (lNew.Num(), (PATTRIBVAL) lNew.Get(0));

      // if the object is the bone object, and have set attributes for it,
      // then make sure to recalc  all the bone locations
      if (pWC->pObjectBone && IsEqualGUID(pWC->gObjectBone, *paoc[i].pgObject)) {
         // BUFIX - Disable because moved into AttribSetGroup
         // of bone object: pWC->pObjectBone->ObjEditBonesCalcPostBend ();

         // remember that have changed the attributes here
         fBoneAttrChanged = TRUE;
      }

      // advance
      i = j;
   }  // over all attributes changed

   // done
   return fBoneAttrChanged;
}

/**********************************************************************************
ObjEditAttribInfo - Like AttribInfo() in CObjectSocket except this uses the plPCOE

inputs
   POBJWORLDCACHE    pWC - The CListFixed of COEAttrib, CWorldSocket are used from here.
                        Additionally, the gLastInstanceUsed is changed to the current
                        guid.
   PCListFixed       plPCOE - List of attributes.
   PWSTR             pszName - Attribute looking for
   PATTRIBINFO       pInfo - Filled in with info
returns
   BOOL - TRUE if success
*/
BOOL ObjEditAttribInfo (POBJWORLDCACHE pwc, PCListFixed plPCOE, PWSTR pszName, PATTRIBINFO pInfo)
{
   // find with own binary search
   DWORD dwCur, i, dwNum;
   PCOEAttrib *pap;
   pap = (PCOEAttrib*) plPCOE->Get(0);
   dwNum = plPCOE->Num();
   for (dwCur = 1; dwCur < dwNum; dwCur *= 2);
   for (i = 0; dwCur; dwCur /= 2) {
      if (i + dwCur >= dwNum)
         continue;   // too large

      if (_wcsicmp(pap[i+dwCur]->m_szName, pszName) < 0)
         i += dwCur;
   }

   // know that i is someplace before the match
   int iRet;
   for (; i < dwNum; i++) {
      iRet = _wcsicmp(pap[i]->m_szName, pszName);
      if (iRet < 0)
         continue;   // move on
      if (iRet > 0)
         break;   // went too high

      // else match, only care about non-delete
      if (pap[i]->m_dwType == 0) {
         pInfo->dwType = pap[i]->m_dwInfoType;
         pInfo->fDefLowRank = pap[i]->m_fDefLowRank;
         pInfo->fDefPassUp = pap[i]->m_fDefPassUp;
         pInfo->fMax = pap[i]->m_fMax;
         pInfo->fMin = pap[i]->m_fMin;
         pInfo->pszDescription = ((pap[i]->m_memInfo.p) && (((PWSTR)pap[i]->m_memInfo.p)[0])) ?
            (PWSTR)(pap[i]->m_memInfo.p) : (PWSTR)NULL;

         // BUGFIX - Because attribute info location might change depending upon
         // the location of the object, call in to the object
         PCOEATTRIBCOMBO pc;
         DWORD dwFind;
         PCObjectSocket pos;
         pos = NULL;
         dwFind = -1;
         pc = (PCOEATTRIBCOMBO) pap[i]->m_lCOEATTRIBCOMBO.Get(0);
         if (pc)
            dwFind = pwc->pWorld->ObjectFind (&pc->gFromObject);
         if (dwFind != -1)
            pos = pwc->pWorld->ObjectGet (dwFind);
         pInfo->pLoc.Zero();
         if (pos) {
            ATTRIBINFO ai;
            if (pos->AttribInfo (pc->szName, &ai))
               pInfo->pLoc.Copy (&ai.pLoc);

            CMatrix m;
            pos->ObjectMatrixGet (&m);
            pInfo->pLoc.p[3] = 1;
            pInfo->pLoc.MultiplyLeft (&m);
         }
         pInfo->pLoc.p[3] = 1;
         pInfo->pLoc.MultiplyLeft (&pwc->mRot);
         return TRUE;
      }
   }

   // else, not found
   return FALSE;
}



/**********************************************************************************
ObjEditUseThisInstance - Should be called before an instance object does a render,
or anything that would require the master world to be reset to the instance's attributes.

This does:
   - If pWC.gLastInstanceUsed == pgObject then ignore
   - Else, do ObjAttribSetGroup forom PCListFixed of ATTRIBVAL

inputs
   POBJWORLDCACHE    pWC - The CListFixed of COEAttrib, CWorldSocket are used from here.
                        Additionally, the gLastInstanceUsed is changed to the current
                        guid.
   GUID              *pgObject - Instance GUID of the object setting it
   PCObjectSocket    pos - Object pointer also
   PCListFixed       plATTRIBVAL - List of attributes stored by the instance
   PCMem             pMemSort - For scratch memory optimizations
   PCListFixed       plChange - For scratch memory optimizations
returns
   none
*/
void ObjEditUseThisInstance (POBJWORLDCACHE pWC, GUID *pgObject, PCObjectSocket pos,
                             PCListFixed plATTRIBVAL,
                             PCMem pMemSort, PCListFixed plChange)
{
   if (IsEqualGUID(pWC->gLastInstanceUsed, *pgObject) && (pWC->pLastInstanceUsed == pos))
      return;  // same object as last time

   // else, new values
   ObjEditAttribSetGroup (pWC, pgObject, pos, plATTRIBVAL->Num(), (PATTRIBVAL) plATTRIBVAL->Get(0), FALSE,
      pMemSort, plChange);
}


/**********************************************************************************
ObjEditInstanceChanged - Call this when the instance attributes have changed.
It doesnt send the changes immediately to the world cache, BUT if the world cache's
las gLastInstanceUsed == pgObject, then gLastInstanceUsed is set to 0.

inputs
   POBJWORLDCACHE    pWC - The CListFixed of COEAttrib, CWorldSocket are used from here.
                        Additionally, the gLastInstanceUsed is changed to the current
                        guid.
   GUID              *pgObject - Instance GUID of the object setting it
returns
   none
*/
void ObjEditInstanceChanged (POBJWORLDCACHE pWC, GUID *pgObject)
{
   if (IsEqualGUID(pWC->gLastInstanceUsed, *pgObject)) {
      memset (&pWC->gLastInstanceUsed, 0, sizeof(pWC->gLastInstanceUsed));
      pWC->pLastInstanceUsed = NULL;
   }
}



/*********************************************************************************
ObjEditBonesFind - Called by ObjEditAnalyzeCacheItem() when a new object is created.
Searches through all the objects in the world and finds the bones object (if any).
Once there, it then proceeds to create mirror information for each bone. And
determine bone tranform matricies.

inputs
   POBJWORLDCACHE    pwc - Filled within info except bones
returns
   none
*/
void ObjEditBonesFind (POBJWORLDCACHE pwc)
{
   // Find bone
   pwc->pObjectBone = NULL;
   memset (&pwc->gObjectBone, 0, sizeof(pwc->gObjectBone));

   DWORD i;
   PCObjectSocket pos;
   OSMBONE osb;
   memset (&osb, 0 ,sizeof(osb));
   for (i = 0; i < pwc->pWorld->ObjectNum(); i++) {
      pos = pwc->pWorld->ObjectGet (i);
      if (!pos)
         continue;
   
      if (pos->Message (OSM_BONE, &osb) && osb.pb) {
         pwc->pObjectBone = osb.pb;
         break;
      }
   }
   if (!pwc->pObjectBone)
      return;  // not found
   pwc->pObjectBone->GUIDGet (&pwc->gObjectBone);
   pwc->pObjectBone->m_fSkeltonForOE = TRUE; // set flag so knows to export different CP

   if (!pwc->pObjectBone->ObjEditBoneSetup (TRUE, &pwc->mRot)) {
      pwc->pObjectBone = NULL;
      return;
   }
}


/**********************************************************************************
ObjEditAnalyzeCacheItem - This is an internal function that's called once a cache
item has been created for the world. It analyszes some of the data in the word
and updates some POBJWORLDCACHE info.

inputs
   POBJWORLDCACHE    pwc - The pWorld, gCode, gSub, and dwCount is filled in. The rest is generally not
retursn
   none
*/
void ObjEditAnalyzeCacheItem (POBJWORLDCACHE pwc)
{
   PWSTR psz;
   DWORD dw;

   // get the render show information
   psz = pwc->pWorld->VariableGet (WSObjType());
   dw = psz ? (DWORD)_wtoi(psz) : 0;
   if (!dw)
      dw = RENDERSHOW_FURNITURE;
   pwc->dwRenderShow = dw;

   // location
   psz = pwc->pWorld->VariableGet (WSObjLocation());
   pwc->dwLocation = psz ? (DWORD)_wtoi(psz) : 0;

   // embeddinginfo
   psz = pwc->pWorld->VariableGet (WSObjEmbed());
   pwc->dwEmbed = psz ? (DWORD)_wtoi(psz) : 0;

   // create the rotation matrix
   pwc->mRot.Identity();
   if (pwc->dwEmbed) switch (pwc->dwLocation) {
      case 0:  // floor
      default:
         pwc->mRot.RotationX (PI / 2);
         break;
      case 1:  // wall
         // do nothing
         break;
      case 2:  // ceiling
         pwc->mRot.RotationX (-PI / 2);
         break;
   }

   if (pwc->dwRenderDepth < MAXRENDERDEPTH) {
      // make sure that not recursing
      pwc->dwRenderDepth++;

      if (pwc->plCOEAttrib)
         ObjEditClearPCOEAttribList (pwc->plCOEAttrib);
      else {
         pwc->plCOEAttrib = new CListFixed;
         if (pwc->plCOEAttrib)
            pwc->plCOEAttrib->Init (sizeof(PCOEAttrib));
      }
      memset (&pwc->gLastInstanceUsed, 0, sizeof(pwc->gLastInstanceUsed));
      pwc->pLastInstanceUsed = NULL;
      if (pwc->plCOEAttrib)
         ObjEditCreatePCOEAttribList (pwc->pWorld, pwc->plCOEAttrib);

      // find the bones
      ObjEditBonesFind (pwc);

      pwc->dwRenderDepth--;
   }  // pwc->dwRenderDepth


   if (!pwc->fLoading) {
      // determine the cutout
      if (pwc->plCutout)
         delete pwc->plCutout;
      pwc->plCutout = NULL;

      if (pwc->dwEmbed >= 2) {
         DWORD dwPlane = (pwc->dwLocation == 1) ? 1 : 2;
         BOOL fFlip = (pwc->dwLocation != 0);
         pwc->plCutout = OutlineFromPoints (pwc->pWorld, dwPlane, fFlip);
      }


      // attributes exported
      if (pwc->plCPEXPORT)
         delete pwc->plCPEXPORT;
      pwc->plCPEXPORT = NULL;
      DWORD i, j;
      PCObjectSocket pos;
      CListFixed l;
      l.Init (sizeof(DWORD));
      DWORD *padw;
      OSCONTROL Info;
      CPEXPORT Export;
      memset (&Export, 0, sizeof(Export));
      for (i = 0; i < pwc->pWorld->ObjectNum(); i++) {
         pos = pwc->pWorld->ObjectGet (i);
         if (!pos)
            continue;

         // get the items
         l.Clear();
         pos->ControlPointEnum (&l);
         padw = (DWORD*) l.Get(0);

         // over all the cp's
         // NOTE: Need to get CPs AFTER get bones since will want CP for bones
         for (j = 0; j < l.Num(); j++) {
            if (!pos->ControlPointQuery (padw[j], &Info))
               continue;

            // if export then remember
            if (Info.fPassUp) {
               Export.dwID = padw[j];
               Export.pos = pos;
               if (!pwc->plCPEXPORT) {
                  pwc->plCPEXPORT = new CListFixed;
                  if (pwc->plCPEXPORT)
                     pwc->plCPEXPORT->Init (sizeof(CPEXPORT));
               }
               if (pwc->plCPEXPORT)
                  pwc->plCPEXPORT->Add (&Export);
            }
         }  // j, cp id's
      }  // i
   } // if not loading

}


/**********************************************************************************
ObjEditReloadCache - Given that an object class has changed, this reloads it into
the object cache.

inputs
   GUID        *pgCode - Major code
   GUID        *pgSub - Minor code
returns
   BOOL - TRUE if was cached
*/
BOOL ObjEditReloadCache (DWORD dwRenderShard, GUID *pgCode, GUID *pgSub)
{
   DWORD i;
   POBJWORLDCACHE pwc;
   for (i = 0; i < galOBJWORLDCACHE[dwRenderShard].Num(); i++) {
      pwc = (POBJWORLDCACHE) galOBJWORLDCACHE[dwRenderShard].Get(i);
      if (IsEqualGUID (*pgSub, pwc->gSub) && IsEqualGUID (*pgCode, pwc->gCode))
         break;
   }
   if (i >= galOBJWORLDCACHE[dwRenderShard].Num())
      return FALSE;
   if (pwc->fLoading)
      return FALSE;

   PCMMLNode2 pNode;
   pNode = LibraryObjects(dwRenderShard, FALSE)->ItemGet (pgCode, pgSub);
   if (!pNode)
      pNode = LibraryObjects(dwRenderShard, TRUE)->ItemGet (pgCode, pgSub);
   if (!pNode)
      return FALSE;

   BOOL fFailedToLoad;
   pwc->fLoading = TRUE;
   pwc->pWorld->MMLFrom (pNode, &fFailedToLoad);
   pwc->fLoading = FALSE;

   ObjEditAnalyzeCacheItem (pwc);

   return TRUE;
}

/**********************************************************************************
CObjectEditor::Constructor and destructor */
CObjectEditor::CObjectEditor (PVOID pParams, POSINFO pInfo)
{
   // tell the remapper to use this
   m_Remap.m_pEdit = this;
   m_pObjectRender = NULL;
   m_lATTRIBVAL.Init (sizeof(ATTRIBVAL));
   m_dwRenderShard = pInfo->dwRenderShard;

   m_mObjectTemp.Identity();

   // store this away in the cache
   DWORD i;
   POBJWORLDCACHE pwc;
   for (i = 0; i < galOBJWORLDCACHE[m_dwRenderShard].Num(); i++) {
      pwc = (POBJWORLDCACHE) galOBJWORLDCACHE[m_dwRenderShard].Get(i);
      if (IsEqualGUID (pInfo->gSub, pwc->gSub) && IsEqualGUID (pInfo->gCode, pwc->gCode))
         break;
   }
   if (i >= galOBJWORLDCACHE[m_dwRenderShard].Num()) {
      // make a new one
      PCWorld pWorld;
      pwc = NULL;
      pWorld = new CWorld;
      if (!pWorld)
         goto skip;
      pWorld->m_fKeepUndo = FALSE;  // so dont keep undo/redo around
      pWorld->RenderShardSet (pInfo->dwRenderShard);
      PCMMLNode2 pNode;
      pNode = LibraryObjects(m_dwRenderShard, FALSE)->ItemGet (&pInfo->gCode, &pInfo->gSub);
      if (!pNode)
         pNode = LibraryObjects(m_dwRenderShard, TRUE)->ItemGet (&pInfo->gCode, &pInfo->gSub);
      if (!pNode) {
         delete pWorld;
         goto skip;
      }

      OBJWORLDCACHE wc;
      memset (&wc, 0, sizeof(wc));
      wc.dwCount = 0;
      wc.gCode = pInfo->gCode;
      wc.gSub = pInfo->gSub;
      wc.pWorld = pWorld;
      wc.fLoading = TRUE;

      galOBJWORLDCACHE[m_dwRenderShard].Add (&wc, sizeof(wc));
      pwc = (POBJWORLDCACHE) galOBJWORLDCACHE[m_dwRenderShard].Get(galOBJWORLDCACHE[m_dwRenderShard].Num()-1);

      // BUGFIX - Move the loading of the world until AFTER it has ben added to the
      // cache so that it the world references an object that references the world
      // will only end up with one world loaded
      BOOL fFailedToLoad;
      if (!pWorld->MMLFrom (pNode, &fFailedToLoad))
         fFailedToLoad = TRUE;
      // ignore failed to load
      pWorld->UndoClear(TRUE, TRUE);
      pWorld->m_fDirty = FALSE;
      pwc->fLoading = FALSE;


      // extract some other information from it
      ObjEditAnalyzeCacheItem (pwc);
   }

   // abort if loading
   if (pwc->fLoading)
      pwc = NULL;

   // increment usage count
   if (pwc)
      pwc->dwCount++;

skip:
   m_pObjectWorld = pwc ? pwc->pWorld : NULL;
   m_dwRenderShow = pwc ? pwc->dwRenderShow : NULL;
   m_pOBJWORLDCACHE = pwc;
   m_fCanBeEmbedded = FALSE;
   if (pwc && pwc->dwEmbed)
      m_fCanBeEmbedded = TRUE;

   // get all the defaults
   m_lATTRIBVAL.Clear ();
   ObjEditAttribGetAll (pwc->plCOEAttrib, TRUE, FALSE, TRUE, &m_lATTRIBVAL);

   // NOTE: Only setting this at first. If user changes the object while it's
   // visible the object's m_dwRenderShow won't be changed, but this isn't
   // that big a deal

   m_OSINFO = *pInfo;

}


CObjectEditor::~CObjectEditor (void)
{
   // only deal with if remember m_pOBJWORLDCACHE)
   if (m_pOBJWORLDCACHE) {

      // release reference count
      DWORD i;
      POBJWORLDCACHE pwc;
      for (i = 0; i < galOBJWORLDCACHE[m_dwRenderShard].Num(); i++) {
         pwc = (POBJWORLDCACHE) galOBJWORLDCACHE[m_dwRenderShard].Get(i);
         if (pwc == (POBJWORLDCACHE) m_pOBJWORLDCACHE) {
            pwc->dwCount--;
            if (!pwc->dwCount) {
               // BUGFIX - Because deleting an object may delete sub-objects make a copy of this
               OBJWORLDCACHE wc = *pwc;
               pwc = &wc;

               // move this up
               galOBJWORLDCACHE[m_dwRenderShard].Remove (i);

               delete pwc->pWorld;
               pwc->pWorld = NULL;
               if (pwc->plCOEAttrib) {
                  ObjEditClearPCOEAttribList (pwc->plCOEAttrib);
                  delete pwc->plCOEAttrib;
                  pwc->plCOEAttrib = NULL;
               }
               if (pwc->plCutout) {
                  delete pwc->plCutout;
                  pwc->plCutout = NULL;
               }
               if (pwc->plCPEXPORT) {
                  delete pwc->plCPEXPORT;
                  pwc->plCPEXPORT = NULL;
               }

               // BUFIX - Had ObjEditBonesClear(), but dont call here because
               // will be done when delete bone object, plus if put here
               // gets called twice and causes crash
               //if (pwc->pObjectBone)
               //   pwc->pObjectBone->ObjEditBonesClear ();
            }
            break;
         }
      }
   }  // if m_pOBJWORLDDACHE

}


/**********************************************************************************
CObjectEditor::Delete - Called to delete this object
*/
void CObjectEditor::Delete (void)
{
   delete this;
}

/**********************************************************************************
CObjectEditor::Render - Draws a test box.

inputs
   POBJECTRENDER     pr - Render information
*/
void CObjectEditor::Render (POBJECTRENDER pr, DWORD dwSubObject)
{
   if (!m_pObjectWorld || !m_pOBJWORLDCACHE)
      return;

   // just update the classification just inccase
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;

   // update the attributes
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   m_dwRenderShow = pwc->dwRenderShow;
   m_fCanBeEmbedded = (pwc->dwEmbed ? TRUE : FALSE);

   if (pwc->dwRenderDepth > MAXRENDERDEPTH)
      return;  // dont go there since recursion
   pwc->dwRenderDepth++;

   m_pObjectRender = pr;
   if (m_pWorld) {
      m_SSRemap.m_pMaster = m_pWorld->SurfaceSchemeGet();
      m_SSRemap.m_pOrig = m_pObjectWorld->m_pOrigSurfaceScheme;
   }
   m_pObjectWorld->SurfaceSchemeSet (&m_SSRemap);


   // loop through all the objects
   DWORD i;
   PCObjectSocket pos;
   for (i = 0; i < m_pObjectWorld->ObjectNum(); i++) {
      pos = m_pObjectWorld->ObjectGet (i);

      // If it's a bone then dont draw
      if (pos == pwc->pObjectBone)
         continue;

      // calculate the objects that affect this one
      if (pwc->pObjectBone)
         pwc->pObjectBone->ObjEditBonesTestObject (i);

      // create a matrix that includes rotation/translation for this object
      ObjectMatrixGet (&m_mObjectTemp);
      m_mObjectTemp.MultiplyLeft (&pwc->mRot); // so deals with embedding
      pr->pRS->MatrixSet (&m_mObjectTemp);

      // draw
      OBJECTRENDER or;
      memset (&or, 0, sizeof(or));
      or.dwReason = pr->dwReason;
      or.dwShow = -1;
      or.pRS = &m_Remap;
      pos->Render (&or, (DWORD)-1);
   }

   m_pObjectWorld->SurfaceSchemeSet (NULL);
   pwc->dwRenderDepth--;
}



/**********************************************************************************
CObjectEditor::QueryBoundingBox - Standard API
*/
void CObjectEditor::QueryBoundingBox (PCPoint pCorner1, PCPoint pCorner2, DWORD dwSubObject)
{
   if (!m_pObjectWorld || !m_pOBJWORLDCACHE) {
      pCorner1->Zero();
      pCorner2->Zero();
      return;
   }

   // just update the classification just inccase
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;

   // update the attributes
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   //m_dwRenderShow = pwc->dwRenderShow;
   m_fCanBeEmbedded = (pwc->dwEmbed ? TRUE : FALSE);

   if (pwc->dwRenderDepth > MAXRENDERDEPTH) {
      pCorner1->Zero();
      pCorner2->Zero();
      return;  // dont go there since recursion
   }
   pwc->dwRenderDepth++;

   m_pObjectRender = NULL; // pr;
   //if (m_pWorld) {
   //   m_SSRemap.m_pMaster = m_pWorld->SurfaceSchemeGet();
   //   m_SSRemap.m_pOrig = m_pObjectWorld->m_pOrigSurfaceScheme;
   //}
   //m_pObjectWorld->SurfaceSchemeSet (&m_SSRemap);


   // loop through all the objects
   DWORD i;
   PCObjectSocket pos;
   CMatrix mTemp, mObject;
   BOOL fSet = FALSE;
   CPoint p1, p2;
   for (i = 0; i < m_pObjectWorld->ObjectNum(); i++) {
      pos = m_pObjectWorld->ObjectGet (i);

      // If it's a bone then dont draw
      if (pos == pwc->pObjectBone)
         continue;

      // calculate the objects that affect this one
      if (pwc->pObjectBone)
         pwc->pObjectBone->ObjEditBonesTestObject (i);

      // get this bounding box
      pos->QueryBoundingBox (&p1, &p2, (DWORD)-1);
      pos->ObjectMatrixGet (&mObject);

      // rotate
      mTemp.Multiply (&pwc->mRot, &mObject);
      BoundingBoxApplyMatrix (&p1, &p2, &mTemp);

      // set
      if (fSet) {
         pCorner1->Min (&p1);
         pCorner2->Max (&p2);
      }
      else {
         pCorner1->Copy (&p1);
         pCorner2->Copy (&p2);
         fSet = TRUE;
      }

   }
   if (!fSet) {
      pCorner1->Zero();
      pCorner2->Zero();
   }

   //m_pObjectWorld->SurfaceSchemeSet (NULL);
   pwc->dwRenderDepth--;

#ifdef _DEBUG
   // test, make sure bounding box not too small
   //CPoint p1,p2;
   //DWORD i;
   CObjectTemplate::QueryBoundingBox (&p1, &p2, dwSubObject);
   for (i = 0; i < 3; i++)
      if ((p1.p[i] < pCorner1->p[i] - CLOSE) || (p2.p[i] > pCorner2->p[i] + CLOSE))
         break;
   if (i < 3)
      OutputDebugString ("\r\nCObjectEditor::QueryBoundingBox too small.");
#endif
}

/**********************************************************************************
CObjectEditor::Clone - Clones this

inputs
   none
returns
   PCObjectSocket - Clone
*/
PCObjectSocket CObjectEditor::Clone (void)
{
   PCObjectEditor pNew;

   pNew = new CObjectEditor(NULL, &m_OSINFO);
   // dont need to increase object count since the new() function will do it

   // clone template info
   CloneTemplate(pNew);

   // lone remap
   m_SSRemap.CloneTo (&pNew->m_SSRemap);

   pNew->m_lATTRIBVAL.Init (sizeof(ATTRIBVAL), m_lATTRIBVAL.Get(0), m_lATTRIBVAL.Num());

   return pNew;
}

static PWSTR gpszSSRemap = L"SSRemap";

PCMMLNode2 CObjectEditor::MMLTo (void)
{
   // call into the template first
   PCMMLNode2 pNode = MMLToTemplate();
   if (!pNode)
      return NULL;

   // write out only the attributes that are different than original
   CListFixed lDiff;
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);
   lDiff.Init (sizeof(ATTRIBVAL));
   ObjEditAttribGetAll (pwc->plCOEAttrib, FALSE, TRUE, TRUE, &lDiff);
   WCHAR szTemp[64];
   DWORD i;
   PATTRIBVAL pav;
   pav = (PATTRIBVAL) lDiff.Get(0);
   for (i = 0; i < lDiff.Num(); i++) {
      swprintf (szTemp, L"Atts%d", (int) i);
      MMLValueSet (pNode, szTemp, pav[i].szName);
      swprintf (szTemp, L"Attv%d", (int) i);
      MMLValueSet (pNode, szTemp, pav[i].fValue);
   }

   // write out the remap
   PCMMLNode2 pSub;
   pSub = m_SSRemap.RemapMMLTo ();
   if (pSub) {
      pSub->NameSet (gpszSSRemap);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

BOOL CObjectEditor::MMLFrom (PCMMLNode2 pNode)
{
   // call into the template first
   if (!MMLFromTemplate (pNode))
      return FALSE;


   // write out only the attributes that are different than original
   CListFixed lDiff;
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   lDiff.Init (sizeof(ATTRIBVAL));
   WCHAR szTemp[64];
   DWORD i;
   PWSTR psz;
   ATTRIBVAL av;
   memset (&av,0,sizeof(av));
   for (i = 0; ; i++) {
      swprintf (szTemp, L"Atts%d", (int) i);
      psz = MMLValueGet (pNode, szTemp);
      if (!psz)
         break;
      wcscpy (av.szName, psz);

      swprintf (szTemp, L"Attv%d", (int) i);
      av.fValue = MMLValueGetDouble (pNode, szTemp, 0);

      lDiff.Add (&av);
   }
   if (lDiff.Num()) {
      // set the current batch
      ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
         &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

      // changes on top of that
      ObjEditAttribSetGroup (pwc, &m_gGUID, this, lDiff.Num(), (PATTRIBVAL) lDiff.Get(0), FALSE,
         &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

      // restore into current list
      m_lATTRIBVAL.Clear();
      ObjEditAttribGetAll (pwc->plCOEAttrib, FALSE, FALSE, TRUE, &m_lATTRIBVAL);
   }

   PCMMLNode2 pSub;
   pSub = NULL;
   pNode->ContentEnum (pNode->ContentFind (gpszSSRemap), &psz, &pSub);
   if (pSub)
      m_SSRemap.RemapMMLFrom (pSub);

   return TRUE;
}


/******************************************************************************
CObjectEditor::SurfaceGet - Trap the call and see what kind of surfaces are
listed in CSSRemap. Use that.
*/
PCObjectSurface CObjectEditor::SurfaceGet (DWORD dwID)
{
   if (!dwID)
      return NULL;
   PWSTR psz;
   psz = m_SSRemap.m_tSchemesQuerried.Enum (dwID-1);
   if (!psz)
      return NULL;   // was never querried

   // get the info
   DWORD dwRemap;
   PCObjectSurface pos;
   dwRemap = m_SSRemap.RemapFind (psz);
   if (dwRemap == (DWORD)-1)
      pos = (m_pWorld->SurfaceSchemeGet())->SurfaceGet (psz, NULL, FALSE);
   else
      pos = m_SSRemap.RemapGetSurface (dwRemap);
   if (!pos)
      return NULL;

   // set the ID
   pos->m_dwID = dwID;
   return pos;

}

/******************************************************************************
CObjectEditor::SurfaceSet - Trap this call so we remember the remap.
*/
BOOL CObjectEditor::SurfaceSet (PCObjectSurface pos)
{
   PWSTR psz;
   psz = m_SSRemap.m_tSchemesQuerried.Enum (pos->m_dwID-1);
   if (!psz)
      return FALSE;   // was never querried

   // obejct about to change
   if (m_pWorld)
      m_pWorld->ObjectAboutToChange (this);

   // copy it over
   m_SSRemap.RemapAdd (psz, pos);

   // call into world object and say that matrix has changed
   if (m_pWorld)
      m_pWorld->ObjectChanged (this);

   // done
   return TRUE;
}
/**********************************************************************************
CObjectEditor::ObjectClassQuery
asks the curent object what other objects (including itself) it requires
so that when a file is saved, all user objects will be saved along with
the file, so people on other machines can load them in.
The object just ADDS (doesn't clear or remove) elements, which are two
guids in a row: gCode followed by gSub of the object. All objects
must add at least on (their own). Some, like CObjectEditor, will add
all its sub-objects too
*/
BOOL CObjectEditor::ObjectClassQuery (PCListFixed plObj)
{
   GUID ag[2];
   GUID *pag;
   OSINFO OI;

   // add own. make sure not there already
   DWORD j;
   InfoGet (&OI);
   ag[0] = OI.gCode;
   ag[1] = OI.gSub;
   for (j = 0; j < plObj->Num(); j++) {
      pag = (GUID*) plObj->Get(j);
      if (IsEqualGUID (ag[1], pag[1]) && IsEqualGUID (ag[0], pag[0]))
         break;
   }
   if (j < plObj->Num())
      return TRUE;   // already there
   plObj->Add (ag);

   // look through the contents
   if (!m_pObjectWorld)
      return TRUE;   // no contents
   // ask them
   DWORD i;
   for (i = 0; i < m_pObjectWorld->ObjectNum(); i++) {
      PCObjectSocket pos = m_pObjectWorld->ObjectGet(i);

      pos->ObjectClassQuery (plObj);
   }

   return TRUE;
}



/******************************************************************************
CObjectEditor::TextureQuery - Trap this. Just call into the objects in the
world and have them add their bit.
*/
BOOL CObjectEditor::TextureQuery (PCListFixed plText)
{
   if (!m_pObjectWorld || !m_pOBJWORLDCACHE)
      return FALSE;

   // make sure dont recurse
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (pwc->dwRenderDepth > MAXRENDERDEPTH)
      return TRUE;  // dont go there since recursion
   pwc->dwRenderDepth++;

   // make sure remap is sert up
   if (m_pWorld) {
      m_SSRemap.m_pMaster = m_pWorld->SurfaceSchemeGet();
      m_SSRemap.m_pOrig = m_pObjectWorld->m_pOrigSurfaceScheme;
   }
   m_pObjectWorld->SurfaceSchemeSet (&m_SSRemap);

   // ask them
   DWORD i;
   for (i = 0; i < m_pObjectWorld->ObjectNum(); i++) {
      PCObjectSocket pos = m_pObjectWorld->ObjectGet(i);

      pos->TextureQuery (plText);
   }

   m_pObjectWorld->SurfaceSchemeSet (NULL);
   pwc->dwRenderDepth--;

   return TRUE;
}

/******************************************************************************
CObjectEditor::ColorQuery - Trap this. Just call into the objects in the
world and have them add their bit.
*/
BOOL CObjectEditor::ColorQuery (PCListFixed plColor)
{
   if (!m_pObjectWorld || !m_pOBJWORLDCACHE)
      return FALSE;

   // make sure dont recurse
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (pwc->dwRenderDepth > MAXRENDERDEPTH)
      return TRUE;  // dont go there since recursion
   pwc->dwRenderDepth++;

   // make sure remap is sert up
   if (m_pWorld) {
      m_SSRemap.m_pMaster = m_pWorld->SurfaceSchemeGet();
      m_SSRemap.m_pOrig = m_pObjectWorld->m_pOrigSurfaceScheme;
   }
   m_pObjectWorld->SurfaceSchemeSet (&m_SSRemap);

   // ask them
   DWORD i;
   for (i = 0; i < m_pObjectWorld->ObjectNum(); i++) {
      PCObjectSocket pos = m_pObjectWorld->ObjectGet(i);

      pos->ColorQuery (plColor);
   }

   m_pObjectWorld->SurfaceSchemeSet (NULL);
   pwc->dwRenderDepth--;

   return TRUE;
}



/**************************************************************************************
CObjectEditor::MoveReferencePointQuery - 
given a move reference index, this fill in pp with the position of
the move reference RELATIVE to ObjectMatrixGet. References are numbers
from 0+. If the index is more than the number of points then the
function returns FALSE

inputs
   DWORD       dwIndex - index.0 .. # ref
   PCPoint     pp - Filled with point relative to ObjectMatrixGet() IF its valid
returns
   BOOL - TRUE if valid index.
*/
BOOL CObjectEditor::MoveReferencePointQuery (DWORD dwIndex, PCPoint pp)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   POBJWORLDCACHE pwc;

   // if bones use
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (pwc->pObjectBone && pwc->pObjectBone->m_plBoneMove && pwc->pObjectBone->m_plBoneMove->Num()) {
      // use this instance
      ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
         &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

      // have bones, so use them as movmeent reference
      if (dwIndex >= pwc->pObjectBone->m_plBoneMove->Num())
         return FALSE;

      PCBone pb;
      pb = *((PCBone*) pwc->pObjectBone->m_plBoneMove->Get(dwIndex));
      pp->Copy (&pb->m_pEndOS);
      pp->p[3] = 1;
      CMatrix m;
      pwc->pObjectBone->ObjectMatrixGet (&m);
      pp->MultiplyLeft (&m);
      pp->MultiplyLeft (&pwc->mRot);   // incase orientation different
      return TRUE;
   }

   ps = gaObjectMove;
   dwDataSize = sizeof(gaObjectMove);

   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP))
      return FALSE;

   // always at 0,0 in ObjectEditor
   pp->Zero();
   return TRUE;
}

/**************************************************************************************
CObjectEditor::MoveReferenceStringQuery -
given a move reference index (numbered from 0 to the number of references)
this fills in a string at psz and dwSize BYTES that names the move reference
to the end user. *pdwNeeded is filled with the number of bytes needed for
the string. Returns FALSE if dwIndex is too high, or dwSize is too small (although
pdwNeeded will be filled in)

inputs
   DWORD       dwIndex - index. 0.. # ref
   PWSTR       psz - To be filled in witht he string
   DWORD       dwSize - # of bytes available in psz
   DWORD       *pdwNeeded - If not NULL, filled with the size needed
returns
   BOOL - TRUE if psz copied.
*/
BOOL CObjectEditor::MoveReferenceStringQuery (DWORD dwIndex, PWSTR psz, DWORD dwSize, DWORD *pdwNeeded)
{
   PSPLINEMOVEP ps;
   DWORD dwDataSize;
   DWORD dwNeeded;

   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (pwc->pObjectBone && pwc->pObjectBone->m_plBoneMove && pwc->pObjectBone->m_plBoneMove->Num()) {
      // have bones, so use them as movmeent reference
      if (dwIndex >= pwc->pObjectBone->m_plBoneMove->Num()) {
         if (pdwNeeded)
            *pdwNeeded = 0;
         return FALSE;
      }

      PCBone pb;
      pb = *((PCBone*) pwc->pObjectBone->m_plBoneMove->Get(dwIndex));
      dwNeeded = ((DWORD)wcslen (pb->m_szName) + 1) * 2;
      if (pdwNeeded)
         *pdwNeeded = dwNeeded;
      if (dwNeeded <= dwSize) {
         wcscpy (psz, pb->m_szName);
         return TRUE;
      }
      else
         return FALSE;
   }

   ps = gaObjectMove;
   dwDataSize = sizeof(gaObjectMove);
   if (dwIndex >= dwDataSize / sizeof(SPLINEMOVEP)) {
      if (pdwNeeded)
         *pdwNeeded = 0;
      return FALSE;
   }

   dwNeeded = ((DWORD)wcslen (ps[dwIndex].pszName) + 1) * 2;
   if (pdwNeeded)
      *pdwNeeded = dwNeeded;
   if (dwNeeded <= dwSize) {
      wcscpy (psz, ps[dwIndex].pszName);
      return TRUE;
   }
   else
      return FALSE;
}


/**********************************************************************************
CObjectEditor::TurnOnGet - 
returns how TurnOn the object is, from 0 (closed) to 1.0 (TurnOn), or
< 0 for an object that can't be TurnOned
*/
fp CObjectEditor::TurnOnGet (void)
{
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (!pwc)
      return -1;
   // set the current batch
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   fp f;
   if (ObjEditAttribGet (pwc->plCOEAttrib, AttribOnOff(), &f))
      return f;
   else
      return -1;
}

/**********************************************************************************
CObjectEditor::TurnOnSet - 
TurnOns/closes the object. if fTurnOn==0 it's close, 1.0 = TurnOn, and
values in between are partially TurnOned closed. Returne TRUE if success
*/
BOOL CObjectEditor::TurnOnSet (fp fTurnOn)
{
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (!pwc)
      return FALSE;
   // set the current batch
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   fp f;
   if (!ObjEditAttribGet (pwc->plCOEAttrib, AttribOnOff(), &f))
      return FALSE;  // doesn't support

   // set
   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));
   wcscpy (av.szName, AttribOnOff());
   fTurnOn = max(0,fTurnOn);
   fTurnOn = min(1,fTurnOn);
   av.fValue = fTurnOn;

   m_pWorld->ObjectAboutToChange (this);

   // set it
   ObjEditAttribSetGroup (pwc, &m_gGUID, this, 1, &av, FALSE,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   // restore attributes
   m_lATTRIBVAL.Clear();
   ObjEditAttribGetAll (pwc->plCOEAttrib, FALSE, FALSE, TRUE, &m_lATTRIBVAL);

   m_pWorld->ObjectChanged (this);

   return TRUE;
}


/**********************************************************************************
CObjectEditor::OpenGet - 
returns how Open the object is, from 0 (closed) to 1.0 (Open), or
< 0 for an object that can't be Opened
*/
fp CObjectEditor::OpenGet (void)
{
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (!pwc)
      return -1;
   // set the current batch
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   fp f;
   if (ObjEditAttribGet (pwc->plCOEAttrib, AttribOpenClose(), &f))
      return f;
   else
      return -1;
}

/**********************************************************************************
CObjectEditor::OpenSet - 
Opens/closes the object. if fOpen==0 it's close, 1.0 = Open, and
values in between are partially Opened closed. Returne TRUE if success
*/
BOOL CObjectEditor::OpenSet (fp fOpen)
{
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (!pwc)
      return FALSE;
   // set the current batch
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   fp f;
   if (!ObjEditAttribGet (pwc->plCOEAttrib, AttribOpenClose(), &f))
      return FALSE;  // doesn't support

   // set
   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));
   wcscpy (av.szName, AttribOpenClose());
   fOpen = max(0,fOpen);
   fOpen = min(1,fOpen);
   av.fValue = fOpen;

   m_pWorld->ObjectAboutToChange (this);

   // set it
   ObjEditAttribSetGroup (pwc, &m_gGUID, this, 1, &av, FALSE,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   // restore attributes
   m_lATTRIBVAL.Clear();
   ObjEditAttribGetAll (pwc->plCOEAttrib, FALSE, FALSE, TRUE, &m_lATTRIBVAL);

   m_pWorld->ObjectChanged (this);

   return TRUE;
}


/********************************************************************************
CObjectEditor::LightQuery -
ask the object if it has any lights. If it does, pl is added to (it
is already initialized to sizeof LIGHTINFO) with one or more LIGHTINFO
structures. Coordinates are in OBJECT SPACE.
*/
BOOL CObjectEditor::LightQuery (PCListFixed pl, DWORD dwShow)
{
   if (!m_pObjectWorld || !m_pOBJWORLDCACHE)
      return FALSE;

   // just update the classification just inccase
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   m_dwRenderShow = pwc->dwRenderShow;

   // make sure dont recurse
   if (pwc->dwRenderDepth > MAXRENDERDEPTH)
      return TRUE;  // dont go there since recursion
   pwc->dwRenderDepth++;

   // update the attributes
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   // tell things to turn on/off
   if (m_pWorld) {
      m_SSRemap.m_pMaster = m_pWorld->SurfaceSchemeGet();
      m_SSRemap.m_pOrig = m_pObjectWorld->m_pOrigSurfaceScheme;
   }
   m_pObjectWorld->SurfaceSchemeSet (&m_SSRemap);

   // loop through all the objects
   DWORD i;
   PCObjectSocket pos;

   pwc->dwRenderDepth--;

   // make sure want to see lights
   if (!(dwShow & m_dwRenderShow)) {
      m_pObjectWorld->SurfaceSchemeSet (NULL);
      return FALSE;
   }

   // make sure dont recurse
   if (pwc->dwRenderDepth > MAXRENDERDEPTH) {
      m_pObjectWorld->SurfaceSchemeSet (NULL);
      return TRUE;  // dont go there since recursion
   }
   pwc->dwRenderDepth++;

   // loop through all the objects and ask them if they supprot lights
   for (i = 0; i < m_pObjectWorld->ObjectNum(); i++) {
      pos = m_pObjectWorld->ObjectGet (i);
      DWORD dwNum;
      dwNum = pl->Num();
      pos->LightQuery(pl, -1);   // show everything

      if (pl->Num() <= dwNum)
         continue;   // no change

      // get the matrix and invert for vector
      CMatrix m, mInv;
      pos->ObjectMatrixGet (&m);
      m.MultiplyLeft (&pwc->mRot); // so deals with embedding
      m.Invert (&mInv);
      mInv.Transpose(); // can use for vectors

      // loop through each of these ans translate from the given world to the new one
      DWORD j;
      for (j = dwNum; j < pl->Num(); j++) {
         PLIGHTINFO pli = (PLIGHTINFO) pl->Get(j);
         pli->pDir.p[3] = pli->pLoc.p[3] = 1;
         pli->pLoc.MultiplyLeft (&m);
         pli->pDir.MultiplyLeft (&mInv);
      }
   }

   m_pObjectWorld->SurfaceSchemeSet (NULL);
   pwc->dwRenderDepth--;

   return TRUE;
}



/**********************************************************************************
CObjectEditor::EmbedDoCutout - Member function specific to the template. Called
when the object has moved within the surface. This enables the super-class for
the embedded object to pass a cutout into the container. (Basically, specify the
hole for the window or door)
*/
BOOL CObjectEditor::EmbedDoCutout (void)
{
   // find the surface
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (!pwc || !pwc->plCutout)
      return FALSE;
   GUID gCont;
   PCObjectSocket pos;
   if (!m_pWorld || !EmbedContainerGet (&gCont))
      return FALSE;
   pos = m_pWorld->ObjectGet (m_pWorld->ObjectFind (&gCont));
   if (!pos)
      return FALSE;

   // will need to transform from this object space into the container space
   CMatrix mCont, mTrans;
   pos->ObjectMatrixGet (&mCont);
   mCont.Invert4 (&mTrans);
   mTrans.MultiplyLeft (&m_MatrixObject);
   mTrans.MultiplyLeft (&pwc->mRot);

   CPoint pDelta;
   CMatrix mRotInv;
   //pwc->mRot.Invert (&mRotInv);
   mRotInv.Copy (&pwc->mRot);
   mRotInv.Transpose();
   pDelta.Zero();
   pDelta.p[1] = .01;
   pDelta.p[3] = 1;
   pDelta.MultiplyLeft (&mRotInv);



   CListFixed lFront, lBack;
   DWORD dwNum;
   dwNum = pwc->plCutout->Num();
   lFront.Init (sizeof(CPoint), pwc->plCutout->Get(0), dwNum);
   lBack.Init (sizeof(CPoint), pwc->plCutout->Get(0), dwNum);

   PCPoint pFront, pBack;
   pFront = (PCPoint) lFront.Get(0);
   pBack = (PCPoint) lBack.Get(0);

   // convert to object's space
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      pFront[i].p[3] = 1;
      pFront[i].MultiplyLeft (&mTrans);

      pBack[i].Add (&pDelta);
      pBack[i].p[3] = 1;
      pBack[i].MultiplyLeft (&mTrans);
   }
   pos->ContCutout (&m_gGUID, dwNum, pFront, pBack, (pwc->dwEmbed >= 3));

   return TRUE;
}


/*********************************************************************************
COERemap - So that callbacks for the rendering of the world get filtered through.
*/
COERemap::COERemap (void)
{
   m_pEdit = NULL;
   m_mMatrixSet.Identity();
   memset (m_adwRecurse, 0, sizeof(m_adwRecurse));
}

COERemap::~COERemap (void)
{
   // do nothing, for now
}


BOOL COERemap::QueryWantNormals (void)
{
   if (m_adwRecurse[0])
      return FALSE;

   BOOL fRet;
   m_adwRecurse[0]++;
   fRet = m_pEdit->m_pObjectRender->pRS->QueryWantNormals();
   m_adwRecurse[0]--;
   return fRet;
}

BOOL COERemap::QueryWantTextures (void)
{
   if (m_adwRecurse[1])
      return FALSE;

   BOOL fRet;
   m_adwRecurse[1]++;
   fRet = m_pEdit->m_pObjectRender->pRS->QueryWantTextures();
   m_adwRecurse[1]--;
   return fRet;
}



/******************************************************************************
COERemap::QueryCloneRender - From CRenderSocket
*/
BOOL COERemap::QueryCloneRender (void)
{
   // if using bones then dont support clone render because the sub-objects will
   // be affected by the bones
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pEdit->m_pOBJWORLDCACHE;
   if (pwc && pwc->pObjectBone && pwc->pObjectBone->m_plBoneAffect && pwc->pObjectBone->m_plBoneAffect->Num())
      return FALSE;

   if (m_adwRecurse[3])
      return FALSE;

   m_adwRecurse[3]++;

   BOOL fRet;
   fRet = m_pEdit->m_pObjectRender->pRS->QueryCloneRender();
   m_adwRecurse[3]--;
   return fRet;
}


/******************************************************************************
COERemap::CloneRender - From CRenderSocket
*/
BOOL COERemap::CloneRender (GUID *pgCode, GUID *pgSub, DWORD dwNum, PCMatrix pamMatrix)
{
   // if using bones then dont support clone render because the sub-objects will
   // be affected by the bones
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pEdit->m_pOBJWORLDCACHE;
   if (pwc && pwc->pObjectBone && pwc->pObjectBone->m_plBoneAffect && pwc->pObjectBone->m_plBoneAffect->Num())
      return FALSE;

   if (m_adwRecurse[3])
      return FALSE;

   m_adwRecurse[3]++;

   // create scratch matricies
   // CListFixed lm;
   // changed lm to m_lCloneRenderm. might be recursion problem but doesn't look like it
   DWORD i;
   PCMatrix pm;
	MALLOCOPT_INIT;
	MALLOCOPT_OKTOMALLOC;
   m_lCloneRenderm.Init (sizeof(CMatrix), pamMatrix, dwNum);
	MALLOCOPT_RESTORE;
   pm = (PCMatrix) m_lCloneRenderm.Get(0);

   CMatrix m;
   m_pEdit->ObjectMatrixGet (&m);
   if (pwc)
      m.MultiplyLeft (&pwc->mRot); // so deals with embedding

   // else apply own matrix
   for (i = 0; i < m_lCloneRenderm.Num(); i++, pm++)
      pm->MultiplyRight (&m);

   BOOL fRet;
   fRet = m_pEdit->m_pObjectRender->pRS->CloneRender (pgCode, pgSub, m_lCloneRenderm.Num(), (PCMatrix)m_lCloneRenderm.Get(0));

   m_adwRecurse[3]--;
   return fRet;
}

/******************************************************************************
COERemap::QuerySubDetail - From CRenderSocket. Basically end up ignoring
*/
BOOL COERemap::QuerySubDetail (PCMatrix pMatrix, PCPoint pBound1, PCPoint pBound2, fp *pfDetail)
{
   *pfDetail = QueryDetail();
   return TRUE;
}

fp COERemap::QueryDetail (void)
{
   if (m_adwRecurse[2])
      return 1;   // error

   fp fRet;
   m_adwRecurse[2]++;
   fRet = m_pEdit->m_pObjectRender->pRS->QueryDetail();
   m_adwRecurse[2]--;
   return fRet;
}

void COERemap::MatrixSet (PCMatrix pm)
{
   if (m_adwRecurse[3])
      return;

   m_adwRecurse[3]++;
   // create a matrix that includes rotation/translation for this object
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pEdit->m_pOBJWORLDCACHE;
   m_mMatrixSet.Copy (pm); // BUGFIX - Always remember m_MatrixSet, get rid of the following
#if 0
   if (pwc && pwc->pObjectBone && pwc->pObjectBone->m_plBoneAffect && pwc->pObjectBone->m_plBoneAffect->Num()) {
      // dont pass matrix set right down because want to capture it and use the info
      // for deformations
   }
   else {
      m_pEdit->ObjectMatrixGet (&m_pEdit->m_mObjectTemp);
      if (pwc)
         m_pEdit->m_mObjectTemp.MultiplyLeft (&pwc->mRot); // so deals with embedding
      m_pEdit->m_mObjectTemp.MultiplyLeft (pm);
      m_pEdit->m_pObjectRender->pRS->MatrixSet (&m_pEdit->m_mObjectTemp);
   }
#endif
   m_adwRecurse[3]--;
}

void COERemap::PolyRender (PPOLYRENDERINFO pInfo)
{
   if (m_adwRecurse[4])
      return;

   m_adwRecurse[4]++;
   // loop through all the surfaces and remap the IDs
   DWORD i;
   for (i = 0; i < pInfo->dwNumSurfaces; i++) {
      PRENDERSURFACE prs = &pInfo->paSurfaces[i];
      if (prs->szScheme[0]) {
         DWORD *pdwFind = (DWORD*) m_pEdit->m_SSRemap.m_tSchemesQuerried.Find(prs->szScheme);
         if (pdwFind)
            prs->wMinorID = (WORD) (*pdwFind) + 1; // add once to scheme number
         else
            prs->wMinorID = 0;   // since not linked to a scheme that know
      }
      else
         prs->wMinorID = 0;   // since not linked 5o a scheme
   }

   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pEdit->m_pOBJWORLDCACHE;
   // BUGFIX - Need bonesalreadyapplied to be FALSE
   if (pwc->pObjectBone && !pInfo->fBonesAlreadyApplied && pwc->pObjectBone->m_plBoneAffect && pwc->pObjectBone->m_plBoneAffect->Num())
      pwc->pObjectBone->ObjEditBoneRender (pInfo, m_pEdit->m_pObjectRender->pRS, &m_mMatrixSet);
   else {
      // BUGFIX - Do matrix set here. If not then broken if fBonesAlreadyApplied
      // is set to TRUE
      m_pEdit->ObjectMatrixGet (&m_pEdit->m_mObjectTemp);
      if (pwc)
         m_pEdit->m_mObjectTemp.MultiplyLeft (&pwc->mRot); // so deals with embedding
      m_pEdit->m_mObjectTemp.MultiplyLeft (&m_mMatrixSet);
      m_pEdit->m_pObjectRender->pRS->MatrixSet (&m_pEdit->m_mObjectTemp);

      m_pEdit->m_pObjectRender->pRS->PolyRender (pInfo);
   }

   m_adwRecurse[4]--;
}





/*********************************************************************************
CSSRemap - Remaps calls from the objects in m_pObjectWorld when they get
the CSurfaceSchemeSocket and call into SurfaceGet(). Uses the remap data to
intercept the calls. This also keeps tracks of calls to SurfaceGet() so they
can be used for surface IDs later on.
*/

CSSRemap::CSSRemap (void)
{
   m_pMaster = NULL;
   m_pOrig = NULL;
   m_dwRecurse = 0;
}

CSSRemap::~CSSRemap (void)
{
   RemapClear ();
}

// from CSurfaceSchemeSocket
PCMMLNode2 CSSRemap::MMLTo (void)
{
   if (m_dwRecurse > MAXRENDERDEPTH)
      return NULL;

   // just pass it on
   PCMMLNode2 pNode;
   m_dwRecurse++;
   pNode = m_pMaster ? m_pMaster->MMLTo() : NULL;
   m_dwRecurse--;
   return pNode;
}

BOOL CSSRemap::MMLFrom (PCMMLNode2 pNode)
{
   if (m_dwRecurse > MAXRENDERDEPTH)
      return NULL;

   // just pass it on
   BOOL fRet;
   m_dwRecurse++;
   fRet = m_pMaster ? m_pMaster->MMLFrom(pNode) : FALSE; 
   m_dwRecurse--;
   return fRet;
}

PCObjectSurface CSSRemap::SurfaceGet (PWSTR pszScheme, PCObjectSurface pDefault, BOOL fNoClone)
{
   if (m_dwRecurse > MAXRENDERDEPTH)
      return NULL;

   if (!pszScheme)
      return NULL;

   // keep track of this
   if (!m_tSchemesQuerried.Find(pszScheme)) {
      DWORD dw = m_tSchemesQuerried.Num();
      m_tSchemesQuerried.Add (pszScheme, &dw, sizeof(dw));  // keep pointed to its ID #
   }

   // see if there's a remap of this
   PCObjectSurface *ppos;
   PCObjectSurface pos, pos2;
   ppos = (PCObjectSurface*) m_tRemap.Find (pszScheme);
   if (ppos) {
      pos = *ppos;
      if (pos->m_szScheme[0]) {
         m_dwRecurse++;
         pos2 = m_pMaster ? m_pMaster->SurfaceGet (pos->m_szScheme, pos, fNoClone) : NULL;  // remapped, but to a sub-scheme
         m_dwRecurse--;
         return pos2;
      }
      else
         return fNoClone ? pos : pos->Clone(); // remapped it to this one
   }

   // else, no remapping, so pass through
   pos = NULL;
   if (!m_pMaster)
      return NULL;   // shouldnt happen but it might
   m_dwRecurse++;
   pos = m_pMaster->SurfaceGet (pszScheme, NULL, fNoClone);
   m_dwRecurse--;
   if (pos)
      return pos;

   // BUGFIX - if no scheme information set up in the master, and there's no
   // remap, then go back to the original schemes for the world
   if (m_pOrig)
      pos = m_pOrig->SurfaceGet (pszScheme, NULL, fNoClone);
   if (pos) {
      // set the scheme in the master also
      PCObjectSurface pos2;
      m_dwRecurse++;
      pos2 = m_pMaster->SurfaceGet (pszScheme, pos, fNoClone);
      m_dwRecurse--;
      if (!fNoClone)
         delete pos;
      return pos2;
   }

   // else, pass in the default, that was passed into us, since is neither in
   // the master or the original
   m_dwRecurse++;
   pos2 = m_pMaster->SurfaceGet (pszScheme, pDefault, fNoClone);
   m_dwRecurse--;
   return pos2;
}



BOOL CSSRemap::SurfaceExists (PWSTR pszScheme)
{
   if (m_dwRecurse > MAXRENDERDEPTH)
      return NULL;

   if (!pszScheme)
      return NULL;

   // keep track of this
   if (!m_tSchemesQuerried.Find(pszScheme)) {
      DWORD dw = m_tSchemesQuerried.Num();
      m_tSchemesQuerried.Add (pszScheme, &dw, sizeof(dw));  // keep pointed to its ID #
   }

   // see if there's a remap of this
   PCObjectSurface *ppos;
   PCObjectSurface pos;
   BOOL fRet;
   ppos = (PCObjectSurface*) m_tRemap.Find (pszScheme);
   if (ppos) {
      pos = *ppos;
      if (pos->m_szScheme[0]) {
         m_dwRecurse++;
         fRet = m_pMaster ? m_pMaster->SurfaceExists (pos->m_szScheme) : FALSE;  // remapped, but to a sub-scheme
         m_dwRecurse--;
         return fRet;
      }
      else
         return TRUE;
   }

   // else, no remapping, so pass through
   if (!m_pMaster)
      return FALSE;   // shouldnt happen but it might
   m_dwRecurse++;
   fRet = m_pMaster->SurfaceExists (pszScheme);
   m_dwRecurse--;
   if (fRet)
      return fRet;

   // BUGFIX - if no scheme information set up in the master, and there's no
   // remap, then go back to the original schemes for the world
   fRet = FALSE;
   if (m_pOrig)
      fRet = m_pOrig->SurfaceExists (pszScheme);
   if (fRet)
      return TRUE;

   // else, pass in the default, that was passed into us, since is neither in
   // the master or the original
   m_dwRecurse++;
   fRet = m_pMaster->SurfaceExists (pszScheme);
   m_dwRecurse--;
   return fRet;
}


BOOL CSSRemap::SurfaceSet (PCObjectSurface pSurface)
{
   if (m_dwRecurse > MAXRENDERDEPTH)
      return FALSE;

   // dont expect to be called, so passs through
   BOOL fRet;
   m_dwRecurse++;
   fRet = m_pMaster ? m_pMaster->SurfaceSet (pSurface) : FALSE;
   m_dwRecurse--;
   return fRet;
}

BOOL CSSRemap::SurfaceRemove (PWSTR pszScheme)
{
   if (m_dwRecurse > MAXRENDERDEPTH)
      return FALSE;

   // dont expect to be called, so passs through
   BOOL fRet;
   m_dwRecurse++;
   fRet = m_pMaster ? m_pMaster->SurfaceRemove (pszScheme) : FALSE;
   m_dwRecurse--;
   return fRet;
}

PCObjectSurface CSSRemap::SurfaceEnum (DWORD dwIndex)
{
   if (m_dwRecurse > MAXRENDERDEPTH)
      return FALSE;

   // dont expect to be called, so passs through
   // NOTE: If this is called then will return the wrong info
   PCObjectSurface pRet;
   m_dwRecurse++;
   pRet = m_pMaster ? m_pMaster->SurfaceEnum (dwIndex) : NULL;
   m_dwRecurse--;
   return pRet;
}

void CSSRemap::WorldSet (CWorldSocket *pWorld)
{
   if (m_dwRecurse > MAXRENDERDEPTH)
      return;

   // dont expect to be called, so passs through
   m_dwRecurse++;
   if (m_pMaster)
      m_pMaster->WorldSet (pWorld);
   m_dwRecurse--;
}

void CSSRemap::Clear (void)
{
   if (m_dwRecurse > MAXRENDERDEPTH)
      return;

   // dont expect to be called, so passs through
   m_dwRecurse++;
   if (m_pMaster)
      m_pMaster->Clear ();
   m_dwRecurse--;
}

/******************************************************************************
CSSRemap::RemapNum - Returns the number of scheme remappings in the remap object
*/
DWORD CSSRemap::RemapNum (void)
{
   return m_tRemap.Num();
}

/******************************************************************************
CSSRemap::RemapGetSurface - Given a remap index number, this returns the the
PCObjectSurface for the map. NOTE: This is a clone of the one stored away, so
the caller WILL HAVE TO free it.

inputs
   DWORD       dwNum - Index number from 0..RemapNum()-1
retursn
   PCObjectSurface - Object surface descriptor that's remapped to. Must be freed
*/
PCObjectSurface CSSRemap::RemapGetSurface (DWORD dwNum)
{
   PWSTR psz = m_tRemap.Enum (dwNum);
   if (!psz)
      return NULL;
   PCObjectSurface *ppos;
   ppos = (PCObjectSurface*) m_tRemap.Find (psz);
   if (!ppos)
      return NULL;
   return (*ppos)->Clone();
}

/******************************************************************************
CSSRemap::RemapGetName - Given a remap index number, this returns the
string for the remap name.

inputs
   DWORD       dwNum - Index number from 0..RemapNum()-1
retursn
   PWSTR - String for the name. This is only valid until the name is removed.
*/
PWSTR CSSRemap::RemapGetName (DWORD dwNum)
{
   return m_tRemap.Enum(dwNum);
}

/******************************************************************************
CSSRemap::RemapRemove - Given a remap index number, this deletes the item

inputs
   DWORD       dwNum - Index number from 0..RemapNum()-1
retursn
   BOOL - TRUE if found and delteted
*/
BOOL CSSRemap::RemapRemove (DWORD dwNum)
{
   PWSTR psz = m_tRemap.Enum (dwNum);
   if (!psz)
      return FALSE;
   PCObjectSurface *ppos;
   ppos = (PCObjectSurface*) m_tRemap.Find (psz);
   if (!ppos)
      return FALSE;
   delete (*ppos);

   m_tRemap.Remove (psz);
   return TRUE;
}

/******************************************************************************
CSSRemap::RemapAdd - Adds a new remap to the object. If the mapping already exists
it is first removed.

inputs
   PWSTR       pszName - Name of the remap
   PCObjectSurface pSurface - A copy if made of this and added
retursn
   BOOL - TRUE if success
*/
BOOL CSSRemap::RemapAdd (PWSTR pszName, PCObjectSurface pSurface)
{
   DWORD dwNum = RemapFind (pszName);
   if (dwNum != -1)
      RemapRemove (dwNum);

   PCObjectSurface pNew;
   pNew = pSurface->Clone();
   m_tRemap.Add (pszName, &pNew, sizeof(pNew));
   return TRUE;
}

/******************************************************************************
CSSRemap::RemapClear - Deletes everything in the remap list
*/
void CSSRemap::RemapClear (void)
{
   // free the objects
   DWORD i;
   PCObjectSurface pos;
   for (i = 0; i < m_tRemap.Num(); i++) {
      PWSTR psz = m_tRemap.Enum(i);
      pos = *((PCObjectSurface*) m_tRemap.Find (psz));
      delete pos;
   }
   m_tRemap.Clear();
}

/******************************************************************************
CSSRemap::RemapFind - Returns the index into the remap given a string.
Returns -1 if can't find.
*/
DWORD CSSRemap::RemapFind (PWSTR psz)
{
   if (!m_tRemap.Find(psz))
      return -1;

   DWORD i;
   PWSTR pszFind;
   for (i = 0; i < m_tRemap.Num(); i++) {
      pszFind = m_tRemap.Enum (i);

      if (!_wcsicmp(psz, pszFind))
         return i;
   }
   return -1;
}

/******************************************************************************
CSSRemap::CloneTo - Clones from this remap object to pNew
*/
void CSSRemap::CloneTo (CSSRemap *pNew)
{
   pNew->Clear();

   pNew->m_pMaster = m_pMaster;

   // clone the remap
   DWORD i;
   PCObjectSurface pos;
   PWSTR psz;
   for (i = 0; i < m_tRemap.Num(); i++) {
      psz = m_tRemap.Enum(i);
      pos = *((PCObjectSurface*) m_tRemap.Find (psz));

      pNew->RemapAdd (psz, pos);
   }

   // clonet he tree of requested schemes
   pNew->m_tSchemesQuerried.Clear();
   for (i = 0; i <m_tSchemesQuerried.Num(); i++) {
      psz = m_tSchemesQuerried.Enum(i);
      pNew->m_tSchemesQuerried.Add (psz, &i, sizeof(i)); // keep pointer the same ID #
   }
}

static PWSTR gpszRemap = L"Remap";
static PWSTR gpszRemapElem = L"RemapElem";
static PWSTR gpszRemapName = L"RemapName";

/******************************************************************************
CSSRemap::RemapMMLTo - Writes all the remap information out to MML
*/
PCMMLNode2 CSSRemap::RemapMMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszRemap);

   // get them
   DWORD i;
   PCObjectSurface pos;
   PCMMLNode2 pSub;
   for (i = 0; i < m_tRemap.Num(); i++) {
      PWSTR psz = m_tRemap.Enum(i);
      pos = *((PCObjectSurface*) m_tRemap.Find (psz));

      pSub = pos->MMLTo ();
      if (!pSub)
         continue;
      MMLValueSet (pSub, gpszRemapName, psz);

      pSub->NameSet (gpszRemapElem);
      pNode->ContentAdd (pSub);
   }

   return pNode;
}

/******************************************************************************
CSSRemap::RemapFrom - Reads all the remap information out to MML
*/
BOOL CSSRemap::RemapMMLFrom (PCMMLNode2 pNode)
{
   // clear it first
   RemapClear ();

   DWORD i;
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
      if (!_wcsicmp(psz, gpszRemapElem)) {
         PCObjectSurface pos = new CObjectSurface;
         if (!pos)
            return FALSE;
         if (!pos->MMLFrom (pSub)) {
            delete pos;
            return FALSE;
         }

         psz = MMLValueGet (pSub, gpszRemapName);
         if (!psz || m_tRemap.Find(psz)) {
            delete pos;
            continue;
         }

         // add it
         m_tRemap.Add (psz, &pos, sizeof(pos));
      }
   }

   return TRUE;
}


/*****************************************************************************************
CObjectEditor::AttribGetIntern - OVERRIDE THIS

Like AttribGet() except that only called if default attributes not handled.
*/
BOOL CObjectEditor::AttribGetIntern (PWSTR pszName, fp *pfValue)
{
   if (IsTemplateAttribute (pszName, TRUE))
      return FALSE;

   // just update the classification just inccase
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;

   // update the attributes
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   // get it
   return ObjEditAttribGet (pwc->plCOEAttrib, pszName, pfValue);
}


/*****************************************************************************************
CObjectEditor::AttribGetAllIntern - OVERRIDE THIS

Like AttribGetAllIntern() EXCEPT plATTRIBVAL is already initialized and filled with
some parameters (default to the object template)
*/
void CObjectEditor::AttribGetAllIntern (PCListFixed plATTRIBVAL)
{
   // just update the classification just inccase
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;

   // update the attributes
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   // get it
   return ObjEditAttribGetAll (pwc->plCOEAttrib, FALSE, FALSE, FALSE, plATTRIBVAL);
}


/*****************************************************************************************
CObjectEditor::AttribSetGroupIntern - OVERRIDE THIS

Like AttribSetGroup() except passing on non-template attributes.
*/
void CObjectEditor::AttribSetGroupIntern (DWORD dwNum, PATTRIBVAL paAttrib)
{
   // just update the classification just inccase
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;

   // update the attributes
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   // pass in set
   m_pWorld->ObjectAboutToChange (this);

   // set it
   BOOL fBoneAttrChanged;
   fBoneAttrChanged = ObjEditAttribSetGroup (pwc, &m_gGUID, this, dwNum, paAttrib, TRUE,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   // restore attributes
   m_lATTRIBVAL.Clear();
   ObjEditAttribGetAll (pwc->plCOEAttrib, FALSE, FALSE, TRUE, &m_lATTRIBVAL);

   m_pWorld->ObjectChanged (this);

   // if a bone attribute changed then need to inform all attached objects so they
   // move also
   if (fBoneAttrChanged)
      TellAttachThatMoved ();
}


/*****************************************************************************************
CObjectEditor::AttribInfoIntern - OVERRIDE THIS

Like AttribInfo() except called if attribute is not for template.
*/
BOOL CObjectEditor::AttribInfoIntern (PWSTR pszName, PATTRIBINFO pInfo)
{
   // just update the classification just inccase
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   
   return ObjEditAttribInfo (pwc, pwc->plCOEAttrib, pszName, pInfo);
}




/**********************************************************************************
CObjectEditor::ControlPointQuery - Standard CP Query
*/
BOOL CObjectEditor::ControlPointQuery (DWORD dwID, POSCONTROL pInfo)
{
   // just update the classification just inccase
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (!pwc->plCPEXPORT)
      return FALSE;
   if (dwID >= pwc->plCPEXPORT->Num())
      return FALSE;

   // update the attributes
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   // find the one
   PCPEXPORT pc;
   pc = (PCPEXPORT) pwc->plCPEXPORT->Get(dwID);

   // pass it down
   if (!pc->pos->ControlPointQuery (pc->dwID, pInfo))
      return FALSE;

   // fix uo
   CMatrix mObject;
   pInfo->dwID = dwID;  // overwrite
   pc->pos->ObjectMatrixGet (&mObject);
   pInfo->pLocation.p[3] = 1;
   pInfo->pLocation.MultiplyLeft (&mObject);

   // may need to account for a bone affecting this
   CMatrix m;
   if (pwc->pObjectBone && pwc->pObjectBone->ObjEditBonesRigidCP (pc->pos, &m))
      pInfo->pLocation.MultiplyLeft (&m);

   // done
   return TRUE;
}

/**********************************************************************************
CObjectEditor::ControlPointSet - Standard CP Query
*/
BOOL CObjectEditor::ControlPointSet (DWORD dwID, PCPoint pVal, PCPoint pViewer)
{
   // just update the classification just inccase
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (!pwc->plCPEXPORT)
      return FALSE;
   if (dwID >= pwc->plCPEXPORT->Num())
      return FALSE;

   // update the attributes
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   // find the one
   PCPEXPORT pc;
   pc = (PCPEXPORT) pwc->plCPEXPORT->Get(dwID);

   // convert the points
   CPoint pNewVal, pNewViewer;
   CMatrix mObject, mInv;
   pNewVal.Copy (pVal);
   pNewVal.p[3] = 1;

   if (pViewer) {
      pNewViewer.Copy (pViewer);
      pNewViewer.p[3] = 1;
   }

   // may need to account for a bone affecting this
   CMatrix m;
   if (pwc->pObjectBone && pwc->pObjectBone->ObjEditBonesRigidCP (pc->pos, &m)) {
      m.Invert4 (&mInv);

      pNewVal.MultiplyLeft (&mInv);
      if (pViewer)
         pNewViewer.MultiplyLeft (&mInv);
   }

   // BUGFIX - Reverse multiplication of ObjEditBonesRigidCP and mInv
   pc->pos->ObjectMatrixGet (&mObject);
   mObject.Invert4 (&mInv);
   pNewVal.MultiplyLeft (&mInv);
   if (pViewer)
      pNewViewer.MultiplyLeft (&mInv);

   // object iD
   GUID gObject;
   PCObjectSocket pos;
   pc->pos->GUIDGet (&gObject);
   pos = pc->pos;

   // assume that changing this CP will only affect this object, which is usually 99.9% correct
   // find all the attributes that might be affected by a change to this object
   CListFixed lCheck, lOrig;
   DWORD i, j;
   lCheck.Init (sizeof(PCOEAttrib));
   lOrig.Init (sizeof(ATTRIBVAL));
   ATTRIBVAL av;
   memset (&av, 0, sizeof(av));
   PCOEAttrib *ppa;
   PCOEAttrib pa;
   if (pwc->plCOEAttrib) {
      ppa = (PCOEAttrib*) pwc->plCOEAttrib->Get(0);
      for (i = 0; i < pwc->plCOEAttrib->Num(); i++) {
         pa = ppa[i];
         if (pa->m_dwType == 1)
            continue;   // this is an unmap

         // if multiple attributes are affected with one go then don't use because
         // will just be a problem later.
         if (pa->m_lCOEATTRIBCOMBO.Num() != 1)
            continue;

         // see what it affects
         PCOEATTRIBCOMBO pc;
         pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
         for (j = 0; j < pa->m_lCOEATTRIBCOMBO.Num(); j++, pc++) {
            if (!IsEqualGUID(pc->gFromObject, gObject))
               continue;   // guids not the same
            
            // if complex relationship don't even bother
            if (pc->dwNumRemap != 2)
               continue;

            // get the attribute at the same time
            wcscpy (av.szName, pc->szName);
            pos->AttribGet (av.szName, &av.fValue);

            // have a relationship, so add... NOTE: Hope there is only one copy or
            // will affect the attribute twice
            lOrig.Add (&av);
            lCheck.Add (&pa);
         } // j - all attributes
      }  // i
   } // have attributes

   // pass it down
   if (!pc->pos->ControlPointSet (pc->dwID, &pNewVal, pViewer ? &pNewViewer : NULL))
      return FALSE;

   BOOL fSentChange;
   fSentChange = FALSE;

   // try and figure out what changed
   ppa = (PCOEAttrib*) lCheck.Get(0);
   PATTRIBVAL pav;
   pav = (PATTRIBVAL) lOrig.Get(0);
   for (i = 0; i < lOrig.Num(); i++) {
      pa = ppa[i];
      fp fVal;
      if (!pos->AttribGet (pav[i].szName, &fVal))
         continue;   // error, so assume no change

      // if values the same then ignore
      if (fVal == pav[i].fValue)
         continue;

      // else, have change, so find the delta
      fVal -= pav[i].fValue;

      // get the one that changed this
      PCOEATTRIBCOMBO pc;
      pc = (PCOEATTRIBCOMBO) pa->m_lCOEATTRIBCOMBO.Get(0);
      // know there's only one COEATTRIBCOMBO and that it affects this attribute
      // AND that it only has two elements
      
      // delta adjust
      fp fDeltaObj, fDeltaCombo;
      fDeltaObj = pc->afpObjectValue[1] - pc->afpObjectValue[0];
      fDeltaCombo = pc->afpComboValue[1] - pc->afpComboValue[0];
      if (!fDeltaObj)
         continue;   // no change
      fVal = (fVal / fDeltaObj) * fDeltaCombo;

      // find this attribute in our list
      PATTRIBVAL pFind;
      wcscpy (av.szName, pa->m_szName);
      pFind = (PATTRIBVAL) bsearch (&av, m_lATTRIBVAL.Get(0), m_lATTRIBVAL.Num(), sizeof(ATTRIBVAL), ATTRIBVALCompare);
      if (!pFind)
         continue;

      // send change
      if (!fSentChange) {
         fSentChange = TRUE;
         m_pWorld->ObjectAboutToChange (this);
      }
      // write it out
      pFind->fValue += fVal;
   }

   // end change
   if (fSentChange) {
      m_pWorld->ObjectChanged (this);

      // just so the user doesn't get confused if mess up attributes
      ObjEditInstanceChanged (pwc, &m_gGUID);
      ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
         &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);
   }

   // if the object changed was the bones then will need to move embeeded
   if (pos == pwc->pObjectBone) {
      // since moving the CP may have require a move of the object, look into this
      CPoint pZero;
      pZero.Zero();
      if (!pZero.AreClose (&pwc->pObjectBone->m_pIKTrans)) {
         // matrix that changes this translation amount into object space
         CMatrix m, mInvTrans;
         pwc->pObjectBone->ObjectMatrixGet (&m);
         m.MultiplyRight (&pwc->mRot);
         m.Invert (&mInvTrans);
         mInvTrans.Transpose ();

         CPoint pMove;
         pMove.Copy (&pwc->pObjectBone->m_pIKTrans);
         pwc->pObjectBone->m_pIKTrans.Zero();
         pMove.p[3] = 1;
#ifdef _DEBUG
         char szTemp[64];
         sprintf (szTemp, "IK move Pre: %g,%g,%g\r\n", (double) pMove.p[0],
            (double) pMove.p[1],(double) pMove.p[2]);
         OutputDebugString (szTemp);
#endif
         pMove.MultiplyLeft (&mInvTrans);

         // apply translation to self
         m.Translation (pMove.p[0], pMove.p[1], pMove.p[2]);
#ifdef _DEBUG
         sprintf (szTemp, "IK move %g,%g,%g\r\n", (double) pMove.p[0],
            (double) pMove.p[1],(double) pMove.p[2]);
         OutputDebugString (szTemp);
#endif
         m.MultiplyRight (&m_MatrixObject);
         ObjectMatrixSet (&m);
      }
      else
         // also, tell attached that have moved
         // dont need to call this if did the object matrix set
         TellAttachThatMoved ();
   }

   // done
   return TRUE;
}



/**********************************************************************************
CObjectEditor::ControlPointEnum - Standard CP Query
*/
void CObjectEditor::ControlPointEnum (PCListFixed plDWORD)
{
   // just update the classification just inccase
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (!pwc->plCPEXPORT)
      return;

   DWORD i;
   plDWORD->Required (plDWORD->Num() + pwc->plCPEXPORT->Num());
   for (i = 0; i < pwc->plCPEXPORT->Num(); i++)
      plDWORD->Add (&i);
}



/*****************************************************************************************
CObjectEditor::AttachClosest
Given a point (pLoc) in the object's coords, returns a name (max 64 characters incl null)
and stores it in pszClosest. This name can then be used for adding an attachment
In the case of attaching to on object with bones, this will be the bone name

USUALLY OVERRIDDEN
*/
BOOL CObjectEditor::AttachClosest (PCPoint pLoc, PWSTR pszClosest)
{
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;

   // default behavour is to fill in with an empty string
   pszClosest[0] = 0;
   if (!pwc->pObjectBone)
      return TRUE;   // no bone, so nothing to do

   // update the attributes
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   // convert to the bone's coords
   CMatrix m, mInv;
   CPoint pBone;
   pwc->pObjectBone->ObjectMatrixGet (&m);
   m.MultiplyRight (&pwc->mRot);   // incase orientation different
   m.Invert4 (&mInv);
   pBone.Copy (pLoc);
   pBone.p[3] = 1;
   pBone.MultiplyLeft (&mInv);

   // find the closest
   DWORD dwClosest, i;
   PCBone *ppa;
   fp fClosest, f;
   CPoint pDist;
   dwClosest = -1;
   ppa = (PCBone*) pwc->pObjectBone->m_lBoneList.Get(0);
   for (i = 0; i < pwc->pObjectBone->m_lBoneList.Num(); i++) {
      pDist.Subtract (&pBone, &ppa[i]->m_pEndOS);
      f = pDist.Length();
      if ((dwClosest == -1) || (f < fClosest)) {
         dwClosest = i;
         fClosest = f;
      }
   }
   if (dwClosest == -1)
      return TRUE;   // no bones, so use NULL

   // else, this
   wcscpy (pszClosest, ppa[dwClosest]->m_szName);

   return TRUE;
}


/*****************************************************************************************
CObjectEditor::AttachPointsEnum
Given a point (pLoc) in the object's coords, returns a name (max 64 characters incl null)
and stores it in pszClosest. This name can then be used for adding an attachment
In the case of attaching to on object with bones, this will be the bone name

USUALLY OVERRIDDEN
*/
void CObjectEditor::AttachPointsEnum (PCListVariable plEnum)
{
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;

   // default behavour is to fill in with an empty string
   plEnum->Clear();
   if (!pwc->pObjectBone)
      return;   // no bone, so nothing to do

   // update the attributes
   ObjEditUseThisInstance (pwc, &m_gGUID, this, &m_lATTRIBVAL,
      &m_memObjEditAttribSetGroupSort, &m_lObjEditAttribSetGroupChange);

   // find the closest
   DWORD i;
   PCBone *ppa;
   ppa = (PCBone*) pwc->pObjectBone->m_lBoneList.Get(0);
   for (i = 0; i < pwc->pObjectBone->m_lBoneList.Num(); i++)
      plEnum->Add (ppa[i]->m_szName, (wcslen(ppa[i]->m_szName)+1)*sizeof(WCHAR));
}

/*****************************************************************************************
CObjectEditor::InternAttachMatrix - The superclass object should override this. What it
does is take an attachment name (such as a bone name). This returns a matrix that converts from
bone space into object space, taking into account the movement of the bone.

USUALLY OVERRIDDEN
*/
void CObjectEditor::InternAttachMatrix (PWSTR pszAttach, PCMatrix pmAttachRot)
{
   POBJWORLDCACHE pwc;
   pwc = (POBJWORLDCACHE) m_pOBJWORLDCACHE;
   if (!pwc->pObjectBone) {
      pmAttachRot->Identity();
      return;
   }

   // find the bone
   DWORD i;
   PCBone *ppa;
   ppa = (PCBone*) pwc->pObjectBone->m_lBoneList.Get(0);
   for (i = 0; i < pwc->pObjectBone->m_lBoneList.Num(); i++) {
      if (!_wcsicmp(ppa[i]->m_szName, pszAttach))
         break;
   }
   if (i >= pwc->pObjectBone->m_lBoneList.Num()) {
      // not found, so just use identity
      pmAttachRot->Identity();

      return;
   }

   // post bone
   CPoint pLen;
   PCBone pBone;
   CMatrix mTrans;
   pBone = ppa[i];
   pLen.Subtract (&pBone->m_pEndOS, &pBone->m_pStartOS);
   mTrans.Identity();
   if (fabs( pBone->m_pMotionDef.p[3]) > EPSILON)
      mTrans.Translation (pLen.Length() * pBone->m_pMotionCur.p[3] / pBone->m_pMotionDef.p[3]/2, 0, 0);  // go half way out as is standard
   // undo half the twist that's stored in m_mRender
   if (fabs(pBone->m_pMotionCur.p[2] - pBone->m_pMotionDef.p[2]) > CLOSE) {
      CMatrix mRot;
      mRot.RotationX (-0.5 * (pBone->m_fFlipLR ? -1 : 1) * (pBone->m_pMotionCur.p[2] - pBone->m_pMotionDef.p[2]));
      mTrans.MultiplyRight (&mRot);
   }
   pmAttachRot->Multiply (&pBone->m_mRender, &mTrans);
   pmAttachRot->MultiplyRight (&pBone->m_mCalcOS); // so gets to starting point for bone

   // by this point go from bone to bone object space
   CMatrix m;
   pwc->pObjectBone->ObjectMatrixGet (&m);
   pmAttachRot->MultiplyRight (&m);
   pmAttachRot->MultiplyRight (&pwc->mRot);   // incase orientation different

   // done
}




/*****************************************************************************************
CObjectEditor::EditorCreate - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectEditor::EditorCreate (BOOL fAct)
{
   // can only edit user objects, not built in
   WCHAR szMajor[128], szMinor[128], szName[128];
   if (!LibraryObjects(m_dwRenderShard, TRUE)->ItemNameFromGUID (&m_OSINFO.gCode, &m_OSINFO.gSub, szMajor, szMinor, szName))
      return FALSE;

   if (!fAct)
      return TRUE;

   return ObjectEditorShowWindow (m_dwRenderShard, &m_OSINFO.gCode, &m_OSINFO.gSub);
 }


/*****************************************************************************************
CObjectEditor::EditorDestroy - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectEditor::EditorDestroy (void)
{
   // NOTE: For object editor don't actually close the eidting window since can
   // still edit object even if it's not in a world
   return TRUE;
}

/*****************************************************************************************
CObjectEditor::EditorShowWindow - From CObjectSocket. SOMETIMES OVERRIDDEN
*/
BOOL CObjectEditor::EditorShowWindow (BOOL fShow)
{
   // make sure one isn't visible already
   DWORD i;
   PVIEWOBJ pvo;
   for (i = 0; i < ListVIEWOBJ()->Num(); i++) {
      pvo = (PVIEWOBJ) ListVIEWOBJ()->Get(i);
      if (IsEqualGUID (pvo->gCode, m_OSINFO.gCode) && IsEqualGUID (pvo->gSub, m_OSINFO.gSub)) {
         ShowWindow (pvo->pView->m_hWnd, fShow ? SW_SHOW : SW_HIDE);
         return TRUE;
      }
   }

   return FALSE;
}

// BUGBUG - When do fur, will need to manipulte points when morph

// BUGBUG - May want to allow CP for joints to be exportable up the object so that
// can put a character on a vehicle

// BUGBUG - Should increment minor ID so that if an object contains a sub-object,
// will get outlines around it (for example couch or reclinder should have sub
// objects between cushions). NOTE: If high-bit set then do something different,
// since high-bit(?) means that is transparent and shouldnt be outlined

// BUGBUG - Some of the bones/attributes seem to have disappeared from the
// head. These include morpheyebrowsup (or down?), and bonehead (tilt).
