/*********************************************************************
Control.h - Common code used within controls.
*/

#ifndef _CONTROL_H_
#define _CONTROL_H_

// ControlButton.cpp
void Draw3DButton (PCRender pRender, COLORREF crBase, COLORREF crButton,
                   PWSTR pszStyle, PWSTR pszDefault, BOOL fDrawDown);


// ControlComboBox.cpp - For standardized drop-down functionality
// handle drop-down functionality
typedef struct {
   int            iWidth, iHeight;  // either a number or percent
   BOOL           fWidthPercent, fHeightPercent;   // TRUE if iWidth percent, FALSE if pixels
   PWSTR          pszStyle;   // style string
   PWSTR          pszVCenter; // vertical centering, "top", "bottom", "center"
   PWSTR          pszAppear;  // set to "right" for ComboBox to appear to right, default "below"
   COLORREF       cButton;
   COLORREF       cButtonBase;
   COLORREF       cLight;
   int            iButtonWidth, iButtonHeight;  // actual width and height of drawn thingy
   int            iButtonDepth;  // in Z coordinate
   int            iMarginTopBottom;
   int            iMarginLeftRight;
   int            iMarginComboBoxText;      // margin between the ComboBox and the text
   BOOL           fShowButton;         // if TRUE, show the ComboBox, FALSE just the text
   COLORREF       cTBackground, cBBackground;   // background color blend
   int            iCBHeight;  // height of the box
   int            iCBWidth;   // width of the box

   BOOL           fRecalcText;      // set to TRUE if need to recalc text in main control display
   CRender        *pRender;         // rendering objet
   CEscTextBlock  *pTextBlock;      // text block object that draw for the main display
                                    // Note: This does not delete the node, pNodeTextBlock
} DROPDOWN, *PDROPDOWN;

// ESCM_DROPDOWNTEXTBLOCK - Internal message sent to the control's main callback
// by the dropdown callback. It's sent when the text block is first created or resized.
// If the callback sets the pNode, pszMML, or pszText then the text block text will
// be recreated.
#define  ESCM_DROPDOWNTEXTBLOCK     (ESCM_USER+3504)
typedef struct {
   BOOL        fMustSet;   // if TRUE then pNode, pszMML, or pszText must be set since there's no text block
   PCMMLNode   pNode;      // fill this with PCMMLNode if want node.
   BOOL        fDeleteNode;   // if TRUE delete the node when delete the text block. Set if change pNode
   PWSTR       pszMML;     // MML for the block. This is NOT deleted.
   PWSTR       pszText;    // Text for the block. This is NOT deleted.
} ESCMDROPDOWNTEXTBLOCK, *PESCMDROPDOWNTEXTBLOCK;


// ESCM_DROPDOWNOPENED - Internal message sent to the control's main callback
// by the dropdown callback after the drop-down window has been created.
// THe callback should use pWindow and display a page as it likes
#define  ESCM_DROPDOWNOPENED        (ESCM_USER+3505)
typedef struct {
   PCEscWindow    pWindow; // already created in the right place
   int            iHeight; // height for the window
   int            iWidth;  // width for the window
   BOOL           fKeyOpen;   // set to TRUE if a key opened the combo. if FALSE opened by mousing over
} ESCMDROPDOWNOPENED, *PESCMDROPDOWNOPENED;


BOOL DropDownMessageHandler (PCEscControl pControl, DWORD dwMessage, PVOID pParam, PDROPDOWN pdd);
static void DropDownMakeTextBlock (PCEscControl pControl, PDROPDOWN pdd, int iWidth, RECT *pFull, BOOL fReinterpret);
void DropDownFitToScreen (PCEscPage pPage);


#endif // _CONTROL_H_
