/***********************************************************************
ControlStatus.cpp - Code for a control

begun 4/6/2000 by Mike Rozak
Copyright 2000 mike Rozak. All rights reserved
*/

#include <windows.h>
#include <stdlib.h>
#include "escarpment.h"
#include "resleak.h"


#define ESCM_ALIGNTEXT        (ESCM_USER+1)


typedef struct {
   COLORREF       cBorder;
   int            iBorderSize;
   int            iMarginLeftRight;
   int            iMarginTopBottom;
   PWSTR          pszVCenter;
   PWSTR          pszHCenter;
   CEscTextBlock  *pTextBlock;
   CMMLNode       *pNode;
   BOOL           fRecalcText;      // set to TRUE if need to recalc text
} STATUS, *PSTATUS;


/***********************************************************************
Control callback
*/
BOOL ControlStatus (PCEscControl pControl, DWORD dwMessage, PVOID pParam)
{
   STATUS *pc = (STATUS*) pControl->m_mem.p;

   switch (dwMessage) {
   case ESCM_CONSTRUCTOR:
      {
         pControl->m_mem.Required (sizeof(STATUS));
         pc = (STATUS*) pControl->m_mem.p;
         memset (pc, 0, sizeof(STATUS));
         pc->cBorder = 0;
         pc->iBorderSize = 2;
         pc->iMarginLeftRight = pc->iMarginTopBottom = 6;

         pControl->AttribListAddColor (L"bordercolor", &pc->cBorder, FALSE, TRUE);
         pControl->AttribListAddDecimal (L"border", &pc->iBorderSize, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"lrmargin", &pc->iMarginLeftRight, &pc->fRecalcText, TRUE);
         pControl->AttribListAddDecimal (L"tbmargin", &pc->iMarginTopBottom, &pc->fRecalcText, TRUE);
         pControl->AttribListAddString (L"align", &pc->pszHCenter, NULL, FALSE);
         pControl->AttribListAddString (L"valign", &pc->pszVCenter, &pc->fRecalcText, TRUE);

      }
      return TRUE;

   case ESCM_INITCONTROL:
      {
         // do nothing
      }
      return TRUE;



   case ESCM_DESTRUCTOR:
      if (pc->pTextBlock)
         delete pc->pTextBlock;
      if (pc->pNode)
         delete pc->pNode;
      return TRUE;


   case ESCM_STATUSTEXT:
      {
         ESCMSTATUSTEXT *p = (ESCMSTATUSTEXT*) pParam;

         PCMMLNode   pNode;
         if (p->pNode) {
            pNode = p->pNode->Clone();
         }
         else
            pNode = NULL;
         if (!pNode && p->pszText) {
            pNode = new CMMLNode;
            pNode->NameSet (L"null");
            pNode->ContentAdd (p->pszText);
         }
         if (!pNode && p->pszMML) {
            CEscError   err;
            pNode = ParseMML (p->pszMML, pControl->m_hInstance, NULL, NULL, &err);
         }

         if (!pNode)
            return FALSE;

         // dlete the old one
         if (pc->pTextBlock)
            delete pc->pTextBlock;
         pc->pTextBlock = NULL;
         if (pc->pNode)
            delete pc->pNode;
         pc->pNode = NULL;

         // recalc
         pc->pNode = pNode;
         pControl->Invalidate ();
      }
      return TRUE;

   case ESCM_ALIGNTEXT:
      {
         if (!pc->pTextBlock)
            return TRUE;

         // figure out center, etc.
         RECT  r;
         r.left = r.top = 0;
         r.right = pControl->m_rPosn.right - pControl->m_rPosn.left;
         r.bottom = pControl->m_rPosn.bottom - pControl->m_rPosn.top;

         POINT pOffset;
         if (pc->pszHCenter && !wcsicmp(pc->pszHCenter, L"right"))
            pOffset.x = r.right - pc->iMarginLeftRight - pc->pTextBlock->m_iCalcWidth;
         else if (pc->pszHCenter && !wcsicmp(pc->pszHCenter, L"left"))
            pOffset.x = r.left + pc->iMarginLeftRight;
         else
            pOffset.x = (r.right + r.left) / 2 - pc->pTextBlock->m_iCalcWidth / 2;

         if (pc->pszVCenter && !wcsicmp(pc->pszVCenter, L"bottom"))
            pOffset.y = r.bottom - pc->iMarginTopBottom - pc->pTextBlock->m_iCalcHeight;
         else if (pc->pszVCenter && !wcsicmp(pc->pszVCenter, L"top"))
            pOffset.y = r.top + pc->iMarginTopBottom;
         else
            pOffset.y = (r.bottom + r.top) / 2 - pc->pTextBlock->m_iCalcHeight / 2;

         // post interpret
         pc->pTextBlock->PostInterpret (pOffset.x, pOffset.y, &r);
      }
      return TRUE;

   case ESCM_SIZE:
      {
         if (!pc->pTextBlock)
            return TRUE;

         // set flag so we reinterpret
         pc->fRecalcText = TRUE;

         pControl->Invalidate ();
      }
      return TRUE;


   case ESCM_PAINT:
      {
         PESCMPAINT p = (PESCMPAINT) pParam;

         // if there's no text block and there's a node
         if (!pc->pTextBlock && pc->pNode) {
            pc->pTextBlock = pControl->TextBlock (p->hDC, pc->pNode,
               p->rControlHDC.right - p->rControlHDC.left - 2*pc->iMarginLeftRight - 2*pc->iBorderSize,
               TRUE, FALSE);
            pc->pNode = NULL;
            pControl->Message (ESCM_ALIGNTEXT);
            pc->fRecalcText = FALSE;
         }

         // if there's no content use default
         if (!pc->pTextBlock && pControl->m_pNode->ContentNum()) {
            pc->pTextBlock = pControl->TextBlock (p->hDC, pControl->m_pNode,
               p->rControlHDC.right - p->rControlHDC.left - 2*pc->iMarginLeftRight - 2*pc->iBorderSize,
               FALSE, TRUE);
            pControl->Message (ESCM_ALIGNTEXT);
            pc->fRecalcText = FALSE;
         }

         // if reinterpret set then do so
         if (pc->fRecalcText && pc->pTextBlock) {
            pc->pTextBlock->m_fi = pControl->m_fi;
            pc->pTextBlock->ReInterpret (p->hDC,
               p->rControlHDC.right - p->rControlHDC.left - 2*pc->iMarginLeftRight - 2*pc->iBorderSize,
               FALSE);
            pControl->Message (ESCM_ALIGNTEXT);
         }
         pc->fRecalcText = FALSE;

         // paint the text block
         if (pc->pTextBlock) {
            POINT pOffset;
            pOffset.x = p->rControlHDC.left;
            pOffset.y = p->rControlHDC.top;
            pc->pTextBlock->Paint (p->hDC, &pOffset, &p->rControlHDC, &p->rControlScreen, &p->rTotalScreen);
         }

         if (pc->iBorderSize) {
            HBRUSH hbr;
            int   iBorder = (int) pc->iBorderSize;
            hbr = CreateSolidBrush (pc->cBorder);

            // left
            RECT r;
            r = p->rControlHDC;
            r.right = r.left + iBorder;
            FillRect (p->hDC, &r, hbr);

            // right
            r = p->rControlHDC;
            r.left = r.right - iBorder;
            FillRect (p->hDC, &r, hbr);

            // top
            r = p->rControlHDC;
            r.bottom = r.top + iBorder;
            FillRect (p->hDC, &r, hbr);

            // bottom
            r = p->rControlHDC;
            r.top = r.bottom - iBorder;
            FillRect (p->hDC, &r, hbr);

            DeleteObject (hbr);
         }

      }
      return TRUE;

   }

   return FALSE;
}

// BUGBUG - 2.0 - When have <big><status/></big> and resize the window (with
// the status control in it) then lose the <big> property
