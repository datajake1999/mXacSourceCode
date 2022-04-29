/*************************************************************************************
CMIFLString.cpp - Definition for a parameter

begun 11/01/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "..\M3D.h"
#include "resource.h"
#include "..\mifl.h"


/*************************************************************************************
CMIFLString::Constructor and destructor
*/
CMIFLString::CMIFLString (void)
{
   m_lLANGID.Init (sizeof(LANGID));

   Clear();
}

CMIFLString::~CMIFLString (void)
{
   // do nothing for now
}

/*************************************************************************************
CMIFLString::Clear - Wipes the parameter to intial state
*/
void CMIFLString::Clear()
{
   m_dwTempID = MIFLTempID ();
   m_dwLibOrigFrom = 0;

   MemZero (&m_memName);
   m_lLANGID.Clear();
   m_lString.Clear();
}

/*************************************************************************************
CMIFLString::CloneTo - Stanrdard API

inputs
   LANGID      lidKeep - If -1 then keep all languages, else only keep the language specified
*/
BOOL CMIFLString::CloneTo (CMIFLString *pTo, LANGID lidKeep)
{
   pTo->Clear();

   pTo->m_dwTempID = m_dwTempID;
   pTo->m_dwLibOrigFrom = m_dwLibOrigFrom;

   if (((PWSTR)m_memName.p)[0])
      MemCat (&pTo->m_memName, (PWSTR)m_memName.p);

   if ((lidKeep == (LANGID)-1) || (m_lLANGID.Num() < 2)) {
      pTo->m_lLANGID.Init (sizeof(LANGID), m_lLANGID.Get(0), m_lLANGID.Num());
      
      DWORD i;
      pTo->m_lString.Required (m_lString.Num());
      for (i = 0; i < m_lString.Num(); i++)
         pTo->m_lString.Add (m_lString.Get(i), m_lString.Size(i));
   }
   else {
      // find the closest match and keep that
      DWORD dwFind = MIFLLangMatch (&m_lLANGID, lidKeep, FALSE);
      LANGID *pl = (LANGID*) m_lLANGID.Get(0);

      pTo->m_lLANGID.Add (&pl[dwFind]);
      pTo->m_lString.Add (m_lString.Get(dwFind), m_lString.Size(dwFind));
   }

   return TRUE;
}


/*************************************************************************************
CMIFLString::FindExact - Given a language ID, find an exact match.

inputs
   LANGID         lid - Language
   BOOL           fExactSecondary - If TRUE needs an exact secondary match too
returns
   DWORD - index into list, or -1 if can't find
*/
DWORD CMIFLString::FindExact (LANGID lid, BOOL fExactSecondary)
{
   if (!fExactSecondary)
      return MIFLLangMatch (&m_lLANGID, lid, TRUE);

   // see if exists in the list...
   DWORD dwIndex;
   LANGID *pl = (LANGID*) m_lLANGID.Get(0);
   for (dwIndex = 0; dwIndex < m_lLANGID.Num(); dwIndex++)
      if (pl[dwIndex] == lid)
         return dwIndex;

   return (DWORD)-1;
}

/*************************************************************************************
CMIFLString::Get - Gets the resource value by finding the cloest language.

inputs
   LANGID         lid - Language
   DWORD          *pdwLength - Filled in with the length of the string, in characters
returns
   PWSTR - Resource from the closest matching language
*/
PWSTR CMIFLString::Get (LANGID lid, DWORD *pdwLength)
{
   switch (m_lLANGID.Num()) {
   case 0:
      return NULL;
   case 1:
      if (pdwLength)
         *pdwLength = (DWORD)m_lString.Size(0) / sizeof(WCHAR) - 1;
      return (PWSTR) m_lString.Get(0); // only one language
   }

   // else...
   DWORD dwFind = MIFLLangMatch (&m_lLANGID, lid, FALSE);
   if (pdwLength)
      *pdwLength = (DWORD)m_lString.Size(dwFind) / sizeof(WCHAR) - 1;
   return (PWSTR)m_lString.Get(dwFind);
}


/*************************************************************************************
CMIFLString::Clone - Stanrdard API

inputs
   BOOL        fKeepDocs - If TRUE (default) then documentation will be cloned.
                  If FALSE, then don't bother cloning documetnation because wont be used
*/
CMIFLString *CMIFLString::Clone (LANGID lidKeep)
{
   PCMIFLString pNew = new CMIFLString;
   if (!pNew)
      return NULL;
   if (!CloneTo (pNew, lidKeep)) {
      delete pNew;
      return NULL;
   }

   return pNew;
}



static PWSTR gpszMIFLString = L"MIFLString";
static PWSTR gpszName = L"Name";

/*************************************************************************************
CMIFLString::MMLTo - Stanrdard API
*/
PCMMLNode2 CMIFLString::MMLTo (void)
{
   PCMMLNode2 pNode = new CMMLNode2;
   if (!pNode)
      return NULL;
   pNode->NameSet (gpszMIFLString);

   if (m_memName.p && ((PWSTR)m_memName.p)[0])
      MMLValueSet (pNode, gpszName, (PWSTR)m_memName.p);

   DWORD i;
   WCHAR szTemp[64];
   PWSTR psz;
   LANGID *pl = (LANGID*)m_lLANGID.Get(0);
   for (i = 0; i < m_lLANGID.Num(); i++) {
      swprintf (szTemp, L"ID%d", (int)i);
      MMLValueSet (pNode, szTemp, (int)pl[i]);

      psz = (PWSTR)m_lString.Get(i);
      if (!psz || !psz[0])
         continue;   // nothing to write
      swprintf (szTemp, L"S%d", (int)i);
      MMLValueSet (pNode, szTemp, psz);
   } // i


   return pNode;
}


/*************************************************************************************
CMIFLString::MMLTo - Stanrdard API
*/
BOOL CMIFLString::MMLFrom (PCMMLNode2 pNode)
{
   Clear();

   PWSTR psz;
   psz = MMLValueGet (pNode, gpszName);
   if (psz)
      MemCat (&m_memName, psz);

   DWORD i;
   WCHAR szTemp[64];
   LANGID *pl = (LANGID*)m_lLANGID.Get(0);
   for (i = 0; ; i++) {
      LANGID lid;
      swprintf (szTemp, L"ID%d", (int)i);
      lid = (LANGID)MMLValueGetInt (pNode, szTemp, -1);
      if (lid == (LANGID)-1)
         break;

      swprintf (szTemp, L"S%d", (int)i);
      psz = MMLValueGet (pNode, szTemp);
      if (!psz)
         psz = L"";

      // add them
      m_lLANGID.Add (&lid);
      m_lString.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   } // i

   return TRUE;
}

/*************************************************************************************
CMIFLString::LangRemove - Removes an occurance of the given language, if it appears.

inputs
   LANGID            lid - Language
   PCMIFLLib         pLib - Library to use for undo/redo
returns
   BOOL - TRUE if had
*/
BOOL CMIFLString::LangRemove (LANGID lid, PCMIFLLib pLib)
{
   // see if it's in the list
   LANGID *pl = (LANGID*)m_lLANGID.Get(0);
   DWORD i;
   for (i = 0; i < m_lLANGID.Num(); i++)
      if (pl[i] == lid)
         break;
   if (i >= m_lLANGID.Num())
      return FALSE;

   // else, delte
   pLib->AboutToChange();
   m_lLANGID.Remove (i);
   m_lString.Remove (i);
   pLib->Changed();
   return TRUE;
}





/*********************************************************************************
StringEditPage - UI
*/

BOOL StringEditPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PMIFLPAGE pmp = (PMIFLPAGE) pPage->m_pUserData;
   PCMIFLProj pProj = pmp->pProj;
   PCMIFLLib pLib = pmp->pLib;
   PCMIFLString pString = pmp->pString;

   if (MIFLDefPage (pPage, dwMessage, pParam))
      return TRUE;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         TTSCacheDefaultGet (pPage, L"ttsfile");

         PCEscControl pControl;

         pControl = pPage->ControlFind (L"name");
         if (pControl)
            pControl->AttribSet (Text(), (PWSTR) pString->m_memName.p);


         // indicate if string name is valid
         pPage->Message (ESCM_USER+83);

         // set string text
         DWORD i;
         WCHAR szTemp[64];
         LANGID *pl = (LANGID*)pString->m_lLANGID.Get(0);
         for (i = 0; i < pString->m_lLANGID.Num(); i++) {
            swprintf (szTemp, L"lang:%d", (int)pl[i]);
            pControl = pPage->ControlFind (szTemp);
            if (!pControl)
               continue;

            PWSTR psz = (PWSTR)pString->m_lString.Get(i);
            if (!psz)
               continue;
            pControl->AttribSet (Text(), psz);
         } // i

         // will need to set movelib combo
         MIFLComboLibs (pPage, L"movelib", L"move", pProj, pLib->m_dwTempID);
      }
      return TRUE;

   case ESCM_USER+83:   // test for unique
      {
         // see if the name is unqiue
         DWORD i;
         for (i = 0; i < pProj->LibraryNum(); i++) {
            PCMIFLLib pLib = pProj->LibraryGet(i);
            DWORD dwFind = pLib->StringFind ((PWSTR)pString->m_memName.p, pString->m_dwTempID);
            if (dwFind != -1)
               break;
         } // i

         // set
         PCEscControl pControl = pPage->ControlFind (L"isunique");
         if (!pControl)
            return TRUE;
         ESCMSTATUSTEXT st;
         memset (&st, 0, sizeof(st));

         MemZero (&gMemTemp);
         if (i < pProj->LibraryNum())
            MemCat (&gMemTemp, L"<font color=#800000>(Name already exists)</font>");
         else
            MemCat (&gMemTemp, L"<null>(Unique name)</null>");
         st.pszMML = (PWSTR)gMemTemp.p;
         pControl->Message (ESCM_STATUSTEXT, &st);
         return TRUE;
      }
      return TRUE;

   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         if (!p->pControl || !p->pControl->m_pszName)
            return FALSE;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"delete")) {
            if (IDYES != pPage->MBYesNo (L"Are you sure you wish to delete this string?"))
               return TRUE;

            // remove the String
            pLib->StringRemove (pString->m_dwTempID);

            // set a link to go to the String list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dstringlist", pLib->m_dwTempID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"duplicate")) {
            pPage->MBInformation (L"Duplicated", L"You are now modifying the duplicated string.");

            // duplicate
            DWORD dwID = pLib->StringAddCopy (pString);

            // set a link to go to the String list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dstring:%dedit", pLib->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"move")) {
            // find where moved to
            PCEscControl pControl = pPage->ControlFind (L"movelib");
            if (!pControl)
               return TRUE;
            ESCMCOMBOBOXGETITEM gi;
            memset (&gi, 0, sizeof(gi));
            gi.dwIndex = pControl->AttribGetInt (CurSel());
            pControl->Message (ESCM_COMBOBOXGETITEM, &gi);
            if (!gi.pszName)
               return FALSE;

            // find the library
            DWORD dwLibIndex = _wtoi(gi.pszName);
            PCMIFLLib pLibTo = pProj->LibraryGet (dwLibIndex);
            if (!pLibTo)
               return TRUE;

            if (pLib->m_fReadOnly) {
               if (IDYES != pPage->MBYesNo (L"Do you want to copy the string?",
                  L"The library you are moving from is built-in, and can't be modified. "
                  L"The string will be copied to the new library instead of moved."))
                  return TRUE;
            }

            // duplicate
            DWORD dwID = pLibTo->StringAddCopy (pString);

            // remove from the current lib
            if (!pLib->m_fReadOnly)
               pLib->StringRemove (pString->m_dwTempID);

            // set a link to go to the String list
            WCHAR szTemp[64];
            swprintf (szTemp, L"lib:%dstring:%dedit", pLibTo->m_dwTempID, dwID);
            pPage->Exit (szTemp);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newttsfile")) {
            if (TTSCacheDefaultUI (pPage->m_pWindow->m_hWnd))
               TTSCacheDefaultGet (pPage, L"ttsfile");
            return TRUE;
         }

         // transplanted prosody options
         PWSTR pszTTSSC = L"ttssc:", pszTTSSP = L"ttssp:", pszTTSTP = L"ttstp:", pszTTSDT = L"ttsdt:";
         DWORD dwTTSSCLen = (DWORD)wcslen(pszTTSSC), dwTTSSPLen = (DWORD)wcslen(pszTTSSP),
            dwTTSTPLen = (DWORD)wcslen(pszTTSTP), dwTTSDTLen = (DWORD)wcslen(pszTTSDT);
         if (!wcsncmp (psz, pszTTSSC, dwTTSSCLen)) {
            LANGID lid = (LANGID)_wtoi(psz + dwTTSSCLen);
            DWORD dwIndex = pString->FindExact (lid, FALSE);
            PWSTR pszSpeak = (PWSTR) pString->m_lString.Get(dwIndex);
            TTSCacheSpellCheck (pszSpeak, pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         if (!wcsncmp (psz, pszTTSSP, dwTTSSPLen)) {
            LANGID lid = (LANGID)_wtoi(psz + dwTTSSPLen);
            DWORD dwIndex = pString->FindExact (lid, FALSE);
            PWSTR pszSpeak = (PWSTR) pString->m_lString.Get(dwIndex);
            TTSCacheSpeak (pszSpeak, FALSE, FALSE, pPage->m_pWindow->m_hWnd);
            return TRUE;
         }
         if (!wcsncmp (psz, pszTTSTP, dwTTSTPLen)) {
            LANGID lid = (LANGID)_wtoi(psz + dwTTSTPLen);
            DWORD dwIndex = pString->FindExact (lid, FALSE);
            PWSTR pszSpeak = (PWSTR) pString->m_lString.Get(dwIndex);

            if (2 == pLib->TransProsQuickDialog (NULL, pPage->m_pWindow->m_hWnd, lid,
               (PWSTR)pString->m_memName.p, pszSpeak)) {

                  // enable the delete control
                  WCHAR szTemp[64];
                  swprintf (szTemp, L"ttsdt:%d", (int)lid);
                  PCEscControl pControl = pPage->ControlFind (szTemp);
                  if (pControl)
                     pControl->Enable (TRUE);
            }

            return TRUE;
         }
         if (!wcsncmp (psz, pszTTSDT, dwTTSDTLen)) {
            LANGID lid = (LANGID)_wtoi(psz + dwTTSDTLen);
            DWORD dwIndex = pString->FindExact (lid, FALSE);
            PWSTR pszSpeak = (PWSTR) pString->m_lString.Get(dwIndex);
            if (pLib->TransProsQuickDeleteUI (pPage, pszSpeak, lid)) {
                  // disable the delete control
                  WCHAR szTemp[64];
                  swprintf (szTemp, L"ttsdt:%d", (int)lid);
                  PCEscControl pControl = pPage->ControlFind (szTemp);
                  if (pControl)
                     pControl->Enable (FALSE);
            }
            return TRUE;
         }
      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE) pParam;
         if (!p->pControl || !p->pControl->m_pszName)
            break;
         PWSTR psz = p->pControl->m_pszName;

         if (!_wcsicmp(psz, L"name")) {
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!pString->m_memName.Required (dwNeed))
               return FALSE;
            pLib->AboutToChange();
            p->pControl->AttribGet (Text(), (PWSTR)pString->m_memName.p, (DWORD)pString->m_memName.m_dwAllocated, &dwNeed);
            pLib->StringSort();
            pLib->Changed();

            // indicate if string name is valid
            pPage->Message (ESCM_USER+83);

            return TRUE;
         }

         PWSTR pszLang = L"lang:";
         DWORD dwLangLen = (DWORD)wcslen(pszLang);
         if (!wcsncmp(psz, pszLang, dwLangLen)) {
            LANGID lid = (LANGID)_wtoi(psz + dwLangLen);

            // get the string
            CMem mem;
            DWORD dwNeed = 0;
            p->pControl->AttribGet (Text(), NULL, 0, &dwNeed);
            if (!mem.Required (dwNeed))
               return FALSE;
            pLib->AboutToChange();
            p->pControl->AttribGet (Text(), (PWSTR)mem.p, (DWORD)mem.m_dwAllocated, &dwNeed);
            PWSTR psz = (PWSTR)mem.p;

            // see if exists in the list...
            DWORD dwIndex;
            LANGID *pl = (LANGID*) pString->m_lLANGID.Get(0);
            dwIndex = MIFLLangMatch (&pString->m_lLANGID, lid, TRUE);
            //for (dwIndex = 0; dwIndex < pString->m_lLANGID.Num(); dwIndex++)
            //   if (pl[dwIndex] == lid)
            //      break;
            if (dwIndex >= pString->m_lLANGID.Num()) {
               // if it's an empty string then ignore
               if (!psz[0])
                  return TRUE;

               pLib->AboutToChange();
               pString->m_lLANGID.Add (&lid);
               dwNeed = ((DWORD)wcslen(psz)+1)*sizeof(WCHAR);  // just to make sure
               pString->m_lString.Add (psz, dwNeed);
               pLib->Changed();
               return TRUE;
            } // add it

            // else, already exists....

            // if the new string is empty then delete it
            if (!psz[0]) {
               pLib->AboutToChange();
               pString->m_lLANGID.Remove (dwIndex);
               pString->m_lString.Remove (dwIndex);
               pLib->Changed();
               return TRUE;
            }

            // else, set
            pLib->AboutToChange();
            dwNeed = ((DWORD)wcslen(psz)+1)*sizeof(WCHAR);  // just to make sure
            pString->m_lString.Set (dwIndex, psz, dwNeed);
            pLib->Changed();
            return TRUE;
         }
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            MemZero (&gMemTemp);
            MemCat (&gMemTemp, L"Modify string - "); 
            MemCatSanitize (&gMemTemp, (PWSTR)pString->m_memName.p);

            // NOTE: Strings dont have assocaited help

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"OVERRIDES")) {
            MemZero (&gMemTemp);

            PWSTR pszHigher, pszLower;
            pProj->StringOverridden (pLib->m_dwTempID, (PWSTR)pString->m_memName.p, &pszHigher, &pszLower);

            if (pszHigher) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overridden by string in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszHigher));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            if (pszLower) {
               MemCat (&gMemTemp, L"<p align=right><bold><big><big>(Overrides string in ");
               MemCatSanitize (&gMemTemp, LibraryDisplayName(pszLower));
               MemCat (&gMemTemp, L")</big></big></bold></p>");
            }
            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"STRINGLANG")) {
            MemZero (&gMemTemp);

            DWORD i;
            LANGID *pl = (LANGID*)pProj->m_lLANGID.Get(0);
            for (i = 0; i < pProj->m_lLANGID.Num(); i++) {
               if (i)
                  MemCat (&gMemTemp, L"<p/>");

               LANGID lid = pl[i];
               DWORD dwClosestLang = MIFLLangMatch (&pString->m_lLANGID, lid, TRUE);
               if (dwClosestLang < pString->m_lLANGID.Num())
                  lid = *((LANGID*)pString->m_lLANGID.Get(dwClosestLang));

               // get the string
               DWORD dwIndex = MIFLLangFind (lid);
               PWSTR psz = MIFLLangGet (dwIndex, NULL);
               PWSTR pszOrig = psz;
               if (!psz)
                  psz = L"Unknown";

               MemCat (&gMemTemp, L"<bold><a>");
               MemCatSanitize (&gMemTemp, psz);
               if (!i)
                  MemCat (&gMemTemp, L" (default)");
               MemCat (&gMemTemp, L"<xHoverHelp>Type in the text used. You don't need quotes or escape characters.</xHoverHelp>");
               MemCat (&gMemTemp, L"</a><br/><edit width=100% multiline=true wordwrap=true maxchars=100000 ");
               if (pLib->m_fReadOnly)
                  MemCat (&gMemTemp, L"readonly=true ");
               MemCat (&gMemTemp, L"name=lang:");
               MemCat (&gMemTemp, (int)lid);
               MemCat (&gMemTemp, L"/></bold>");

               // transplanted prosody
               if (pProj->m_fTransProsQuick) {
                  MemCat (&gMemTemp,
                     L"<br/><align align=right>"
                     L"<button name=ttssc:");
                  MemCat (&gMemTemp, (int)lid);
                  MemCat (&gMemTemp,
                     L">Spell check</button>"
                     L"<button name=ttssp:");
                  MemCat (&gMemTemp, (int)lid);
                  MemCat (&gMemTemp,
                     L">Speak</button>");

                  if (!pLib->m_fReadOnly) {
                     MemCat (&gMemTemp,
                        L"<button name=ttstp:");
                     MemCat (&gMemTemp, (int)lid);
                     MemCat (&gMemTemp,
                        L">Record transplanted prosody</button>");

                     MemCat (&gMemTemp,
                        L"<button name=ttsdt:");
                     MemCat (&gMemTemp, (int)lid);
                     PWSTR pszText = pString->Get(lid);
                     DWORD dwID = pLib->TransProsQuickFind (pszText, lid);
                     if (dwID == (DWORD)-1)
                        MemCat (&gMemTemp, L" enabled=false");
                     MemCat (&gMemTemp,
                        L">");
                     MemCat (&gMemTemp,
                        L"Delete trans. pros.</button>");
                  }

                  MemCat (&gMemTemp,
                     L"</align>");
               } // transprosquick
            };

            // if not lang's
            if (!pProj->m_lLANGID.Num())
               MemCat (&gMemTemp, L"Your current project doesn't include any languages.");

            p->pszSubString = (PWSTR)gMemTemp.p;
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"IFTRANSPROSQUICK")) {
            p->pszSubString = pProj->m_fTransProsQuick ? L"" : L"<comment>";
            return TRUE;
         }
         else if (!_wcsicmp(p->pszSubName, L"ENDIFTRANSPROSQUICK")) {
            p->pszSubString = pProj->m_fTransProsQuick ? L"" : L"</comment>";
            return TRUE;
         }
      }
      break;
   }

   return FALSE;
}



/********************************************************************************
CMIFLString::DialogEditSingle - Brings up a page (using pWindow) to display the
given link. Once the user exits the page pszLink is filled in with the new link

inputs
   PCMIFLLib         pLib - Library that the string is part of
   PCEscWindow       pWindow - Window to use
   PWSTR             pszUse - Initialliy filled with a string for the link to start
                     out with. If the link is not found to match any known pages, such as "",
                     then it automatically goes to the main project page.
                     NOTE: This is minus the "lib:%dstring%d" part.
   PWSTR             pszNext - Once the page
                     has been viewed by the user the contents of pszLink are filled in
                     with the new string.
   int               *piVScroll - Should initially fill in with the scroll start, and will be
                     subsequencly filled in with window scroll position
returns
   BOOl - TRUE if success
*/
BOOL CMIFLString::DialogEditSingle (PCMIFLLib pLib, PCEscWindow pWindow, PWSTR pszUse, PWSTR pszNext, int *piVScroll)
{
   // what resource to use
   DWORD dwPage = IDR_MMLSTRINGEDIT;
   PESCPAGECALLBACK pPage = StringEditPage;

   // if it's a project link...
   if (!_wcsicmp(pszUse, L"edit")) {
      dwPage = IDR_MMLSTRINGEDIT;
      pPage = StringEditPage;
   }

   // keep a list of all the transplanted prosody resources originally used
   DWORD dwKeepID = m_dwTempID;
   CListFixed lResIDOrig;
   pLib->TransProsQuickEnum (&m_lString, &m_lLANGID, 0, &lResIDOrig);

   PWSTR pszRet;
   MIFLPAGE mp;
   memset (&mp, 0, sizeof(mp));
   mp.pString = this;
   mp.pLib = pLib;
   mp.pProj = mp.pLib->ProjectGet();
   mp.iVScroll = *piVScroll;
redo:
   pszRet = pWindow->PageDialog (ghInstance, dwPage, pPage, &mp);
   *piVScroll = mp.iVScroll = pWindow->m_iExitVScroll;
   if (pszRet && !_wcsicmp(pszRet, MIFLRedoSamePage()))
      goto redo;

   if (pszRet)
      wcscpy (pszNext, pszRet);
   else
      pszNext[0] = 0;

   // delete any transplanted prosody
   CListFixed lResIDNew;
   lResIDNew.Init (sizeof(DWORD));
   if ((DWORD)-1 != pLib->StringFind (m_dwTempID)) {
      // not deleted
      pLib->TransProsQuickEnum (&m_lString, &m_lLANGID, 0, &lResIDNew);
   }
   pLib->TransProsQuickDelete (&lResIDOrig, &lResIDNew);

   return TRUE;
}



