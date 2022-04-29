/*************************************************************************************
CMIFLSocketServer.cpp - Code for displaying the main server window

begun 29/2/04 by Mike Rozak.
Copyright 2004 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <Psapi.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifserver.h"
#include "resource.h"



// ATPINFO - Information passed to AutoTransPros dialog
typedef struct {
   PCMIFLProj        pProj;         // project
   PCMIFLLib         pLib;          // library
   WCHAR             szWaveDir[256];  // wave directory
   WCHAR             szSRFile[256]; // speech recognition file
   WCHAR             szTTSFile[256];   // text-to-speech file
   BOOL              fAutoClean;    // if TRUE then delete al transpros resources before scanning
   DWORD             dwQuality;     // 1 for 1 sample per phone, etc.
   DWORD             dwNumbers;     // where the numbers start increasing rom
} ATPINFO, *PATPINFO;

static PWSTR gpszTransPros = L"TransPros";
static PWSTR gpszOrigText = L"OrigText";

/*****************************************************************************
CMIFLSocketServer::Constructor and destrucotur
*/
CMIFLSocketServer::CMIFLSocketServer ()
{
   m_fJustCalledHackITVMNew = FALSE;
   m_hImport.Init (sizeof(DWORD));

   DWORD dwID;

   // callback
   // NOTE: Don't need this in definitions
   //dwID = SLIMP_CONNECTIONNEW;
   //m_hImport.Add (L"connectionnew", &dwID, FALSE);

   //dwID = SLIMP_CONNECTIONMESSAGE;
   //m_hImport.Add (L"connectionmessage", &dwID, FALSE);

   //dwID = SLIMP_CONNECTIONERROR;
   //m_hImport.Add (L"connectionerror", &dwID, FALSE);

#define IMPCMD(x,y)           dwID = x; m_hImport.Add(y, &dwID, FALSE)

   // function calls
   dwID = SLIMP_CONNECTIONBAN;
   m_hImport.Add (L"connectionban", &dwID, FALSE);

   dwID = SLIMP_CONNECTIONQUEUEDTOSEND;
   m_hImport.Add (L"connectionqueuedtosend", &dwID, FALSE);

   dwID = SLIMP_CONNECTIONDISCONNECT;
   m_hImport.Add (L"connectiondisconnect", &dwID, FALSE);

   dwID = SLIMP_CONNECTIONINFOSET;
   m_hImport.Add (L"connectioninfoset", &dwID, FALSE);

   dwID = SLIMP_CONNECTIONINFOGET;
   m_hImport.Add (L"connectioninfoget", &dwID, FALSE);

   dwID = SLIMP_CONNECTIONENUM;
   m_hImport.Add (L"connectionenum", &dwID, FALSE);

   dwID = SLIMP_CONNECTIONSEND;
   m_hImport.Add (L"connectionsend", &dwID, FALSE);

   dwID = SLIMP_CONNECTIONSENDVOICECHAT;
   m_hImport.Add (L"connectionsendvoicechat", &dwID, FALSE);

   dwID = SLIMP_VOICECHATALLOWWAVES;
   m_hImport.Add (L"voicechatallowwaves", &dwID, FALSE);

   dwID = SLIMP_CONNECTIONSLIMIT;
   m_hImport.Add (L"connectionslimit", &dwID, FALSE);

   dwID = SLIMP_RENDERCACHELIMITS;
   m_hImport.Add (L"rendercachelimits", &dwID, FALSE);

   dwID = SLIMP_RENDERCACHESEND;
   m_hImport.Add (L"rendercachesend", &dwID, FALSE);

   dwID = SLIMP_SHARDPARAM;
   m_hImport.Add (L"shardparam", &dwID, FALSE);

   dwID = SLIMP_SHUTDOWNIMMEDIATELY;
   m_hImport.Add (L"shutdownimmediately", &dwID, FALSE);

   // info values
   dwID = SLIMP_INFOIP;
   m_hImport.Add (L"ip", &dwID, FALSE);

   dwID = SLIMP_INFOSTATUS;
   m_hImport.Add (L"status", &dwID, FALSE);

   dwID = SLIMP_INFOUSER;
   m_hImport.Add (L"user", &dwID, FALSE);

   dwID = SLIMP_INFOCHARACTER;
   m_hImport.Add (L"character", &dwID, FALSE);

   dwID = SLIMP_INFOUNIQUEID;
   m_hImport.Add (L"uniqueid", &dwID, FALSE);

   dwID = SLIMP_INFOOBJECT;
   m_hImport.Add (L"object", &dwID, FALSE);

   dwID = SLIMP_INFOSENDBYTES;
   m_hImport.Add (L"sendbytes", &dwID, FALSE);

   dwID = SLIMP_INFOSENDBYTESCOMP;
   m_hImport.Add (L"sendbytescomp", &dwID, FALSE);

   dwID = SLIMP_INFOSENDBYTESEXPECT;
   m_hImport.Add (L"sendbytesexpect", &dwID, FALSE);

   dwID = SLIMP_INFORECEIVEBYTES;
   m_hImport.Add (L"receivebytes", &dwID, FALSE);

   dwID = SLIMP_INFORECEIVEBYTESCOMP;
   m_hImport.Add (L"receivebytescomp", &dwID, FALSE);

   dwID = SLIMP_INFORECEIVEBYTESEXPECT;
   m_hImport.Add (L"receivebytesexpect", &dwID, FALSE);

   dwID = SLIMP_INFOCONNECTTIME;
   m_hImport.Add (L"connecttime", &dwID, FALSE);

   dwID = SLIMP_INFOSENDLAST;
   m_hImport.Add (L"sendlast", &dwID, FALSE);

   dwID = SLIMP_INFORECEIVELAST;
   m_hImport.Add (L"receivelast", &dwID, FALSE);

   dwID = SLIMP_INFOBYTESPERSEC;
   m_hImport.Add (L"bytespersec", &dwID, FALSE);

   IMPCMD (SLIMP_DATABASEOBJECTADD, L"databaseobjectadd");
   IMPCMD (SLIMP_DATABASEOBJECTSAVE, L"databaseobjectsave");
   IMPCMD (SLIMP_DATABASEOBJECTCHECKOUT, L"databaseobjectcheckout");
   IMPCMD (SLIMP_DATABASEOBJECTCHECKIN, L"databaseobjectcheckin");
   IMPCMD (SLIMP_DATABASEOBJECTDELETE, L"databaseobjectdelete");
   IMPCMD (SLIMP_DATABASEPROPERTYGET, L"databasepropertyget");
   IMPCMD (SLIMP_DATABASEPROPERTYSET, L"databasepropertyset");
   IMPCMD (SLIMP_DATABASEQUERY, L"databasequery");
   IMPCMD (SLIMP_DATABASEPROPERTYADD, L"databasepropertyadd");
   IMPCMD (SLIMP_DATABASEPROPERTYENUM, L"databasepropertyenum");
   IMPCMD (SLIMP_DATABASEPROPERTYREMOVE, L"databasepropertyremove");
   IMPCMD (SLIMP_DATABASESAVE, L"databasesave");
   IMPCMD (SLIMP_DATABASEBACKUP, L"databasebackup");
   IMPCMD (SLIMP_DATABASEOBJECTQUERYCHECKOUT, L"databaseobjectquerycheckout");
   IMPCMD (SLIMP_DATABASEOBJECTQUERYCHECKOUT2, L"databaseobjectquerycheckout2");
   IMPCMD (SLIMP_DATABASEOBJECTENUMCHECKOUT, L"databaseobjectenumcheckout");
   IMPCMD (SLIMP_DATABASEOBJECTNUM, L"databaseobjectnum");
   IMPCMD (SLIMP_DATABASEOBJECTGET, L"databaseobjectget");


   IMPCMD (SLIMP_NLPPARSERENUM, L"nlpparserenum");
   IMPCMD (SLIMP_NLPPARSERREMOVE, L"nlpparserremove");
   IMPCMD (SLIMP_NLPPARSERCLONE, L"nlpparserclone");
   IMPCMD (SLIMP_NLPPARSE, L"nlpparse");
   IMPCMD (SLIMP_NLPRULESETENUM, L"nlprulesetenum");
   IMPCMD (SLIMP_NLPRULESETREMOVE, L"nlprulesetremove");
   IMPCMD (SLIMP_NLPRULESETADD, L"nlprulesetadd");
   IMPCMD (SLIMP_NLPRULESETENABLEGET, L"nlprulesetenableget");
   IMPCMD (SLIMP_NLPRULESETENABLESET, L"nlprulesetenableset");
   IMPCMD (SLIMP_NLPRULESETENABLEALL, L"nlprulesetenableall");
   IMPCMD (SLIMP_NLPRULESETEXISTS, L"nlprulesetexists");
   IMPCMD (SLIMP_NLPVERBFORM, L"nlpverbform");
   IMPCMD (SLIMP_NLPNOUNCASE, L"nlpnouncase");

   IMPCMD (SLIMP_SAVEDGAMEENUM, L"savedgameenum");
   IMPCMD (SLIMP_SAVEDGAMEREMOVE, L"savedgameremove");
   IMPCMD (SLIMP_SAVEDGAMESAVE, L"savedgamesave");
   IMPCMD (SLIMP_SAVEDGAMELOAD, L"savedgameload");
   IMPCMD (SLIMP_SAVEDGAMENUM, L"savedgamenum");
   IMPCMD (SLIMP_SAVEDGAMENAME, L"savedgamename");
   IMPCMD (SLIMP_SAVEDGAMEINFO, L"savedgameinfo");
   IMPCMD (SLIMP_SAVEDGAMEFILESENUM, L"savedgamefilesenum");
   IMPCMD (SLIMP_SAVEDGAMEFILESNUM, L"savedgamefilesnum");
   IMPCMD (SLIMP_SAVEDGAMEFILESDELETE, L"savedgamefilesdelete");
   IMPCMD (SLIMP_SAVEDGAMEFILESNAME, L"savedgamefilesname");

   IMPCMD (SLIMP_BINARYDATASAVE, L"binarydatasave");
   IMPCMD (SLIMP_BINARYDATAREMOVE, L"binarydataremove");
   IMPCMD (SLIMP_BINARYDATARENAME, L"binarydatarename");
   IMPCMD (SLIMP_BINARYDATAENUM, L"binarydataenum");
   IMPCMD (SLIMP_BINARYDATANUM, L"binarydatanum");
   IMPCMD (SLIMP_BINARYDATAGETNUM, L"binarydatagetnum");
   IMPCMD (SLIMP_BINARYDATALOAD, L"binarydataload");
   IMPCMD (SLIMP_BINARYDATAQUERY, L"binarydataquery");

   IMPCMD (SLIMP_HELPARTICLE, L"helparticle");
   IMPCMD (SLIMP_HELPCONTENTS, L"helpcontents");
   IMPCMD (SLIMP_HELPSEARCH, L"helpsearch");

   IMPCMD (SLIMP_EMAILSEND, L"emailsend");

   IMPCMD (SLIMP_PERFORMANCEMEMORY, L"performancememory");
   IMPCMD (SLIMP_PERFORMANCECPU, L"performancecpu");
   IMPCMD (SLIMP_PERFORMANCENETWORK, L"performancenetwork");
   IMPCMD (SLIMP_PERFORMANCEDISK, L"performancedisk");
   IMPCMD (SLIMP_PERFORMANCEGUI, L"performancegui");
   IMPCMD (SLIMP_PERFORMANCETHREADS, L"performancethreads");
   IMPCMD (SLIMP_PERFORMANCEHANDLES, L"performancehandles");
   IMPCMD (SLIMP_PERFORMANCECPUOBJECT, L"performancecpuobject");
   IMPCMD (SLIMP_PERFORMANCECPUOBJECTQUERY, L"performancecpuobjectquery");

   IMPCMD (SLIMP_TEXTLOG, L"textlog");
   IMPCMD (SLIMP_TEXTLOGNOAUTO, L"textlognoauto");
   IMPCMD (SLIMP_TEXTLOGNUMLINES, L"textlognumlines");
   IMPCMD (SLIMP_TEXTLOGENUM, L"textlogenum");
   IMPCMD (SLIMP_TEXTLOGDELETE, L"textlogdelete");
   IMPCMD (SLIMP_TEXTLOGREAD, L"textlogread");
   IMPCMD (SLIMP_TEXTLOGAUTOSET, L"textlogautoset");
   IMPCMD (SLIMP_TEXTLOGAUTOGET, L"textlogautoget");
   IMPCMD (SLIMP_TEXTLOGENABLESET, L"textlogenableset");
   IMPCMD (SLIMP_TEXTLOGENABLEGET, L"textlogenableget");
   IMPCMD (SLIMP_TEXTLOGSEARCH, L"textlogsearch");
}

CMIFLSocketServer::~CMIFLSocketServer ()
{
   // do nothing for now
}


/*****************************************************************************
CMIFLSocketServer - This is a test callback
*/
DWORD CMIFLSocketServer::LibraryNum (void)
{
   return 12;
}

BOOL CMIFLSocketServer::LibraryEnum (DWORD dwNum, PMASLIB pLib)
{
   if (dwNum >= LibraryNum())
      return FALSE;

   switch (dwNum) {
   case 11:  // VM library
      return VMLibraryEnum (pLib);
   case 10:  // server library
      memset (pLib, 0, sizeof(*pLib));
      pLib->hResourceInst = ghInstance;
      pLib->dwResource = IDR_MIFLSERVLIB;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszName = L"Server library";
      pLib->pszDescShort = L"Library that's necessary for using the IF server's features.";
      return TRUE;
   case 9:  // basic IF library
      memset (pLib, 0, sizeof(*pLib));
      pLib->hResourceInst = ghInstance;
      pLib->dwResource = IDR_MIFLBASICIF;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszName = L"Basic interactive fiction library";
      pLib->pszDescShort = L"This library provides the basic functionality for interactive "
         L"fiction, such as rooms, objects, and parsing. It's highly recommended that "
         L"you use other, otherwise you need to write your own.";
      return TRUE;

   case 8:  // basic ad,om library
      memset (pLib, 0, sizeof(*pLib));
      pLib->hResourceInst = ghInstance;
      pLib->dwResource = IDR_MIFLBASICADMIN;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszName = L"Basic administration library";
      pLib->pszDescShort = L"The library provides command and features that let "
         L"administrators remotely monitor and administer an online IF title.";
      return TRUE;

   case 7:  // basic com library
      memset (pLib, 0, sizeof(*pLib));
      pLib->hResourceInst = ghInstance;
      pLib->dwResource = IDR_MIFLBASICCOM;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszName = L"Basic communications library";
      pLib->pszDescShort = L"The basic communications library provides internal "
         L"E-mail functionaity (sometimes called mud-mail) along with a bulletin "
         L"board system.";
      return TRUE;

   case 6:  // basic RPG library
      memset (pLib, 0, sizeof(*pLib));
      pLib->hResourceInst = ghInstance;
      pLib->dwResource = IDR_MIFLBASICRPG;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszName = L"Basic RPG library";
      pLib->pszDescShort = L"Provides standard RPG functionality, such as attributes, "
         L"skills, combat, NPC AI, and some MUD functionality. It's highly recommended that "
         L"you use other, otherwise you need to write your own.";
      return TRUE;

   case 5:  // basic AI library
      memset (pLib, 0, sizeof(*pLib));
      pLib->hResourceInst = ghInstance;
      pLib->dwResource = IDR_MIFLBASICAI;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszName = L"Basic artificial intelligence library";
      pLib->pszDescShort = L"The artificial intelligence library provides AI routines "
         L"for NPCs, automatically placing it into the cMoble class. You should use "
         L"this library for AI routines.";
      return TRUE;

   case 4:  // basic fantasy library
      memset (pLib, 0, sizeof(*pLib));
      pLib->hResourceInst = ghInstance;
      pLib->dwResource = IDR_MIFLBASICFANTASY;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszName = L"Basic fantasy library";
      pLib->pszDescShort = L"The Basic Fantasy library includes classes to handle "
         L"standard fantasy constructs, such as races (Elves, Dwarves, etc.), magic, "
         L"equipment, etc.";
      return TRUE;

   case 3:  // basic monster library
      memset (pLib, 0, sizeof(*pLib));
      pLib->hResourceInst = ghInstance;
      pLib->dwResource = IDR_MIFLBASICMONSTERS;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszName = L"Basic monsters library";
      pLib->pszDescShort = L"This library contains the cMonster class and "
         L"several sample monsters.";
      return TRUE;

   case 2:  // extra objects
      memset (pLib, 0, sizeof(*pLib));
      pLib->hResourceInst = ghInstance;
      pLib->dwResource = IDR_MIFLEXTRAOBJECTS;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszName = L"Extra objects";
      pLib->pszDescShort = L"This library contains extra objects, like "
         L"furniture and food items.";
      return TRUE;

   case 1:  // reading room
      memset (pLib, 0, sizeof(*pLib));
      pLib->hResourceInst = ghInstance;
      pLib->dwResource = IDR_MIFLREADINGROOM;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszName = L"Reading-room library";
      pLib->pszDescShort = L"Code in this library creates a \"reading room\" where the "
         L"player can read their side-stories, apart from the world.";
      return TRUE;

   case 0:  // test world
      memset (pLib, 0, sizeof(*pLib));
      pLib->hResourceInst = ghInstance;
      pLib->dwResource = IDR_MIFLTESTWORLD;
      pLib->fDefaultOn = TRUE;   // NOTE - make sure that default on
      pLib->pszName = L"Sample world";
      pLib->pszDescShort = L"This is a sample world whose code you can look through "
         L"to learn how to create your own CircumReality world. Make sure to read "
         L"the library's description for information about how to compile the test world.";
      return TRUE;
   }

   return FALSE;
}


DWORD CMIFLSocketServer::ResourceNum (void)
{
   return 20;
}

BOOL CMIFLSocketServer::ResourceEnum (DWORD dwNum, PMASRES pRes)
{
   memset (pRes, 0, sizeof(*pRes));

   switch (dwNum) {
   case 0:
      pRes->pszName = CircumrealityImage();
      pRes->pszDescShort = L"Display images (.jpg or .bmp) on the player's computer.";
      return TRUE;
   case 1:
      pRes->pszName = Circumreality3DScene();
      pRes->pszDescShort = L"Displays a full scene from a " APPLONGNAMEW L" file.";
      return TRUE;
   case 2:
      pRes->pszName = Circumreality3DObjects();
      pRes->pszDescShort = L"Displays a blank scene filled with a couple of objects of "
         L"your choosing.";
      return TRUE;
   case 3:
      pRes->pszName = CircumrealityTitle();
      pRes->pszDescShort = L"Draws a title over an optional background.";
      return TRUE;
   case 4:
      pRes->pszName = CircumrealityText();
      pRes->pszDescShort = L"Displays scrollable text that can include edit fields and links.";
      return TRUE;
   case 5:
      pRes->pszName = CircumrealityHelp();
      pRes->pszDescShort = L"Displays online help to the player.";
      return TRUE;

   case 6:
      pRes->pszName = CircumrealityWave();
      pRes->pszDescShort = L"Plays a wave file (.wav) on the player's computer.";
      return TRUE;
   case 7:
      pRes->pszName = CircumrealityMusic();
      pRes->pszDescShort = L"Plays a music file (.mid) on the player's computer.";
      return TRUE;
   case 8:
      pRes->pszName = CircumrealityVoice();
      pRes->pszDescShort = L"Specified a text-to-speech voice (file) to use for speaking.";
      return TRUE;
   case 9:
      pRes->pszName = CircumrealityTransPros();
      pRes->pszDescShort = L"Transplant prosody from a recording onto text-to-speech, to make it sound more realistic.";
      return TRUE;
   case 10:
      pRes->pszName = CircumrealityAmbient();
      pRes->pszDescShort = L"Specify background sounds (such as bird chirps and ocean waves) to "
         L"play in a room.";
      return TRUE;
   case 11:
      pRes->pszName = CircumrealityConvScript();
      pRes->pszDescShort = L"Create a scripted conversation between 2-4 NPCs.";
      return TRUE;
   case 12:
      pRes->pszName = CircumrealityCutScene();
      pRes->pszDescShort = L"Construct a sequence of dialog, sounds, and images into a cut scene.";
      return TRUE;
   case 13:
      pRes->pszName = CircumrealitySpeakScript();
      pRes->pszDescShort = L"Used to create a combination of a single NPC's speech, narration, and emotes.";
      return TRUE;

   case 14:
      pRes->pszName = CircumrealityGeneralMenu();
      pRes->pszDescShort = L"Display a list of choices to the user, each choice corresponding to a command.";
      return TRUE;
   case 15:
      pRes->pszName = CircumrealityVerbWindow();
      pRes->pszDescShort = L"Display a palette of verbs that the user can use to save typing.";
      return TRUE;

   case 16:
      pRes->pszName = CircumrealityNLPRuleSet();
      pRes->pszDescShort = L"Lets you modify NLP (natural language processing) rules for the command line "
         L"or NPC conversations.";
      return TRUE;

   case 17:
      pRes->pszName = CircumrealityTitleInfo();
      pRes->pszDescShort = L"Specifies if the IF title is single-player or run on a server, "
         L"and the server location. You need on TitleInfo resource named rTitleInfo. "
         L"You can include others and use them as links to other online worlds.";
      return TRUE;

   case 18:
      pRes->pszName = CircumrealityVoiceChatInfo();
      pRes->pszDescShort = L"Controls what voice chat features are available to a user.";
      return TRUE;

   case 19:
      pRes->pszName = CircumrealityLotsOfText();
      pRes->pszDescShort = L"Use to include the raw text from entire E-books.";
      return TRUE;
   }

   return FALSE;
}

PCMMLNode2 CMIFLSocketServer::ResourceEdit (HWND hWnd, PWSTR pszType, LANGID lid, PCMMLNode2 pIn, BOOL fReadOnly,
                                           PCMIFLProj pProj, PCMIFLLib pLib, PCMIFLResource pRes)
{
   if (!_wcsicmp(pszType, CircumrealityImage()))
      return ResImageEdit (hWnd, lid, pIn, fReadOnly);
   else if (!_wcsicmp(pszType, CircumrealityText()))
      return ResTextEdit (hWnd, lid, pIn, fReadOnly);
   else if (!_wcsicmp(pszType, CircumrealityLotsOfText()))
      return ResLotsOfTextEdit (hWnd, lid, pIn, fReadOnly);
   else if (!_wcsicmp(pszType, CircumrealityHelp()))
      return ResHelpEdit (hWnd, lid, pIn, fReadOnly);
   else if (!_wcsicmp(pszType, CircumrealityGeneralMenu()))
      return ResMenuEdit (hWnd, lid, pIn, fReadOnly);
   else if (!_wcsicmp(pszType, CircumrealityNLPRuleSet()))
      return NLPRuleSetEdit (hWnd, lid, pIn, fReadOnly, pProj);
   else if (!_wcsicmp(pszType, CircumrealityWave()))
      return ResWaveEdit (hWnd, lid, pIn, fReadOnly);
   else if (!_wcsicmp(pszType, CircumrealityCutScene()))
      return ResCutSceneEdit (hWnd, lid, pIn, fReadOnly, pProj, pLib, pRes);
   else if (!_wcsicmp(pszType, CircumrealitySpeakScript()))
      return ResSpeakScriptEdit (hWnd, lid, pIn, fReadOnly, pProj, pLib, pRes);
   else if (!_wcsicmp(pszType, CircumrealityConvScript()))
      return ResConvScriptEdit (hWnd, lid, pIn, fReadOnly, pProj, pLib, pRes);
   else if (!_wcsicmp(pszType, CircumrealityMusic()))
      return ResMusicEdit (hWnd, lid, pIn, fReadOnly);
   else if (!_wcsicmp(pszType, CircumrealityVoice()))
      return ResVoiceEdit (hWnd, lid, pIn, fReadOnly);
   else if (!_wcsicmp(pszType, CircumrealityTransPros())) {
      CTTSTransPros TP;

      // original text if possible
      PWSTR pszText = NULL;
      if (pIn) {
         PCMMLNode2 pSub = NULL, pSub2;
         PWSTR psz;
         pIn->ContentEnum (pIn->ContentFind (L"OrigText"), &psz, &pSub);
         if (pSub)
            pSub->ContentEnum (0, &pszText, &pSub2);
         if (!pszText)
            pIn->ContentEnum (0, &pszText, &pSub);
      }

      DWORD dwRet;
      CMem mem;
      {
         CEscWindow cWindow;
         RECT r;
         DialogBoxLocation2 (hWnd, &r);
         cWindow.Init (ghInstance, hWnd, EWS_FIXEDSIZE, &r);

         // UI
         dwRet = TP.Dialog (&cWindow, NULL, pszText, NULL, &mem);
      }
      if (dwRet != 2)
         return NULL;

      PCMMLNode2 pRet = CircumrealityParseMML ((PWSTR)mem.p);
      if (!pRet) {
         // was just text
         pRet = new CMMLNode2;
         pRet->ContentAdd ((PWSTR)mem.p);
      }
      if (pRet)
         pRet->NameSet (CircumrealityTransPros());

      return pRet;
   }
   else if (!_wcsicmp(pszType, Circumreality3DScene()) || !_wcsicmp(pszType, Circumreality3DObjects())) {
      if (!gfM3DInit) {
         CProgress Progress;
         Progress.Start (NULL, "Starting 3D...", TRUE);
         M3DInit (DEFAULTRENDERSHARD, FALSE, &Progress);
         MyCacheM3DFileInit(DEFAULTRENDERSHARD);
         gfM3DInit = TRUE;
      }

      CRenderScene rs;
      rs.m_fLoadFromFile = !_wcsicmp(pszType, Circumreality3DScene());
      rs.MMLFrom (pIn);

      if (rs.Edit (DEFAULTRENDERSHARD, hWnd, lid, fReadOnly, gpMIFLProj, TRUE))
         return rs.MMLTo();
      else
         return NULL;   // no change
   }
   else if (!_wcsicmp(pszType, CircumrealityTitleInfo())) {
      CResTitleInfo rti;
      rti.MMLFrom (pIn);

      if (rti.Edit (hWnd, lid, fReadOnly))
         return rti.MMLTo();
      else
         return NULL;   // no change
   }
   else if (!_wcsicmp(pszType, CircumrealityAmbient())) {
      CAmbient am;
      am.MMLFrom (pIn);

      if (am.Edit (hWnd, fReadOnly))
         return am.MMLTo();
      else
         return NULL;   // no change
   }
   else if (!_wcsicmp(pszType, CircumrealityTitle())) {
      if (!gfM3DInit) {
         CProgress Progress;
         Progress.Start (NULL, "Starting 3D...", TRUE);
         M3DInit (DEFAULTRENDERSHARD, FALSE, &Progress);
         MyCacheM3DFileInit(DEFAULTRENDERSHARD);
         gfM3DInit = TRUE;
      }

      CRenderTitle rs;
      rs.MMLFrom (pIn);

      if (rs.Edit (DEFAULTRENDERSHARD, hWnd, lid, fReadOnly, gpMIFLProj))
         return rs.MMLTo();
      else
         return NULL;   // no change
   }
   else if (!_wcsicmp(pszType, CircumrealityVerbWindow())) {
      CResVerb rs;
      rs.MMLFrom (pIn);

      if (rs.Edit (hWnd, lid, fReadOnly, gpMIFLProj))
         return rs.MMLTo();
      else
         return NULL;   // no change
   }
   else if (!_wcsicmp(pszType, CircumrealityVoiceChatInfo())) {
      CResVoiceChatInfo rs;
      rs.MMLFrom (pIn);

      if (rs.Edit (hWnd, fReadOnly))
         return rs.MMLTo();
      else
         return NULL;   // no change
   }

   // else
   return NULL;
}

BOOL CMIFLSocketServer::FileSysSupport (void)
{
   return FALSE;
}

BOOL CMIFLSocketServer::FileSysLibOpenDialog (HWND hWnd, PWSTR pszFile, DWORD dwChars, BOOL fSave)
{
   return FALSE;
}

PCMMLNode2 CMIFLSocketServer::FileSysOpen (PWSTR pszFile)
{
   return NULL;
}

BOOL CMIFLSocketServer::FileSysSave (PWSTR pszFile, PCMMLNode2 pNode)
{
   return TRUE;
}

BOOL CMIFLSocketServer::FuncImportIsValid (PWSTR pszName, DWORD *pdwParams)
{
   // call the VM for standard library
   if (VMFuncImportIsValid (pszName, pdwParams))
      return TRUE;

   DWORD *pdw = (DWORD*)m_hImport.Find (pszName, FALSE); // know that is lower
   if (pdw) switch (*pdw) {
   case SLIMP_CONNECTIONDISCONNECT: // function that will disconnect and delete connection
   case SLIMP_CONNECTIONQUEUEDTOSEND: // returns number of bytes queued to send
   case SLIMP_CONNECTIONSLIMIT: // limit the number of connections
   case SLIMP_VOICECHATALLOWWAVES:  // allow waves
      *pdwParams = 1;
      return TRUE;
   case SLIMP_CONNECTIONSEND: // function to send a message out to a given connection
   case SLIMP_CONNECTIONINFOSET: // sets piece of information about the connection (such as user name)
   case SLIMP_CONNECTIONBAN:  // ban
      *pdwParams = 3;
      return TRUE;
   case SLIMP_RENDERCACHELIMITS: // render cache limits
   case SLIMP_CONNECTIONSENDVOICECHAT: // send voice chat
      *pdwParams = 5;
      return TRUE;
   case SLIMP_CONNECTIONINFOGET: // gets piece of information about the connection
   case SLIMP_RENDERCACHESEND: // send a cached render/TTS to the player's client
      *pdwParams = 2;
      return TRUE;
   case SLIMP_CONNECTIONENUM: // enumerates all the connections, as numbers or objects
   case SLIMP_SHARDPARAM:
      *pdwParams = 0;
      return TRUE;

   case SLIMP_SHUTDOWNIMMEDIATELY:
      *pdwParams = 1;
      return TRUE;

   case SLIMP_EMAILSEND:
      *pdwParams = 9;
      return TRUE;

   case SLIMP_PERFORMANCEMEMORY:
   case SLIMP_PERFORMANCECPUOBJECT:
   case SLIMP_PERFORMANCECPUOBJECTQUERY:
      *pdwParams = 1;
      return TRUE;

   case SLIMP_PERFORMANCEGUI:
   case SLIMP_PERFORMANCETHREADS:
   case SLIMP_PERFORMANCEHANDLES:
   case SLIMP_PERFORMANCECPU:
   case SLIMP_PERFORMANCENETWORK:
   case SLIMP_PERFORMANCEDISK:
      *pdwParams = 0;
      return TRUE;

   case SLIMP_TEXTLOG:  // log text using auto
      *pdwParams = -1;  // since can optionally add object
      return TRUE;

   case SLIMP_TEXTLOGNOAUTO:  // log text with parameters
      *pdwParams = 5;
      return TRUE;

   case SLIMP_TEXTLOGNUMLINES:  // return number of lines
   case SLIMP_TEXTLOGDELETE:  // delete file
   case SLIMP_TEXTLOGAUTOGET:  // get an auto value
   case SLIMP_TEXTLOGENABLESET:   // set enable
      *pdwParams = 1;
      return TRUE;

   case SLIMP_TEXTLOGENUM:   // enumerates logs
   case SLIMP_TEXTLOGENABLEGET:   // get enable
      *pdwParams = 0;
      return TRUE;

   case SLIMP_TEXTLOGREAD:  // reads a line
   case SLIMP_TEXTLOGAUTOSET:  // set auto value
      *pdwParams = 2;
      return TRUE;

   case SLIMP_TEXTLOGSEARCH:   // search through text log
      *pdwParams = 9;
      return TRUE;

   case SLIMP_DATABASEOBJECTADD: // add object to database
   case SLIMP_DATABASEOBJECTSAVE: // saves a checked out object
   case SLIMP_DATABASEOBJECTCHECKOUT: // checks out an object
   case SLIMP_DATABASEOBJECTDELETE: // deletes an object that's not checked out
   case SLIMP_DATABASEPROPERTYADD: // adds a property to be indexed
   case SLIMP_DATABASEPROPERTYREMOVE: // removes a property that is to be indexed
   case SLIMP_DATABASEOBJECTNUM:
   case SLIMP_DATABASEOBJECTQUERYCHECKOUT2:
      *pdwParams = 2;
      return TRUE;

   case SLIMP_DATABASEPROPERTYGET: // gets 1 or more properties from one or more objects in the database
   case SLIMP_DATABASEQUERY: // runs a query on the database
   case SLIMP_DATABASEOBJECTQUERYCHECKOUT:
   case SLIMP_DATABASEOBJECTGET:
      *pdwParams = 3;
      return TRUE;

   case SLIMP_DATABASEPROPERTYSET: // sets 1 or more properties from one or more objects in the database
      *pdwParams = 4;
      return TRUE;

   case SLIMP_DATABASESAVE: // saves the database to disk
   case SLIMP_DATABASEOBJECTCHECKIN: // checks in an object
      *pdwParams = -1;
      return TRUE;

   case SLIMP_DATABASEPROPERTYENUM: // enumerate properties
   case SLIMP_DATABASEBACKUP: // saves the database, and backs up to another directory
   case SLIMP_DATABASEOBJECTENUMCHECKOUT:
   case SLIMP_SAVEDGAMEENUM:
   case SLIMP_SAVEDGAMENUM:
   case SLIMP_SAVEDGAMEFILESNAME:
   case SLIMP_SAVEDGAMEFILESDELETE:
      *pdwParams = 1;
      return TRUE;

   case SLIMP_SAVEDGAMEFILESNUM:
   case SLIMP_SAVEDGAMEFILESENUM:
   case SLIMP_NLPPARSERENUM:
   case SLIMP_BINARYDATANUM:
      *pdwParams = 0;
      return TRUE;

   case SLIMP_NLPPARSERREMOVE:
   case SLIMP_NLPVERBFORM:
   case SLIMP_BINARYDATAREMOVE:
   case SLIMP_BINARYDATAENUM:
   case SLIMP_BINARYDATAGETNUM:
   case SLIMP_BINARYDATALOAD:
   case SLIMP_BINARYDATAQUERY:
      *pdwParams = 1;
      return TRUE;

   case SLIMP_SAVEDGAMEREMOVE:
   case SLIMP_SAVEDGAMENAME:
   case SLIMP_SAVEDGAMEINFO:
   case SLIMP_NLPRULESETREMOVE:
   case SLIMP_NLPPARSERCLONE:
   case SLIMP_NLPRULESETENABLEGET:
   case SLIMP_NLPRULESETENABLEALL:
   case SLIMP_NLPRULESETENUM:
   case SLIMP_BINARYDATASAVE:
   case SLIMP_BINARYDATARENAME:
      *pdwParams = 2;
      return TRUE;

   case SLIMP_SAVEDGAMELOAD:
   case SLIMP_NLPPARSE:
   case SLIMP_NLPRULESETADD:
   case SLIMP_NLPRULESETENABLESET:
   case SLIMP_NLPRULESETEXISTS:
      *pdwParams = 3;
      return TRUE;

   case SLIMP_SAVEDGAMESAVE:
      *pdwParams = 5;
      return TRUE;

   case SLIMP_NLPNOUNCASE:
      *pdwParams = -1;
      return TRUE;

   case SLIMP_HELPARTICLE:
   case SLIMP_HELPCONTENTS:
   case SLIMP_HELPSEARCH:
      *pdwParams = 3;
      return TRUE;
   }

   return FALSE;
}

BOOL CMIFLSocketServer::FuncImport (PWSTR pszName, PCMIFLVM pVM, PCMIFLVMObject pObject,
   PCMIFLVarList plParams, DWORD dwCharStart, DWORD dwCharEnd, PCMIFLVar pRet)
{
   // call the VM for standard library
   CMIFLVarLValue varLValue;
   if (pVM->FuncImport (pszName, pObject, plParams, dwCharStart, dwCharEnd, &varLValue)) {
      pRet->Set (&varLValue.m_Var);
      return TRUE;
   }

   // if VM doesn't match then fail
   if (!gpMainWindow || (gpMainWindow->VMGet() != pVM)) {
      // BUGFIX - If got here and there window's wrong, then create a new one
      // Otherwise, when have functions in the constructor that call into this
      // code won't be able to access
      InternalTestVMNew (pVM, TRUE);

      if (!gpMainWindow || (gpMainWindow->VMGet() != pVM))
         return FALSE;
   }

   DWORD *pdw = (DWORD*)m_hImport.Find (pszName, FALSE); // know that is lower
   if (pdw) switch (*pdw) {

   case SLIMP_NLPPARSERENUM: // enumerate parsers
      return NLPParserEnum (pVM, plParams, pRet);

   case SLIMP_NLPPARSERREMOVE: // Delete parsers
      return NLPParserRemove (pVM, plParams, pRet);

   case SLIMP_NLPPARSERCLONE: // Clone parsers
      return NLPParserClone (pVM, plParams, pRet);

   case SLIMP_NLPPARSE: // parse using parser
      return NLPParse (pVM, plParams, pRet);

   case SLIMP_NLPRULESETENUM: // enumerate rule sets
      return NLPRuleSetEnum (pVM, plParams, pRet);

   case SLIMP_NLPRULESETADD: // add rule set
      return NLPRuleSetAdd (pVM, plParams, pRet);

   case SLIMP_NLPRULESETENABLEGET: // enable rule-set get
      return NLPRuleSetEnableGet (pVM, plParams, pRet);

   case SLIMP_NLPRULESETEXISTS: // sees if rule set exists for the language
      return NLPRuleSetExists (pVM, plParams, pRet);

   case SLIMP_NLPRULESETENABLEALL: // enable/disable all rule-sets
      return NLPRuleSetEnableAll (pVM, plParams, pRet);

   case SLIMP_NLPRULESETENABLESET: // enable rule set
      return NLPRuleSetEnableSet (pVM, plParams, pRet);

   case SLIMP_NLPRULESETREMOVE: // remove a rule sset
      return NLPRuleSetRemove (pVM, plParams, pRet);

   case SLIMP_NLPVERBFORM:
      return NLPVerbForm (pVM, plParams, pRet);

   case SLIMP_NLPNOUNCASE:
      return NLPNounCase (pVM, plParams, pRet);

   case SLIMP_SAVEDGAMEFILESENUM:
      return SavedGameFilesEnum (pVM, plParams, pRet);

   case SLIMP_SAVEDGAMEFILESNUM:
      return SavedGameFilesNum (pVM, plParams, pRet);

   case SLIMP_SAVEDGAMEFILESNAME:
      return SavedGameFilesName (pVM, plParams, pRet);

   case SLIMP_SAVEDGAMEFILESDELETE:
      return SavedGameFilesDelete (pVM, plParams, pRet);

   case SLIMP_SAVEDGAMEENUM:
      return SavedGameEnum (pVM, plParams, pRet);

   case SLIMP_SAVEDGAMEREMOVE:
      return SavedGameRemove (pVM, plParams, pRet);

   case SLIMP_SAVEDGAMESAVE:
      return SavedGameSave (pVM, plParams, pRet);

   case SLIMP_SAVEDGAMELOAD:
      return SavedGameLoad (pVM, plParams, pRet);

   case SLIMP_SAVEDGAMENUM:
      return SavedGameNum (pVM, plParams, pRet);

   case SLIMP_SAVEDGAMENAME:
      return SavedGameName (pVM, plParams, pRet);

   case SLIMP_SAVEDGAMEINFO:
      return SavedGameInfo (pVM, plParams, pRet);

   case SLIMP_BINARYDATASAVE:
      return BinaryDataSave (pVM, plParams, pRet);

   case SLIMP_BINARYDATAREMOVE:
      return BinaryDataRemove (pVM, plParams, pRet);

   case SLIMP_BINARYDATARENAME:
      return BinaryDataRename (pVM, plParams, pRet);

   case SLIMP_BINARYDATAENUM:
      return BinaryDataEnum (pVM, plParams, pRet);

   case SLIMP_BINARYDATANUM:
      return BinaryDataNum (pVM, plParams, pRet);

   case SLIMP_BINARYDATAGETNUM:
      return BinaryDataGetNum (pVM, plParams, pRet);

   case SLIMP_BINARYDATALOAD:
      return BinaryDataLoad (pVM, plParams, pRet);

   case SLIMP_BINARYDATAQUERY:
      return BinaryDataQuery (pVM, plParams, pRet);


   case SLIMP_HELPARTICLE:
      return HelpArticle (pVM, plParams, pRet);
   case SLIMP_HELPCONTENTS:
      return HelpContents (pVM, plParams, pRet);
   case SLIMP_HELPSEARCH:
      return HelpSearch (pVM, plParams, pRet);


   case SLIMP_DATABASEOBJECTADD: // add object to database
   case SLIMP_DATABASEOBJECTSAVE: // saves a checked out object
   case SLIMP_DATABASEOBJECTCHECKOUT: // checks out an object
   case SLIMP_DATABASEOBJECTCHECKIN: // checks in an object
   case SLIMP_DATABASEOBJECTDELETE: // deletes an object that's not checked out
      // param 1 = database name
      // param 2 = object
      // param 3 (checkin only, and option) = true if dont want to save, false if save
      // param 4 (checkin only, and option) = true then DONT delete, false (or undef) delete
      {
         PCMIFLVar pVar = plParams->Get(1);
         if (!gpMainWindow->m_pDatabase || (plParams->Num() < 2)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }
         if (*pdw != SLIMP_DATABASEOBJECTDELETE)
            if (pVar->TypeGet() != MV_OBJECT) {
               pRet->SetBOOL (FALSE);
               return TRUE;
            }

         PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
         GUID g = pVar->GetGUID();

         PCDatabaseCat pCat = gpMainWindow->m_pDatabase->Get (ps->Get());
         ps->Release();
         PCMIFLVMObject pObj = pVM->ObjectFind (&g);
         BOOL fNoSave, fDelete;

         BOOL fRet = FALSE;
         switch (*pdw) {
         case SLIMP_DATABASEOBJECTADD:
            fRet = (pCat && pObj) ? pCat->ObjectAdd (pObj) : FALSE;
            break;
         case SLIMP_DATABASEOBJECTSAVE:
            fRet = (pCat && pObj) ? pCat->ObjectSave (pObj) : FALSE;
            break;
         case SLIMP_DATABASEOBJECTCHECKOUT: // checks out an object
            fRet = pCat ? pCat->ObjectCheckOut (&g) : FALSE;
            break;
         case SLIMP_DATABASEOBJECTCHECKIN: // checks in an object
            pVar = plParams->Get(2);
            fNoSave = pVar ? pVar->GetBOOL(pVM) : FALSE;

            pVar = plParams->Get(3);   // BUGFIX - Was set to 2
            fDelete = pVar ? !pVar->GetBOOL(pVM) : TRUE;

            fRet = (pCat && pObj) ? pCat->ObjectCheckIn (pObj, fNoSave, fDelete, pVM) : FALSE;
            break;
         case SLIMP_DATABASEOBJECTDELETE: // deletes an object that's not checked out
            fRet = pCat ? pCat->ObjectDelete (pVar) : FALSE;
            break;
         } // switch
         pRet->SetBOOL(fRet);
      }
      return TRUE;

   case SLIMP_DATABASEPROPERTYGET: // gets 1 or more properties from one or more objects in the database
      // param 1 = database name
      // param 2 = object, or list of objects
      // param 3 = parameter string, or list of parmeter strings
      {
         if (!gpMainWindow->m_pDatabase || (plParams->Num() != 3)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
         PCDatabaseCat pCat = gpMainWindow->m_pDatabase->Get (ps->Get());
         ps->Release();

         if (pCat)
            pCat->ObjectAttribGet (plParams->Get(1), plParams->Get(2), pRet, NULL);
         else
            pRet->SetBOOL (FALSE);
      }
      return TRUE;

   case SLIMP_DATABASEQUERY: // runs a query on the database
      // param 1 = database name
      // param 2 = constraints... see CDatabaseCat::ObjectEnum()
      // param 3 = character to disambiguate constraints
      {
         if (!gpMainWindow->m_pDatabase || (plParams->Num() != 3)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
         PCDatabaseCat pCat = gpMainWindow->m_pDatabase->Get (ps->Get());
         ps->Release();

         if (pCat)
            pCat->ObjectEnum (plParams->Get(1), plParams->Get(2)->GetChar(pVM), pRet);
         else
            pRet->SetBOOL (FALSE);
      }
      return TRUE;


   case SLIMP_DATABASEPROPERTYSET: // sets 1 or more properties from one or more objects in the database
      // param 1 = database name
      // param 2 = object or list of objects
      // param 3 = attribute string or list of attribute strings
      // param 4 = table of properties. See CDatabaseCat::ObjectAttribSet()
      {
         if (!gpMainWindow->m_pDatabase || (plParams->Num() != 4)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
         PCDatabaseCat pCat = gpMainWindow->m_pDatabase->Get (ps->Get());
         ps->Release();

         if (pCat)
            pRet->SetBOOL(pCat->ObjectAttribSet (plParams->Get(1), plParams->Get(2), plParams->Get(3)));
         else
            pRet->SetBOOL (FALSE);
      }
      return TRUE;



   case SLIMP_DATABASESAVE: // saves the database to disk
      // param 1 (optional) = database name
      {
         if (!gpMainWindow->m_pDatabase) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         if (plParams->Get(0)) {
            PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
            PCDatabaseCat pCat = gpMainWindow->m_pDatabase->Get (ps->Get());
            ps->Release();

            if (pCat)
               pRet->SetBOOL(pCat->SaveAll());
            else
               pRet->SetBOOL (FALSE);
         }
         else
            pRet->SetBOOL (gpMainWindow->m_pDatabase->SaveAll());
      }
      return TRUE;

   case SLIMP_DATABASEPROPERTYADD: // adds a property to be indexed
   case SLIMP_DATABASEPROPERTYREMOVE: // removes a property that is to be indexed
      // param 1 = database name
      // param 2 = property name
      {
         if (!gpMainWindow->m_pDatabase || (plParams->Num() != 2)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
         PCDatabaseCat pCat = gpMainWindow->m_pDatabase->Get (ps->Get());
         ps->Release();

         if (pCat) {
            BOOL fRet;
            ps = plParams->Get(1)->GetString(pVM);
            if (*pdw == SLIMP_DATABASEPROPERTYADD)
               fRet = pCat->CacheAdd (ps->Get());
            else
               fRet = pCat->CacheRemove (ps->Get());
            pRet->SetBOOL(fRet);
            ps->Release();
         }
         else
            pRet->SetBOOL (FALSE);
      }
      return TRUE;

   case SLIMP_DATABASEPROPERTYENUM: // Enumerate properties
      // param 1 = database name
      {
         if (!gpMainWindow->m_pDatabase || (plParams->Num() != 1)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
         PCDatabaseCat pCat = gpMainWindow->m_pDatabase->Get (ps->Get());
         ps->Release();

         if (pCat)
            pCat->CacheEnum (pRet);
         else
            pRet->SetBOOL (FALSE);
      }
      return TRUE;

   case SLIMP_DATABASEOBJECTQUERYCHECKOUT: // sees if an object is checked out
      // param 1 = database name
      // param 2 = object
      // param 3 = TRUE if check for a remote VM connection, FALSE if local only
      // returns, 2 if checked out by this process, 1 by another, 0 if not checked out, -1 if not in database
      {
         if (!gpMainWindow->m_pDatabase || (plParams->Num() != 3)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVar pvObject = plParams->Get(1);
         if (pvObject->TypeGet() != MV_OBJECT) {
            pRet->SetBOOL (FALSE);
            return TRUE;   // BUGFIX - Was FALSE
         }
         GUID gObject = pvObject->GetGUID();

         PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
         PCDatabaseCat pCat = gpMainWindow->m_pDatabase->Get (ps->Get());
         ps->Release();

         if (pCat)
            pRet->SetDouble (pCat->ObjectQueryCheckOut (&gObject, plParams->Get(2)->GetBOOL(pVM)));
         else
            pRet->SetBOOL (FALSE);
      }
      return TRUE;

   case SLIMP_DATABASEOBJECTQUERYCHECKOUT2: // sees if an object is checked out to any database
      // param 1 = object
      // param 2 = TRUE if check for a remote VM connection, FALSE if local only
      // returns string for the database, or NULL
      {
         if (!gpMainWindow->m_pDatabase || (plParams->Num() != 2)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVar pvObject = plParams->Get(0);
         if (pvObject->TypeGet() != MV_OBJECT) {
            pRet->SetBOOL (FALSE);
            return TRUE;   // BUGFIX - Was returning FALSE
         }
         GUID gObject = pvObject->GetGUID();

         BOOL fAny = plParams->Get(1)->GetBOOL(pVM);

         PWSTR psz = gpMainWindow->m_pDatabase->ObjectQueryCheckOut (&gObject, fAny);
         if (psz)
            pRet->SetString (psz);
         else
            pRet->SetNULL();
      }
      return TRUE;

   case SLIMP_DATABASEOBJECTENUMCHECKOUT: // enumerate checked out objects
      // param 1 = database name
      // returns, list of objects checked out by this process
      {
         if (!gpMainWindow->m_pDatabase || (plParams->Num() != 1)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
         PCDatabaseCat pCat = gpMainWindow->m_pDatabase->Get (ps->Get());
         ps->Release();

         if (pCat)
            pCat->ObjectEnumCheckOut (pRet);
         else
            pRet->SetBOOL (FALSE);
      }
      return TRUE;

   case SLIMP_DATABASEOBJECTNUM: // return number of checked out
      // param 1 = database name
      // param 2 = if TRUE then return the number of checked out items, FALSE the number total
      // returns, number of checked out
      {
         if (!gpMainWindow->m_pDatabase || (plParams->Num() != 2)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
         PCDatabaseCat pCat = gpMainWindow->m_pDatabase->Get (ps->Get());
         ps->Release();

         if (pCat)
            pRet->SetDouble (pCat->ObjectNum(plParams->Get(1)->GetBOOL(pVM)));
         else
            pRet->SetBOOL (FALSE);
      }
      return TRUE;

   case SLIMP_DATABASEOBJECTGET: // return number of checked out
      // param 1 = database name
      // param 2 = index into # (of checked out)
      // param 3 = if TRUE then only look at checked out, FALSE then all
      // returns, object ID at th index
      {
         if (!gpMainWindow->m_pDatabase || (plParams->Num() != 3)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
         PCDatabaseCat pCat = gpMainWindow->m_pDatabase->Get (ps->Get());
         ps->Release();

         DWORD dwNum = (DWORD) plParams->Get(1)->GetDouble (pVM);

         if (pCat) {
            GUID gID;
            if (pCat->ObjectGet (dwNum, plParams->Get(2)->GetBOOL(pVM), &gID))
               pRet->SetObject (&gID);
            else
               pRet->SetBOOL (NULL);
         }
         else
            pRet->SetBOOL (FALSE);
      }
      return TRUE;

   case SLIMP_DATABASEBACKUP: // saves the database, and backs up to another directory
      // param 1 = directory where to backup to (such as "c:\hello" or "c:\hello\"
      {
         if (!gpMainWindow->m_pDatabase || (plParams->Num() != 1)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
         pRet->SetBOOL (gpMainWindow->m_pDatabase->SaveBackup (ps->Get()));
         ps->Release();
      }
      return TRUE;

   case SLIMP_EMAILSEND: // saves the database, and backs up to another directory
      // param 1 = domain, 2= SMTPserver, 3=emailto, 4=emailfrom, 5=name from, 6=subject, 7=message
      // 8=authuser, 9=authpassword
      {
         if (!gpMainWindow->m_pEmailThread || (plParams->Num() != 9)) {
            pRet->SetBOOL (FALSE);
            return FALSE;
         }

         PCMIFLVarString aps[9];
         DWORD i;
         for (i = 0; i < 9; i++) {
            // can be NULL
            if ((i >= 7) && (i <= 8)) {
               if ((plParams->Get(i)->TypeGet() != MV_STRING) && (plParams->Get(i)->TypeGet() != MV_STRINGTABLE)) {
                  aps[i] = NULL;
                  continue;
               }
            }
            aps[i] = plParams->Get(i)->GetString(pVM);
         }
         pRet->SetBOOL (gpMainWindow->m_pEmailThread->Mail (
            aps[0]->Get(), aps[1]->Get(), aps[2]->Get(), aps[3]->Get(), aps[4]->Get(), aps[5]->Get(), aps[6]->Get(),
            aps[7] ? aps[7]->Get() : NULL, aps[8] ? aps[8]->Get() : NULL ));
         for (i = 0; i < 9; i++)
            if (aps[i])
               aps[i]->Release();
      }
      return TRUE;

   case SLIMP_PERFORMANCEGUI: // GDI resources for the process
      {
         HANDLE hProcess = GetCurrentProcess();
         pRet->SetDouble (GetGuiResources(hProcess, GR_GDIOBJECTS) + GetGuiResources (hProcess, GR_USEROBJECTS));
      }
      return TRUE;

   case SLIMP_PERFORMANCETHREADS: // number of threads (total) in the server
   case SLIMP_PERFORMANCEHANDLES: // number of handles (total) in the server
      {
         PERFORMACE_INFORMATION pi;
         memset (&pi, 0, sizeof(pi));
         pi.cb = sizeof(pi);
         GetPerformanceInfo (&pi, sizeof(pi));

         if (*pdw == SLIMP_PERFORMANCETHREADS)
            pRet->SetDouble (pi.ThreadCount);
         else
            pRet->SetDouble (pi.HandleCount);
      }
      return TRUE;

   case SLIMP_PERFORMANCEMEMORY: // returns amount of memory used, in megabytes
      // param 1 = type of memory looking for... this can be 0 = amount specifically allocated,
      // 1 = amount allocated once memory management and buffering taken into account
      // 2 = working set
      // 3 = total memory used, as stated by OS
      {
         if (plParams->Num() != 1) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         DWORD dwVal = (DWORD) plParams->Get(0)->GetDouble(pVM);
         size_t s;
         switch (dwVal) {
         case 0:  // specifically allocated
         case 1:  // with memory management
            s = EscMemoryAllocated (dwVal == 1);
            pRet->SetDouble ((double)(s/1000) / 1000.0);
            return TRUE;

         case 2:  // working set
         case 3:  // total used
            {
               PROCESS_MEMORY_COUNTERS pmk;
               HANDLE hProcess = GetCurrentProcess();
               memset (&pmk, 0, sizeof(pmk));
               pmk.cb = sizeof(pmk);
               GetProcessMemoryInfo (hProcess, &pmk, sizeof(pmk));
               //SYSTEM_INFO si;
               //memset (&si, 0, sizeof(si));
               //GetSystemInfo (&si);

               if (dwVal == 2)
                  s = pmk.WorkingSetSize;
               else
                  s = pmk.PagefileUsage;

               pRet->SetDouble ((double)(s/1000) / 1000.0);
            }
            return TRUE;

         default:
            pRet->SetBOOL (FALSE);
            return TRUE;
         } // dwVal

         // BUGFIX - this code shouldnt be here
         //PCMIFLVarString aps[7];
         //DWORD i;
         //for (i = 0; i < 7; i++)
         //   aps[i] = plParams->Get(i)->GetString(pVM);
         //pRet->SetBOOL (gpMainWindow->m_pEmailThread->Mail (
         //   aps[0]->Get(), aps[1]->Get(), aps[2]->Get(), aps[3]->Get(), aps[4]->Get(), aps[5]->Get(), aps[6]->Get() ));
         //for (i = 0; i < 7; i++)
         //   aps[i]->Release();
      }
      return TRUE;


   case SLIMP_PERFORMANCECPUOBJECT: // starts/stop timer to keep track of CPU used by specific object
      // param 1 = object. If not object then resets to turn CPU performance off
      // returns Undefined
      {
         if ((plParams->Num() != 1) || !gpMainWindow) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         PCMIFLVar pv = plParams->Get(0);
         if (pv->TypeGet() == MV_OBJECT) {
            GUID g = pv->GetGUID ();
            gpMainWindow->CPUMonitor(&g);
         }
         else
            gpMainWindow->CPUMonitor(NULL);
         pRet->SetUndefined();
         return TRUE;
      }
      return TRUE;


   case SLIMP_PERFORMANCECPUOBJECTQUERY: // return percent of CPU used by the given object
      // param 1 = object.
      // returns percent from 0..1, or FALSE if error
      {
         if ((plParams->Num() != 1) || !gpMainWindow) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         PCMIFLVar pv = plParams->Get(0);
         if (pv->TypeGet() != MV_OBJECT) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         GUID g = pv->GetGUID ();
         double fTime = gpMainWindow->CPUMonitorUsed (&g);
         pRet->SetDouble (fTime);
         return TRUE;
      }
      return TRUE;


   case SLIMP_PERFORMANCECPU: // returns the CPU performance
      // returns list with [
      //    the number of seconds the process has run so far / # processors,
      //    # seconds since creation (without dividing),
      //    number of seconds in main thread (without dividing),
      //    number of seconds since main thread creation (without dividing) ]
      {
         HANDLE hProcess = GetCurrentProcess();
         FILETIME ftCreation, ftExit, ftKernel, ftUser, ftNow;
         GetProcessTimes (hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser);
         GetSystemTimeAsFileTime (&ftNow);

         // how long since created, etc.
         __int64 iRealTime = *((__int64*)&ftNow) - *((__int64*)&ftCreation);
         __int64 iProcessTime = *((__int64*)&ftKernel) + *((__int64*)&ftUser);
         iProcessTime /= HowManyProcessors(FALSE); // divide by number of processors

         // also, thread time
         GetThreadTimes (GetCurrentThread(), &ftCreation, &ftExit, &ftKernel, &ftUser);
         __int64 iThreadRealTime = *((__int64*)&ftNow) - *((__int64*)&ftCreation);
         __int64 iThreadProcessTime = *((__int64*)&ftKernel) + *((__int64*)&ftUser);

         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pRet->SetUndefined();
            return TRUE;
         }

         CMIFLVar var;
         // NOTE: filetime = 100 ns = 0.1 microsec
         var.SetDouble ((double)(iRealTime / 10000) / 1000.0);
         pl->Add (&var, TRUE);
         var.SetDouble ((double)(iProcessTime / 10000) / 1000.0);
         pl->Add (&var, TRUE);
         var.SetDouble ((double)(iThreadRealTime / 10000) / 1000.0);
         pl->Add (&var, TRUE);
         var.SetDouble ((double)(iThreadProcessTime / 10000) / 1000.0);
         pl->Add (&var, TRUE);

         // finally, set the list
         pRet->SetList (pl);
         pl->Release();
      }
      return TRUE;



   case SLIMP_PERFORMANCENETWORK: // amound of data sent/received
      // returns list with [megabytes of data send, megabytes of data received]
      {
         __int64 iSent = 0, iReceived = 0;
         if (gpMainWindow && gpMainWindow->m_pIT)
            gpMainWindow->m_pIT->PerformanceNetwork (&iSent, &iReceived);


         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pRet->SetUndefined();
            return TRUE;
         }

         CMIFLVar var;
         var.SetDouble ((double)(iSent / 1000) / 1000.0);
         pl->Add (&var, TRUE);
         var.SetDouble ((double)(iReceived / 1000) / 1000.0);
         pl->Add (&var, TRUE);

         // finally, set the list
         pRet->SetList (pl);
         pl->Release();
      }
      return TRUE;

   case SLIMP_PERFORMANCEDISK: // returns the amount of free disk space, in gigabytes
      {
         WCHAR szDir[256];
         szDir[0] = 0;
         if (gpMainWindow)
            wcscpy (szDir, gpMainWindow->m_szDatabaseDir);
         wcscat (szDir, L"\\");  // just in case

         // if starts with double backslash then UNC
         PWSTR pszCur;
         if ((szDir[0] == L'\\') && (szDir[1] == L'\\')) {
            pszCur = wcschr (szDir+2, L'\\');
            if (pszCur)
               pszCur++;   // so just beyond that
         }
         else
            pszCur = &szDir[0];

         // find to slash
         pszCur = wcschr (pszCur, L'\\');
         if (pszCur)
            pszCur[1] = 0; // NULL terminte

         ULARGE_INTEGER iFree, iTotal, iTotal2;
         memset (&iFree, 0, sizeof(iFree));
         GetDiskFreeSpaceExW (szDir, &iFree, &iTotal, &iTotal2);

         __int64 i = *((__int64*)&iFree);
         i /= 1000000;  // to megabytes

         pRet->SetDouble ((double)i / 1000.0);
      }
      return TRUE;

   case SLIMP_TEXTLOG:  // log text using auto
      // string to log, followed by optional object
      {
         if ((plParams->Num() < 1) || (plParams->Num() > 2)) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         if (!gpMainWindow || !gpMainWindow->m_pTextLog) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         // get the object
         GUID gObject = GUID_NULL;
         PCMIFLVar pVar;
         if ((pVar = plParams->Get(1)) && (pVar->TypeGet() == MV_OBJECT))
            gObject = pVar->GetGUID();

         // get the string
         pVar = plParams->Get(0);
         PCMIFLVarString ps = pVar->GetString (pVM);
         pRet->SetBOOL (gpMainWindow->m_pTextLog->LogAuto (ps->Get(), &gObject));
         ps->Release();
      }
      return TRUE;

   case SLIMP_TEXTLOGNOAUTO:  // log text with parameters
      // string, actor, object, room, user
      {
         if (!gpMainWindow || !gpMainWindow->m_pTextLog) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         // get the object
         GUID gActor = GUID_NULL;
         GUID gObject = GUID_NULL;
         GUID gRoom = GUID_NULL;
         GUID gUser = GUID_NULL;
         PCMIFLVar pVar;
         if ((pVar = plParams->Get(1)) && (pVar->TypeGet() == MV_OBJECT))
            gActor = pVar->GetGUID();
         if ((pVar = plParams->Get(2)) && (pVar->TypeGet() == MV_OBJECT))
            gObject = pVar->GetGUID();
         if ((pVar = plParams->Get(3)) && (pVar->TypeGet() == MV_OBJECT))
            gRoom = pVar->GetGUID();
         if ((pVar = plParams->Get(4)) && (pVar->TypeGet() == MV_OBJECT))
            gUser = pVar->GetGUID();

         // get the string
         pVar = plParams->Get(0);
         PCMIFLVarString ps = pVar->GetString (pVM);
         pRet->SetBOOL (gpMainWindow->m_pTextLog->Log (ps->Get(), &gActor, &gObject, &gRoom, &gUser));
         ps->Release();
      }
      return TRUE;

   case SLIMP_TEXTLOGNUMLINES:  // return number of lines
      // param is file string
      {
         if (!gpMainWindow || !gpMainWindow->m_pTextLog) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         PCMIFLVar pVar;
         pVar = plParams->Get(0);
         PCMIFLVarString ps = pVar->GetString (pVM);
         DWORD dwDate, dwTime;
         if (!gpMainWindow->m_pTextLog->IDStringToDateTime (ps->Get(), 0, &dwDate, &dwTime)) {
            ps->Release();
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         ps->Release();

         DWORD dwRet = gpMainWindow->m_pTextLog->NumLines (dwDate, dwTime);
         if (dwRet == (DWORD)-1)
            pRet->SetBOOL (FALSE);
         else
            pRet->SetDouble (dwRet);
      }
      return TRUE;


   case SLIMP_TEXTLOGDELETE:  // delete file
      // param is file string
      {
         if (!gpMainWindow || !gpMainWindow->m_pTextLog) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         PCMIFLVar pVar;
         pVar = plParams->Get(0);
         PCMIFLVarString ps = pVar->GetString (pVM);
         DWORD dwDate, dwTime;
         if (!gpMainWindow->m_pTextLog->IDStringToDateTime (ps->Get(), 0, &dwDate, &dwTime)) {
            ps->Release();
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         ps->Release();

         pRet->SetBOOL (gpMainWindow->m_pTextLog->Delete (dwDate, dwTime));
      }
      return TRUE;

   case SLIMP_TEXTLOGAUTOGET:  // get an auto value
      // param1 value name, "actor", "room", or "user"
      {
         if (!gpMainWindow || !gpMainWindow->m_pTextLog) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         PCMIFLVar pVar;
         pVar = plParams->Get(0);
         PCMIFLVarString ps = pVar->GetString (pVM);
         PWSTR psz = ps->Get();
         DWORD dwType;
         if (!_wcsicmp(psz, L"actor"))
            dwType = 0;
         else if (!_wcsicmp(psz, L"room"))
            dwType = 1;
         else if (!_wcsicmp(psz, L"user"))
            dwType = 2;
         else {
            ps->Release();
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         ps->Release();

         GUID g;
         gpMainWindow->m_pTextLog->AutoGet (dwType, &g);
         if (IsEqualGUID (g, GUID_NULL))
            pRet->SetNULL();
         else
            pRet->SetObject (&g);
      }
      return TRUE;

   case SLIMP_TEXTLOGENABLESET:   // set enable
      // param1 is TRUE or FALSE
      {
         if (!gpMainWindow || !gpMainWindow->m_pTextLog) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         PCMIFLVar pVar;
         pVar = plParams->Get(0);
         gpMainWindow->m_pTextLog->EnableSet (pVar->GetBOOL(pVM));
         pRet->SetUndefined ();
      }
      return TRUE;

   case SLIMP_TEXTLOGENABLEGET:   // get enable
      {
         if (!gpMainWindow || !gpMainWindow->m_pTextLog) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         pRet->SetBOOL (gpMainWindow->m_pTextLog->EnableGet());
      }
      return TRUE;

   case SLIMP_TEXTLOGENUM:   // enumerates logs
      {
         if (!gpMainWindow || !gpMainWindow->m_pTextLog) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         PCMIFLVarList pl = new CMIFLVarList;

         // enumerate
         CListFixed list;
         gpMainWindow->m_pTextLog->Enum (&list);

         // add
         DWORD i;
         DWORD *pdw = (DWORD*)list.Get(0);
         PCMIFLVarList pl2;
         CMIFLVar var;
         WCHAR szTemp[32];
         FILETIME ft;
         for (i = 0; i < list.Num(); i++, pdw += 2) {
            // sublist
            pl2 = new CMIFLVarList;
            if (!pl2)
               continue;

            // string
            gpMainWindow->m_pTextLog->DateTimeToIDString (pdw[0], pdw[1], szTemp);
            var.SetString (szTemp);
            pl2->Add (&var, TRUE);

            // time
            gpMainWindow->m_pTextLog->DateTimeToFILETIME (pdw[0], pdw[1], &ft);
            var.SetDouble (MIFLFileTimeToDouble (&ft));
            pl2->Add (&var, TRUE);

            // add it
            var.SetList (pl2);
            pl->Add (&var, TRUE);
            pl2->Release();
         } // i

         // done
         pRet->SetList (pl);
         pl->Release();
      }
      return TRUE;

   case SLIMP_TEXTLOGREAD:  // reads a line
      // param1 is file string
      // param2 is line number
      // returns list with [time stamp, string, actor, object, room, user]
      {
         if (!gpMainWindow || !gpMainWindow->m_pTextLog) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         PCMIFLVar pVar;
         pVar = plParams->Get(0);
         PCMIFLVarString ps = pVar->GetString (pVM);
         DWORD dwDate, dwTime;
         if (!gpMainWindow->m_pTextLog->IDStringToDateTime (ps->Get(), 0, &dwDate, &dwTime)) {
            ps->Release();
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         ps->Release();

         // line number
         DWORD dwLine = (DWORD) (plParams->Get(1))->GetDouble(pVM);

         CMem mem;
         GUID gActor, gObject, gRoom, gUser;
         if (!gpMainWindow->m_pTextLog->Read (dwDate, dwTime, dwLine,
            &dwDate, &dwTime, &mem, &gActor, &gObject, &gRoom, &gUser)) {

            pRet->SetBOOL (FALSE);
            return TRUE;
            }


         // create list
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl) {
            pRet->SetBOOL (FALSE);
            return TRUE;
            }

         // time
         FILETIME ft;
         CMIFLVar var;
         gpMainWindow->m_pTextLog->DateTimeToFILETIME (dwDate, dwTime, &ft);
         var.SetDouble (MIFLFileTimeToDouble (&ft));
         pl->Add (&var, TRUE);

         // string
         var.SetString ((PWSTR)mem.p);
         pl->Add (&var, TRUE);

         // actor
         if (!IsEqualGUID(GUID_NULL, gActor))
            var.SetObject (&gActor);
         else
            var.SetNULL();
         pl->Add (&var, TRUE);

         // Object
         if (!IsEqualGUID(GUID_NULL, gObject))
            var.SetObject (&gObject);
         else
            var.SetNULL();
         pl->Add (&var, TRUE);

         // Room
         if (!IsEqualGUID(GUID_NULL, gRoom))
            var.SetObject (&gRoom);
         else
            var.SetNULL();
         pl->Add (&var, TRUE);

         // User
         if (!IsEqualGUID(GUID_NULL, gUser))
            var.SetObject (&gUser);
         else
            var.SetNULL();
         pl->Add (&var, TRUE);

         // done
         pRet->SetList (pl);
         pl->Release();
      }
      return TRUE;


   case SLIMP_TEXTLOGAUTOSET:  // set auto value
      // param1 value name, "actor", "room", or "user"
      // param2 is an object, or NULL
      {
         if (!gpMainWindow || !gpMainWindow->m_pTextLog) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         PCMIFLVar pVar;
         pVar = plParams->Get(0);
         PCMIFLVarString ps = pVar->GetString (pVM);
         PWSTR psz = ps->Get();
         DWORD dwType;
         if (!_wcsicmp(psz, L"actor"))
            dwType = 0;
         else if (!_wcsicmp(psz, L"room"))
            dwType = 1;
         else if (!_wcsicmp(psz, L"user"))
            dwType = 2;
         else {
            ps->Release();
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         ps->Release();

         GUID g = GUID_NULL;
         pVar = plParams->Get(1);
         if (pVar->TypeGet() == MV_OBJECT)
            g = pVar->GetGUID();

         pRet->SetBOOL (gpMainWindow->m_pTextLog->AutoSet (dwType, &g));
      }
      return TRUE;


   case SLIMP_TEXTLOGSEARCH:   // search through text log
      // param1 = start time
      // param2 = end time
      // param3 = string (or NULL)
      // param4 = actor (or NULL)
      // param5 = object (or NULL)
      // param6 = room (or NULL)
      // param7 = user (or NULL)
      // param8 = callback
      // param9 = param into callback
      {
         if (!gpMainWindow || !gpMainWindow->m_pTextLog) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         // get start and end as date
         FILETIME ft;
         DWORD dwStartDate, dwStartTime, dwEndDate, dwEndTime;
         MIFLDoubleToFileTime (plParams->Get(0)->GetDouble(pVM), &ft);
         gpMainWindow->m_pTextLog->FILETIMEToDateTime (&ft, &dwStartDate, &dwStartTime);
         MIFLDoubleToFileTime (plParams->Get(1)->GetDouble(pVM), &ft);
         gpMainWindow->m_pTextLog->FILETIMEToDateTime (&ft, &dwEndDate, &dwEndTime);

         // guids
         GUID gActor = GUID_NULL, gObject = GUID_NULL, gRoom = GUID_NULL, gUser = GUID_NULL;
         PCMIFLVar pVar;
         pVar = plParams->Get(3);
         if (pVar->TypeGet() == MV_OBJECT)
            gActor = pVar->GetGUID();
         pVar = plParams->Get(4);
         if (pVar->TypeGet() == MV_OBJECT)
            gObject = pVar->GetGUID();
         pVar = plParams->Get(5);
         if (pVar->TypeGet() == MV_OBJECT)
            gRoom = pVar->GetGUID();
         pVar = plParams->Get(6);
         if (pVar->TypeGet() == MV_OBJECT)
            gUser = pVar->GetGUID();

         // make sure callback is really a callback
         pVar = plParams->Get(7);
         switch (pVar->TypeGet()) {
         case MV_FUNC:
         case MV_OBJECTMETH:
            break;   // ok
         default:
            pRet->SetBOOL (FALSE);
            return TRUE;
         } // switch

         // get the string
         PCMIFLVarString ps = NULL;
         pVar = plParams->Get(2);
         switch (pVar->TypeGet()) {
         case MV_STRING:
         case MV_STRINGTABLE:
            ps = pVar->GetString(pVM);
            break;
         } // switch

         // call
         pRet->SetBOOL (gpMainWindow->m_pTextLog->Search (dwStartDate, dwStartTime, dwEndDate, dwEndTime,
            &gActor, &gObject, &gRoom, &gUser, ps ? ps->Get() : NULL,
            gpMainWindow->m_hWnd, WM_SEARCHCALLBACK,
            pVM, plParams->Get(7), plParams->Get(8)));


         // release
         if (ps)
            ps->Release();
      }
      return TRUE;



   case SLIMP_CONNECTIONBAN: // bans an IP address (first param), by amount (second param)
      {
         // need two parameters
         PCMIFLVar pVar =plParams->Get(1);
         if (!pVar) {
            pRet->SetUndefined();
            return TRUE;
         }
         fp fAmount = pVar->GetDouble (pVM);

         pVar = plParams->Get(2);
         if (!pVar) {
            pRet->SetUndefined();
            return TRUE;
         }
         fp fDays = pVar->GetDouble (pVM);

         pVar = plParams->Get(0);
         PCMIFLVarString ps = pVar->GetString (pVM);

         pVar->SetBOOL ((gpMainWindow && gpMainWindow->m_pIT) ?
            gpMainWindow->m_pIT->IPBlacklist (ps->Get(), fAmount, fDays) : FALSE);

         ps->Release();
      }
      return TRUE;

   
   case SLIMP_CONNECTIONDISCONNECT: // function that will disconnect and delete connection
      {
         // need one parameter
         PCMIFLVar pVar =plParams->Get(0);
         if (!pVar) {
            pRet->SetUndefined();
            return TRUE;
         }
         DWORD dwConnect = (DWORD) pVar->GetDouble(pVM);

         pVar->SetBOOL ((gpMainWindow && gpMainWindow->m_pIT) ?
            gpMainWindow->m_pIT->ConnectionRemove (dwConnect) : FALSE);
      }
      return TRUE;

   case SLIMP_CONNECTIONSEND: // function to send a message out to a given connection
      {
         // get the connection
         PCMIFLVar pVar =plParams->Get(0);
         if (!pVar) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         DWORD dwConnect = (DWORD) pVar->GetDouble(pVM);
         PCCircumrealityPacket pp = (gpMainWindow && gpMainWindow->m_pIT) ?
            gpMainWindow->m_pIT->ConnectionGet (dwConnect) : NULL;

         // 2nd parameter is true to queue up message, false for instant
         pVar =plParams->Get(1);
         BOOL fQueue = pVar ? pVar->GetBOOL(pVM) : TRUE;

         // 3rd parameter is string to send
         pVar =plParams->Get(2);
         if (!pVar || !pp) {
            if (pp)
               gpMainWindow->m_pIT->ConnectionGetRelease();

            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         // parse it
         // BUGFIX - Just send the string across directly.. don't go to and from MML
         // This allows several nodes to be in the same string, making ultimate
         // transmission faster (hopefully)
         // PCMMLNode2 pNode;
         PCMIFLVarString ps = pVar->GetString(pVM);
         PWSTR psz = ps->Get();
         //pNode = MIFParseMML (ps->Get());
         //if (!pNode) {
         //   pRet->SetBOOL (FALSE);
         //   ps->Release();
         //   return TRUE;
         //}

// #ifdef _DEBUG
//          OutputDebugStringW (L"\r\n");
//          OutputDebugStringW (psz);
//          OutputDebugStringW (L"\r\n");
// #endif

         pRet->SetBOOL(
            pp->PacketSend (fQueue ? CIRCUMREALITYPACKET_MMLQUEUE : CIRCUMREALITYPACKET_MMLIMMEDIATE,
               psz, ((DWORD)wcslen(psz)+1)*sizeof(WCHAR)
//             pNode
            ));
         gpMainWindow->m_pIT->ConnectionGetRelease(); // free up so can delete
         ps->Release();
//         delete pNode;
      }
      return TRUE;

   case SLIMP_RENDERCACHELIMITS: // limit size of render cache
      {
         // get the number, params 0,1,2 are in megabytes
         double afValues[5];
         PCMIFLVar pVar;
         DWORD i;
         for (i = 0; i < sizeof(afValues)/sizeof(afValues[0]); i++) {
            pVar = plParams->Get(i);
            if (!pVar) {
               pRet->SetBOOL (FALSE);
               return TRUE;
            }
            afValues[i] = pVar->GetDouble (pVM);
         }

         gpMainWindow->m_pIDT->LimitsSet (
            (__int64)afValues[0] * 1000000,
            (__int64)afValues[1] * 1000000,
            (__int64)afValues[2] * 1000000,
            (DWORD)afValues[(sizeof(PVOID) > sizeof(DWORD)) ? 4 : 3]);

         pRet->SetBOOL (TRUE);
         return TRUE;
      }
      return TRUE;


   case SLIMP_RENDERCACHESEND: // send a cached render/TTS to the player's client
      {
         // two parameters:
         //    connection ID
         //    string of MML
         // returns 0+ (quality) that sent, or -1 if didn't end
         PCMIFLVar pVar = plParams->Get(0);
         if (!pVar) {
            pRet->SetDouble (-1);
            return TRUE;
         }
         DWORD dwConnectionID = (DWORD) pVar->GetDouble(pVM);

         pVar = plParams->Get(1);
         if (!pVar) {
            pRet->SetDouble (-1);
            return TRUE;
         }
         PCMIFLVarString ps = pVar->GetString (pVM);
         if (!ps) {
            pRet->SetDouble (-1);
            return TRUE;
         }

         // BUGFIX - Work around MML having optional quotes
         CMem mem;
         PCMMLNode2 pNode = CircumrealityParseMML (ps->Get());
         ps->Release();
         if (!pNode) {
            pRet->SetDouble (-1);
            return TRUE;
         }
         if (!MMLToMem (pNode, &mem)) {
            delete pNode;
            pRet->SetDouble (-1);
            return TRUE;
         }
         mem.CharCat (0);  // just to null terminate
         PWSTR pszString = (PWSTR)mem.p;
         delete pNode;

         int iRet = gpMainWindow->m_pIDT->EntryExists (FALSE, pszString, 0, IMAGEDATABASE_MAXQUALITIES, IMAGEDATABASETYPE_WAVE);

         if (iRet >= 0)
            gpMainWindow->m_pIDT->EntryAdd (FALSE, 1, pszString, 0, IMAGEDATABASE_MAXQUALITIES,
               IMAGEDATABASETYPE_WAVE, dwConnectionID, NULL, 0);

         // finally
         pRet->SetDouble (iRet);
         return TRUE;
      }
      return TRUE;

   case SLIMP_CONNECTIONSLIMIT: // limit the number of connectinons
      {
         // get the number
         PCMIFLVar pVar =plParams->Get(0);
         if (!pVar) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         DWORD dwValue = (DWORD)pVar->GetDouble (pVM);
         if (!dwValue) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         pRet->SetBOOL (gpMainWindow->m_pIT->ConnectionsLimit (dwValue));
         return TRUE;
      }
      return TRUE;

   case SLIMP_VOICECHATALLOWWAVES:  // allow waves
      {
         // get the connection
         PCMIFLVar pVar =plParams->Get(0);
         PCMIFLVarString ps = pVar->GetString (pVM);
         PCMMLNode2 pNode = CircumrealityParseMML (ps->Get());
         ps->Release();
         if (!pNode) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
   
         // extract the files
         CResVoiceChatInfo vci;
         PWSTR psz;
         vci.MMLFrom (pNode);
         delete pNode;

         // add the files to a list of acceptable files
         DWORD i,j;
         PWAVEBASECHOICE pwc = (PWAVEBASECHOICE) vci.m_lWAVEBASECHOICE.Get(0);
         for (i = 0; i < vci.m_lWAVEBASECHOICE.Num(); i++, pwc++) {
            for (j = 0; j < gpMainWindow->m_lWaveBaseValid.Num(); j++) {
               psz = (PWSTR)gpMainWindow->m_lWaveBaseValid.Get(j);
               if (!_wcsicmp(psz, pwc->szFile))
                  break;
            }
            if (j < gpMainWindow->m_lWaveBaseValid.Num())
               continue;   // alreayd exists

            // else, add as a valid file
            gpMainWindow->m_lWaveBaseValid.Add (pwc->szFile, (wcslen(pwc->szFile)+1)*sizeof(WCHAR));
         } // i

         pRet->SetBOOL (TRUE);
         return TRUE;
      }
      return TRUE;

   case SLIMP_CONNECTIONSENDVOICECHAT: // to send voice chat
      {
         // get the connection
         PCMIFLVar pVar =plParams->Get(0);
         if (!pVar) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         DWORD dwConnect = (DWORD) pVar->GetDouble(pVM);
         PCCircumrealityPacket pp = (gpMainWindow && gpMainWindow->m_pIT) ?
            gpMainWindow->m_pIT->ConnectionGet (dwConnect) : NULL;
         if (!pp) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         // 3rd parameter is binary, so get that
         CMem memBinary;
         pVar = plParams->Get(2);
         PCMIFLVarString ps = pVar->GetString (pVM);
         if (!ps) {
            gpMainWindow->m_pIT->ConnectionGetRelease ();
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         BOOL fRet = StringPlusOneToBinary (ps->Get(), &memBinary);
         ps->Release();
         if (!fRet) {
            gpMainWindow->m_pIT->ConnectionGetRelease ();
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         // potentially apply effect... like randomize or whisper
         pVar = plParams->Get(3);   // garble
         if (pVar->GetBOOL(pVM))
            VoiceChatRandomize ((PBYTE)memBinary.p, (DWORD)memBinary.m_dwCurPosn);
         pVar = plParams->Get(4);   // effect
         if (pVar->GetDouble (pVM) == 1.0)
            VoiceChatWhisperify ((PBYTE)memBinary.p, (DWORD)memBinary.m_dwCurPosn);

         // find the string for the MML
         pVar = plParams->Get(1);
         ps = pVar->GetString (pVM);
         if (!ps) {
            gpMainWindow->m_pIT->ConnectionGetRelease ();
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         PWSTR psz = ps->Get();
         DWORD dwSize = ((DWORD)wcslen(psz)+1)*sizeof(WCHAR);
         if (!memBinary.Required (memBinary.m_dwCurPosn + dwSize)) {
            gpMainWindow->m_pIT->ConnectionGetRelease ();
            ps->Release();
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         memmove ((PBYTE)memBinary.p + dwSize, memBinary.p, memBinary.m_dwCurPosn);
         memcpy (memBinary.p, psz, dwSize);
         memBinary.m_dwCurPosn += dwSize;
         ps->Release();
         if (!fRet) {
            gpMainWindow->m_pIT->ConnectionGetRelease ();
            pRet->SetBOOL (FALSE);
            return TRUE;
         }


         pRet->SetBOOL(
            pp->PacketSend (CIRCUMREALITYPACKET_VOICECHAT, memBinary.p, (DWORD)memBinary.m_dwCurPosn, 0, 0)
            );

         gpMainWindow->m_pIT->ConnectionGetRelease(); // free up so can delete
//         delete pNode;
      }
      return TRUE;

   case SLIMP_CONNECTIONENUM: // enumerates all the connections, as numbers or objects
      {
         PCMIFLVarList pl = new CMIFLVarList;
         if (!pl || !gpMainWindow || !gpMainWindow->m_pIT) {
            pRet->SetUndefined();
            return TRUE;
         }

         // enumerate the connections as numbers
         CListFixed l;
         gpMainWindow->m_pIT->ConnectionEnum (&l);

         // add
         DWORD *padw = (DWORD*)l.Get(0);
         DWORD i;
         CMIFLVar var;
         for (i = 0; i < l.Num(); i++,padw++) {
            var.SetDouble (padw[0]);
            pl->Add (&var, TRUE);
         }

         // finally, set the list
         pRet->SetList (pl);
         pl->Release();
      }
      return TRUE;

   case SLIMP_CONNECTIONINFOGET: // gets piece of information about the connection
      {
         // get the connection
         PCMIFLVar pVar = plParams->Get(0);
         DWORD dwConnect = pVar ? (DWORD) pVar->GetDouble(pVM) : 0;
         PCCircumrealityPacket pp = (gpMainWindow && gpMainWindow->m_pIT) ?
            gpMainWindow->m_pIT->ConnectionGet (dwConnect) : NULL;
         if (!pp) {
            pRet->SetUndefined ();
            return TRUE;
         }

         // get info
         CIRCUMREALITYPACKETINFO info;
         pp->InfoGet (&info);
         gpMainWindow->m_pIT->ConnectionGetRelease(); // so can delete connections

         // parse parameter
         pVar = plParams->Get(1);
         if (!pVar) {
            pRet->SetUndefined ();
            return TRUE;
         }
         PCMIFLVarString ps = pVar->GetString(pVM);
         DWORD *pdw = (DWORD*) m_hImport.Find (ps->Get());
         ps->Release();
         if (!pdw) {
            pRet->SetUndefined ();
            return TRUE;
         }
         
         // do code
         LARGE_INTEGER     liPerCountFreq, liCount;
         __int64  iTime;
         switch (pdw[0]) {
         case SLIMP_INFOIP: // IP address
            pRet->SetString (info.szIP);
            break;
         case SLIMP_INFOSTATUS: // status string
            pRet->SetString (info.szStatus);
            break;
         case SLIMP_INFOUSER: // user string
            pRet->SetString (info.szUser);
            break;
         case SLIMP_INFOCHARACTER: // character string
            pRet->SetString (info.szCharacter);
            break;
         case SLIMP_INFOUNIQUEID: // uniqueID
            pRet->SetString (info.szUniqueID);
            break;
         case SLIMP_INFOOBJECT: // object
            if (IsEqualGUID(info.gObject, GUID_NULL))
               pRet->SetNULL();
            else
               pRet->SetObject (&info.gObject);
            break;
         case SLIMP_INFOSENDBYTES: // number bytes sent
            pRet->SetDouble (info.iSendBytes);
            break;
         case SLIMP_INFOSENDBYTESCOMP: // number bytes sent, compressed
            pRet->SetDouble (info.iSendBytesComp);
            break;
         case SLIMP_INFOSENDBYTESEXPECT: // number bytes sent expected
            pRet->SetDouble (info.iSendBytesExpect);
            break;
         case SLIMP_INFORECEIVEBYTES: // number bytes receivec
            pRet->SetDouble (info.iReceiveBytes);
            break;
         case SLIMP_INFORECEIVEBYTESCOMP: // number bytes received, compressed
            pRet->SetDouble (info.iReceiveBytesComp);
            break;
         case SLIMP_INFORECEIVEBYTESEXPECT: // number bytes received expected
            pRet->SetDouble (info.iReceiveBytesExpect);
            break;
         case SLIMP_INFOCONNECTTIME: // time that connected, number of seconds before present
            QueryPerformanceFrequency (&liPerCountFreq);
            QueryPerformanceCounter (&liCount);
            iTime = *((__int64*)&liCount) / ( *((__int64*)&liPerCountFreq) / 1000);
            pRet->SetDouble ((double)(iTime - info.iConnectTime)/1000.0);
            break;
         case SLIMP_INFOSENDLAST: // last time that sent, number of seconds before present
            QueryPerformanceFrequency (&liPerCountFreq);
            QueryPerformanceCounter (&liCount);
            iTime = *((__int64*)&liCount) / ( *((__int64*)&liPerCountFreq) / 1000);
            pRet->SetDouble ((double)(iTime - info.iSendLast)/1000.0);
            break;
         case SLIMP_INFORECEIVELAST: // last time that received, number of seconds before present
            QueryPerformanceFrequency (&liPerCountFreq);
            QueryPerformanceCounter (&liCount);
            iTime = *((__int64*)&liCount) / ( *((__int64*)&liPerCountFreq) / 1000);
            pRet->SetDouble ((double)(iTime - info.iReceiveLast)/1000.0);
            break;
         case SLIMP_INFOBYTESPERSEC: // average bytes per second received
            pRet->SetDouble (info.dwBytesPerSec);
            break;
         default:
            pRet->SetUndefined ();
            break;
         }
      }
      return TRUE;

   case SLIMP_CONNECTIONQUEUEDTOSEND:  // returns the number of bytes queued to send
      // Param0 = connection number
      {
         // get the connection
         PCMIFLVar pVar = plParams->Get(0);
         DWORD dwConnect = pVar ? (DWORD) pVar->GetDouble(pVM) : 0;
         PCCircumrealityPacket pp = (gpMainWindow && gpMainWindow->m_pIT) ?
            gpMainWindow->m_pIT->ConnectionGet (dwConnect) : NULL;
         if (!pp) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         // get info
         __int64 iSent = pp->QueuedToSend ((DWORD)-1);

         gpMainWindow->m_pIT->ConnectionGetRelease(); // so can delete connections

         pRet->SetDouble ((double)iSent);
      }
      return TRUE;


   case SLIMP_CONNECTIONINFOSET: // sets piece of information about the connection (such as user name)
      {
         // get the connection
         PCMIFLVar pVar = plParams->Get(0);
         DWORD dwConnect = pVar ? (DWORD) pVar->GetDouble(pVM) : 0;
         PCCircumrealityPacket pp = (gpMainWindow && gpMainWindow->m_pIT) ?
            gpMainWindow->m_pIT->ConnectionGet (dwConnect) : NULL;
         if (!pp) {
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         // get info
         CIRCUMREALITYPACKETINFO info;
         pp->InfoGet (&info);

         // parse parameter
         pVar = plParams->Get(1);
         if (!pVar) {
            gpMainWindow->m_pIT->ConnectionGetRelease(); // so can delete connections
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         PCMIFLVarString ps = pVar->GetString(pVM);
         DWORD *pdw = (DWORD*) m_hImport.Find (ps->Get());
         ps->Release();
         if (!pdw) {
            gpMainWindow->m_pIT->ConnectionGetRelease(); // so can delete connections
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
         
         // set to?
         pVar = plParams->Get(2);
         if (!pVar) {
            gpMainWindow->m_pIT->ConnectionGetRelease(); // so can delete connections
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         // do code
         switch (pdw[0]) {
         case SLIMP_INFOIP: // IP address
         case SLIMP_INFOSTATUS: // status string
         case SLIMP_INFOUSER: // user string
         case SLIMP_INFOCHARACTER: // character string
         case SLIMP_INFOUNIQUEID: // unique ID
            {
               ps = pVar->GetString(pVM);
               PWSTR psz = ps->Get();
               PWSTR pszTo;
               DWORD dwLen = (DWORD)wcslen(psz);
               DWORD dwMax = sizeof(info.szIP)/sizeof(WCHAR) - 1; 
               WCHAR cTemp;
               if (dwLen >= dwMax) {
                  // string too long, so concatenate
                  cTemp = psz[dwMax];
                  psz[dwMax] = 0;
               }

               switch (pdw[0]) {
               case SLIMP_INFOIP: // IP address
                  pszTo = info.szIP;
                  break;
               case SLIMP_INFOSTATUS: // status string
                  pszTo = info.szStatus;
                  break;
               case SLIMP_INFOUSER: // user stringx
                  pszTo = info.szUser;
                  break;
               case SLIMP_INFOUNIQUEID: // user string
                  pszTo = info.szUniqueID;
                  break;
               case SLIMP_INFOCHARACTER: // character string
               default:
                  pszTo = info.szCharacter;
                  break;
               }
               wcscpy (pszTo, psz);
               if (dwLen >= dwMax)
                  psz[dwMax] = cTemp;
               ps->Release();

               // set this
               pp->InfoSet (&info);
               gpMainWindow->m_pIT->ConnectionGetRelease(); // so can delete connections
               pRet->SetBOOL (TRUE);
            }
            break;
         case SLIMP_INFOOBJECT: // object
            if (pVar->TypeGet() == MV_OBJECT)
               info.gObject = pVar->GetGUID();
            else
               info.gObject = GUID_NULL;
            pp->InfoSet (&info);
            gpMainWindow->m_pIT->ConnectionGetRelease(); // BUGFIX - Added since missing release
            pRet->SetBOOL (TRUE);
            break;
         default:
            gpMainWindow->m_pIT->ConnectionGetRelease(); // BUGFIX - Added since missing release
            pRet->SetBOOL (FALSE);
            break;
         } // switch pdw[0]
      }
      return TRUE;

   case SLIMP_SHARDPARAM:  // parameter for specific shard
      {
         if (gpMainWindow)
            pRet->SetString ((PWSTR) gpMainWindow->m_memShardParam.p);
         else
            pRet->SetUndefined();
      }
      return TRUE;

   case SLIMP_SHUTDOWNIMMEDIATELY:  // causes immediate shutdown
      {
         PCMIFLVar pVar = plParams->Get(0);
         if (gpMainWindow) {
            gpMainWindow->m_fShutDownImmediately = TRUE;
            pRet->SetBOOL (TRUE);

            gfRestart = (pVar && pVar->GetBOOL(pVM));
         }
         else
            pRet->SetBOOL (FALSE);
      }
      return TRUE;

   } // switch

   return FALSE;
} // FuncImport

BOOL CMIFLSocketServer::TestQuery (void)
{
   return TRUE;
}

/************************************************************************************
CMIFLSocketServer::InternalTestVMNew - This is an internal test VMNew. It stores
away the hack information.
*/
void CMIFLSocketServer::InternalTestVMNew (PCMIFLVM pVM, BOOL fHack)
{
   m_fJustCalledHackITVMNew = fHack;

   // create a new main window
   if (!gpMainWindow) {
      WCHAR szCircumreality[256], szCRK[256];
      ProjectNameToCircumreality (gpMIFLProj->m_szFile, TRUE, szCircumreality);
      ProjectNameToCircumreality (gpMIFLProj->m_szFile, FALSE, szCRK);

      if (!MegaFileGenerateIfNecessary (szCircumreality, szCRK, gpMIFLProj, NULL))
         return;  // error

      gpMainWindow = new CMainWindow;
      if (gpMainWindow) {
         if (!gpMainWindow->Init (szCircumreality)) {
            delete gpMainWindow;
            gpMainWindow = NULL;
         }
      }
   }
   if (gpMainWindow) {
      // BUGFIX - Used to set postquitmessage to false here, but changed this to
      // the defaults, and changed the run-once main-window code to set
      // the flag to true
      //gpMainWindow->m_fPostQuitMessage = FALSE; // so dont shut down
      gpMainWindow->VMSet (pVM);
   }
}

void CMIFLSocketServer::TestVMNew (PCMIFLVM pVM)
{
   if (gpMainWindow && (gpMainWindow->VMGet() == pVM) && m_fJustCalledHackITVMNew) {
      m_fJustCalledHackITVMNew = FALSE;
      return;
   }

   InternalTestVMNew (pVM, FALSE);
}

void CMIFLSocketServer::TestVMPageEnter (PCMIFLVM pVM)
{
   OutputDebugString ("TestVMPageEnter\r\n");
}

void CMIFLSocketServer::TestVMPageLeave (PCMIFLVM pVM)
{
   OutputDebugString ("TestVMPageLeave\r\n");
}

void CMIFLSocketServer::TestVMDelete (PCMIFLVM pVM)
{
   // delete the window if no more test
   if (gpMainWindow && (gpMainWindow->VMGet() == pVM)) {
      delete gpMainWindow;
      gpMainWindow = NULL;
   }
}

void CMIFLSocketServer::VMObjectDelete (PCMIFLVM pVM, GUID *pgID)
{
   // if an object is deleted during the course of normal running it will
   // also be removed from the database (only if it's checked out. Those
   // objects checked in will not be affected)

   if (gpMainWindow && gpMainWindow->m_pDatabase)
      gpMainWindow->m_pDatabase->ObjectDelete (pgID, TRUE);
}

void CMIFLSocketServer::VMTimerToBeCalled (PCMIFLVM pVM)
{
   // clear out the CPU performance "current object" whenever
   // a timer is called

   if (gpMainWindow)
      gpMainWindow->CPUMonitor (NULL);
}

void CMIFLSocketServer::Log (PCMIFLVM pVM, PWSTR psz)
{
   // log the output without an associated user or player
   if (gpMainWindow && gpMainWindow->m_pTextLog)
      gpMainWindow->m_pTextLog->Log (psz);
}


void CMIFLSocketServer::MenuEnum (PCMIFLProj pProj, PCMIFLLib pLib, PCListVariable plMenu)
{
   // cant do for read-only
   if (!pLib || pLib->m_fReadOnly)
      return;

   PWSTR psz = L"Automatic transplanted prosody";
   plMenu->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
   return;
}



/*************************************************************************
AutoTransProsPage
*/
BOOL AutoTransProsPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   PATPINFO patp = (PATPINFO)pPage->m_pUserData;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         PCEscControl pControl;

         if (pControl = pPage->ControlFind (L"wavedir"))
            pControl->AttribSet (Text(), patp->szWaveDir);
         if (pControl = pPage->ControlFind (L"ttsfile"))
            pControl->AttribSet (Text(), patp->szTTSFile);
         if (pControl = pPage->ControlFind (L"srfile"))
            pControl->AttribSet (Text(), patp->szSRFile);
         if (pControl = pPage->ControlFind (L"srfile"))
            pControl->AttribSet (Text(), patp->szSRFile);
         if (pControl = pPage->ControlFind (L"autoclean"))
            pControl->AttribSetBOOL (Checked(), patp->fAutoClean);

         ComboBoxSet (pPage, L"quality", patp->dwQuality);
         DoubleToControl (pPage, L"numbers", patp->dwNumbers);

      }
      break;

   case ESCN_EDITCHANGE:
      {
         PESCNEDITCHANGE p = (PESCNEDITCHANGE)pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         DWORD dwNeed;
         if (!_wcsicmp (psz, L"wavedir")) {
            p->pControl->AttribGet (Text(), patp->szWaveDir, sizeof(patp->szWaveDir), &dwNeed);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"numbers")) {
            patp->dwNumbers = (DWORD) DoubleFromControl (pPage, L"numbers");
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

         if (!_wcsicmp(psz, L"autoclean")) {
            patp->fAutoClean = p->pControl->AttribGetBOOL (Checked());
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newsrfile")) {
            if (!VoiceFileOpenDialog (pPage->m_pWindow->m_hWnd, patp->szSRFile,
               sizeof(patp->szSRFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            PCEscControl pControl;
            if (pControl = pPage->ControlFind (L"srfile"))
               pControl->AttribSet (Text(), patp->szSRFile);
            return TRUE;
         }
         else if (!_wcsicmp(psz, L"newttsfile")) {
            if (!TTSFileOpenDialog (pPage->m_pWindow->m_hWnd, patp->szTTSFile,
               sizeof(patp->szTTSFile)/sizeof(WCHAR), FALSE))
               return TRUE;

            PCEscControl pControl;
            if (pControl = pPage->ControlFind (L"ttsfile"))
               pControl->AttribSet (Text(), patp->szTTSFile);
            return TRUE;
         }
         else if (!_wcsicmp (psz, L"scan")) {
            DWORD i;
            PCMIFLLib pLib = patp->pLib;
            PCMIFLResource pRes;
            PWSTR psz;
            PWSTR pszTransProsAuto = L"rTransProsAuto";
            DWORD dwTransProsAutoLen = (DWORD)wcslen(pszTransProsAuto);
            DWORD dwIndex;

            // what's the curent language ID
            LANGID *plid = (LANGID*) patp->pProj->m_lLANGID.Get(0);
            LANGID lid = plid ? *plid : 1033;   // default to english

            pLib->AboutToChange ();

            // potentially clear all existing transpros
            if (patp->fAutoClean) {
               CListVariable lRemove;

               for (i = 0; i < pLib->ResourceNum(); i++) {
                  pRes = pLib->ResourceGet (i);
                  if (_wcsicmp((PWSTR)pRes->m_memType.p, gpszTransPros))
                     continue;   // not transplanted prosody

                  psz = (PWSTR)pRes->m_memName.p;
                  if (!_wcsnicmp(psz, pszTransProsAuto, dwTransProsAutoLen))
                     lRemove.Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
               } // i

               // remove all the resource
               for (i = 0; i < lRemove.Num(); i++) {
                  dwIndex = pLib->ResourceFind ((PWSTR)lRemove.Get(i), (DWORD)-1);
                  if (dwIndex == (DWORD)-1)
                     continue;
                  pRes = pLib->ResourceGet (dwIndex);
                  pLib->ResourceRemove (pRes->m_dwTempID, FALSE);
               } // i
            }

            // create a list of all existing resources, going from the
            // resource <OrigText> string to resource name
            CHashString hOrigText;
            PCMMLNode2 pNode, pSub, pSub2;
            PWSTR pszName;
            hOrigText.Init (sizeof(PWSTR));
            for (i = 0; i < pLib->ResourceNum(); i++) {
               pRes = pLib->ResourceGet (i);
               if (_wcsicmp((PWSTR)pRes->m_memType.p, gpszTransPros))
                  continue;   // not transplanted prosody

               psz = (PWSTR)pRes->m_memName.p;
               if (_wcsnicmp(psz, pszTransProsAuto, dwTransProsAutoLen))
                  continue;   // not an autotranspros resource

               // else, auto transpros
               pNode = pRes->Get (lid);
               if (!pNode)
                  continue;

               // find the origtext
               pSub = NULL;
               pNode->ContentEnum (pNode->ContentFind(gpszOrigText), &psz, &pSub);
               if (!pSub)
                  continue;   // not found

               pSub2 = NULL;
               psz = NULL;
               pSub->ContentEnum (0, &psz, &pSub2);
               if (!psz)
                  continue;   // no string

               // else, remember
               pszName = (PWSTR)pRes->m_memName.p;
               hOrigText.Add (psz, &pszName);
            } // i

            // find all the wave files
            CListVariable lFiles;
            WCHAR szDir[256], szSearch[256];
            wcscpy (szDir, patp->szWaveDir);
            DWORD dwLen = (DWORD)wcslen(szDir);
            if (!dwLen || (szDir[dwLen-1] != L'\\'))
               wcscat (szDir, L"\\");
            wcscpy (szSearch, szDir);
            wcscat (szSearch, L"*.wav");

            // enumerate contents
            WIN32_FIND_DATAW fd;
            HANDLE hFile = FindFirstFileW (szSearch, &fd);
            WCHAR szFullFile[256];
            if (hFile != INVALID_HANDLE_VALUE) {
               while (TRUE) {
                  dwLen = (DWORD)wcslen(fd.cFileName);
                  if (dwLen <= 4)   // must have .wav
                     continue;

                  wcscpy (szFullFile, szDir);
                  wcscat (szFullFile, fd.cFileName);
                  lFiles.Add (szFullFile, (wcslen(szFullFile)+1)*sizeof(WCHAR));

                  // get the text one
                  if (!FindNextFileW (hFile, &fd))
                     break;
               }
               FindClose (hFile);
            } // if not invalid

            // if not files were found then error
            if (!lFiles.Num()) {
               pPage->MBWarning (L".wav files were not found",
                  L"No .wav files were found in the specified directory. Make sure "
                  L"that you typed the directory name in properly.");
               pLib->Changed();
               return TRUE;
            }

            // load in the recognizer
            CVoiceFile VF;
            if (!VF.Open (patp->szSRFile)) {
               pPage->MBWarning (L"The speech recognition training file couldn't be opened.",
                  L"You must have a training file to do transplanted prosody.");
               pLib->Changed();
               return TRUE;
            }

            // load in the TTS engine
            PCMTTS pTTS = TTSCacheOpen (patp->szTTSFile, TRUE, FALSE);
            if (!pTTS) {
               pPage->MBWarning (L"The text-to-speech engine couldn't be opened.",
                  L"You must have a training file to do transplanted prosody.");
               pLib->Changed();
               return TRUE;
            }

            {
               CProgress Progress;
               CTTSTransPros TP;
               CM3DWave Wave;
               CMem memTP;
               DWORD dwCurResNum = patp->dwNumbers;
               WCHAR szTemp[256];
               Progress.Start (pPage->m_pWindow->m_hWnd, "Scanning...", TRUE);
               for (i = 0; i < lFiles.Num(); i++) {
                  Progress.Push ((fp)(i+1) / (fp)lFiles.Num(), (fp)i / (fp)lFiles.Num());

                  PWSTR pszFile = (PWSTR)lFiles.Get(i);
                  char szaTemp[512];
                  WideCharToMultiByte (CP_ACP, 0, pszFile, -1, szaTemp, sizeof(szaTemp), 0,0);

                  Progress.Update (0);

                  // read it in
                  if (!Wave.Open (NULL, szaTemp)) {
                     // error
                     pPage->MBWarning (pszFile,
                        L"The .wav file could not be opened and may be corrupt. "
                        L"Stopping automatic transplanted prosody."
                        );
                     Progress.Pop ();
                     pLib->Changed();
                     TTSCacheClose (pTTS);
                     return TRUE;
                  }

                  // make sure there's some text
                  PWSTR pszText = (PWSTR) Wave.m_memSpoken.p;
                  if (!pszText[0]) {
                     // error
                     pPage->MBWarning (pszFile,
                        L"The .wav file doesn't have a transcript written in it. Use the wave editor to specify one. "
                        L"Stopping automatic transplanted prosody."
                        );
                     Progress.Pop ();
                     pLib->Changed();
                     TTSCacheClose (pTTS);
                     return TRUE;
                  }

                  // make sure there's SR info
                  BOOL fSave = FALSE;
                  if (!Wave.m_dwSRSamples || !Wave.m_adwPitchSamples[PITCH_F0] || !Wave.m_lWVPHONEME.Num() ||
                     !Wave.m_lWVWORD.Num()) {
                        fSave = TRUE;

                        // calculate pitch
                        if (!Wave.m_adwPitchSamples[PITCH_F0]) {
                           Progress.Push (0, 0.25);
                           Wave.CalcPitch (WAVECALC_TRANSPROS, &Progress);
                           Progress.Pop();
                        }

                        // calculate SR features
                        Progress.Push (0.25, 0.5);
                        Wave.CalcSRFeaturesIfNeeded (WAVECALC_TRANSPROS, pPage->m_pWindow->m_hWnd, &Progress);
                        Progress.Pop ();

                        Progress.Push (0.5, 0.75);
                        VF.Recognize ((PWSTR)Wave.m_memSpoken.p, &Wave, FALSE, &Progress);
                        Progress.Pop ();
                     }

                  // set up the trans pros
                  //wcscpy (TP.m_szTTS, patp->szTTSFile);
                  //wcscpy (TP.m_szVoiceFile, patp->szSRFile);
                  TP.m_fAvgPitch = 0;
                  TP.m_dwQuality = patp->dwQuality;
                  TP.m_dwPitchType = 1;   // relative
                  TP.m_dwVolType = 2;  // absolute
                  TP.m_dwDurType = 2;  // absolute
                  TP.m_fPitchAdjust = 1.0;   // no change
                  TP.m_fPitchExpress = 1.0;  // no change
                  TP.m_fVolAdjust = 1.0;  // no change
                  TP.m_fDurAdjust = 1.0; // no change

                  // do transpros
                  Progress.Push (0.75, 1);
                  memTP.m_dwCurPosn = 0;
                  BOOL fRet = TP.WaveToPerPhoneTP ((PWSTR)Wave.m_memSpoken.p, &Wave, patp->dwQuality,
                     pTTS->Lexicon(), NULL, &memTP);
                  Progress.Pop ();

                  // save
                  if (fSave)
                     Wave.Save (TRUE, NULL);

                  // pop for entire file
                  Progress.Pop ();

                  if (!fRet) {
                     // error
                     pPage->MBWarning (pszFile,
                        L"Transplanted prosody wouldn't work with the .wav file. "
                        L"Stopping automatic transplanted prosody."
                        );
                     pLib->Changed();
                     TTSCacheClose (pTTS);
                     return TRUE;
                  }

                  // remove previous resource
                  PWSTR *ppsz = (PWSTR*)hOrigText.Find ((PWSTR)Wave.m_memSpoken.p);
                  if (ppsz) {
                     DWORD dwIndex = pLib->ResourceFind(*ppsz, (DWORD)-1);
                     if (dwIndex != (DWORD)-1) {
                        pRes = pLib->ResourceGet (dwIndex);
                        pLib->ResourceRemove (pRes->m_dwTempID, FALSE);
                     }
                  }

                  // find a unique name
                  for (; ; dwCurResNum++) {
                     swprintf (szTemp, L"%s%.5d", pszTransProsAuto, (int)dwCurResNum);

                     if ((DWORD)-1 != pLib->ResourceFind (szTemp, (DWORD)-1))
                        continue;   // error. already exists

                     // else, unique
                     dwCurResNum++; // for next time
                     break;
                  } // while TRUE

                  // add the resource
                  dwIndex = pLib->ResourceNew (gpszTransPros, FALSE);
                  pRes = pLib->ResourceGet (pLib->ResourceFind(dwIndex));
                  MemZero (&pRes->m_memName);
                  MemCat (&pRes->m_memName, szTemp);
                  pLib->ResourceSort ();  // sort so can find other names

                  MemZero (&pRes->m_memDescShort);
                  MemCat (&pRes->m_memDescShort, L"Automatic transplanted prosody from ");
                  MemCat (&pRes->m_memDescShort, pszFile);
                  MemCat (&pRes->m_memDescShort, L"\r\nText = ");
                  MemCat (&pRes->m_memDescShort, (PWSTR)Wave.m_memSpoken.p);

                  // text
                  CEscError err;
                  PCMMLNode pSub1 = ParseMML ((PWSTR)memTP.p, ghInstance, NULL, NULL, &err, FALSE);
                  pSub = pSub1 ? pSub1->CloneAsCMMLNode2() : NULL;
                  if (pSub1)
                     delete pSub1;
                  pSub2 = NULL;
                  if (pSub)
                     pSub->ContentEnum (0, &psz, &pSub2);
                  if (pSub2) {
                     pSub->ContentRemove (0, FALSE);  // to isolate pSub2
                     pSub2->NameSet (gpszTransPros);

                     pRes->m_lPCMMLNode2.Add (&pSub2);
                     pRes->m_lLANGID.Add (&lid);
                  }
                  if (pSub)
                     delete pSub;

               } // i

               // note that has changed
               pLib->Changed();
               TTSCacheClose (pTTS);

            } // Progress

            // MB that updated
            pPage->MBInformation (L"All the .wav files have been added as automatic transplanted prosody files.");

            // exit with a back
            pPage->Exit (Back());

            return TRUE;
         }
      }
      break;

   case ESCN_COMBOBOXSELCHANGE:
      {
         PESCNCOMBOBOXSELCHANGE p = (PESCNCOMBOBOXSELCHANGE) pParam;
         PWSTR psz = p->pControl->m_pszName;
         if (!psz)
            break;

         if (!_wcsicmp (psz, L"quality")) {
            DWORD dwVal = p->pszName ? _wtoi (p->pszName) : 0;
            if (dwVal == patp->dwQuality)
               return TRUE;

            // else set
            patp->dwQuality = dwVal;
            return TRUE;
         } // quality
      }
      break;

   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         if (!_wcsicmp(p->pszSubName, L"PAGETITLE")) {
            p->pszSubString = L"Automatic transplanted prosody";
            return TRUE;
         }
      }
      break;

   }; // dwMessage

   return FALSE;
}


static PWSTR gpszAutoTransPros = L"AutoTransPros";
static PWSTR gpszQuality = L"Quality";
static PWSTR gpszSRFile = L"SRFile";
static PWSTR gpszTTSFile = L"TTSFile";
static PWSTR gpszWaveDir = L"WaveDir";
static PWSTR gpszAutoClean = L"AutoClean";
static PWSTR gpszNumbers = L"Numbers";

BOOL CMIFLSocketServer::MenuCall (PCMIFLProj pProj, PCMIFLLib pLib, PCEscWindow pWindow, DWORD dwIndex)
{
   // cant do for read-only
   if (!pLib || pLib->m_fReadOnly)
      return TRUE;

   if (dwIndex == 0) {
      ATPINFO atp;

      memset (&atp, 0, sizeof(atp));
      atp.dwQuality = 1;
      atp.fAutoClean = FALSE;
      atp.dwNumbers = 1;
      atp.pLib = pLib;
      atp.pProj = pProj;
      
      // get the settings
      PCMMLNode2 pSub = NULL;
      PWSTR psz;
      pLib->m_pNodeMisc->ContentEnum (pLib->m_pNodeMisc->ContentFind (gpszAutoTransPros), &psz, &pSub);
      if (pSub) {
         atp.dwQuality = (DWORD)MMLValueGetInt (pSub, gpszQuality, (int)atp.dwQuality);
         atp.fAutoClean = (DWORD)MMLValueGetInt (pSub, gpszAutoClean, (int)atp.fAutoClean);
         atp.dwNumbers = (DWORD)MMLValueGetInt (pSub, gpszNumbers, (int)atp.dwNumbers);
         psz = MMLValueGet (pSub, gpszSRFile);
         if (psz)
            wcscpy (atp.szSRFile, psz);
         psz = MMLValueGet (pSub, gpszTTSFile);
         if (psz)
            wcscpy (atp.szTTSFile, psz);
         psz = MMLValueGet (pSub, gpszWaveDir);
         if (psz)
            wcscpy (atp.szWaveDir, psz);
      }

      // auto transplanted prosody
      PWSTR pszRet = pWindow->PageDialog (ghInstance, IDR_MMLAUTOTRANSPROS, AutoTransProsPage, &atp);

      // save the info
      pLib->AboutToChange();
      pSub = NULL;
      DWORD dwIndex = pLib->m_pNodeMisc->ContentFind (gpszAutoTransPros);
      if (dwIndex != (DWORD)-1)
         pLib->m_pNodeMisc->ContentRemove (dwIndex);
      pSub = pLib->m_pNodeMisc->ContentAddNewNode ();
      if (pSub) {
         pSub->NameSet (gpszAutoTransPros);
         MMLValueSet (pSub, gpszQuality, (int)atp.dwQuality);
         MMLValueSet (pSub, gpszAutoClean, (int)atp.fAutoClean);
         MMLValueSet (pSub, gpszNumbers, (int)atp.dwNumbers);
         if (atp.szSRFile[0])
            MMLValueSet (pSub, gpszSRFile, atp.szSRFile);
         if (atp.szTTSFile[0])
            MMLValueSet (pSub, gpszTTSFile, atp.szTTSFile);
         if (atp.szWaveDir[0])
            MMLValueSet (pSub, gpszWaveDir, atp.szWaveDir);
      }

      pLib->Changed();

      return (pszRet && !_wcsicmp(pszRet, Back()));
   }

   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::NLPParserEnum - Enumerates the parsers in gpMainWindow->m_NLP.

inputs
   none
returns
   List continaing the strings for the parser names
*/
BOOL CMIFLSocketServer::NLPParserEnum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   PCMIFLVarList pl = new CMIFLVarList;
   pRet->SetList (pl);
   pl->Release(); // can do this safely because know that pRet will hold onto the list

   DWORD i;
   CMIFLVar var;
   for (i = 0; i < gpMainWindow->m_NLP.ParserNum(); i++) {
      PCCircumrealityNLPParser pParse = gpMainWindow->m_NLP.ParserGet (i);
      if (!pParse)
         continue;
      var.SetString ((PWSTR)pParse->m_memName.p);
      pl->Add (&var, TRUE);
   } // i

   return TRUE;
}

/*************************************************************************************
CMIFLSocketServer::NLPParserRemove - Removes a parser from in gpMainWindow->m_NLP.

inputs
   string - Parser name
returns
   TRUE if removed, FALSE if cant find
*/
BOOL CMIFLSocketServer::NLPParserRemove (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() != 1)
      return FALSE;

   // get the parser
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   DWORD dwIndex = gpMainWindow->m_NLP.ParserFind (ps->Get());
   ps->Release();
   if (dwIndex == -1) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   // remove
   pRet->SetBOOL (gpMainWindow->m_NLP.ParserRemove (dwIndex));
   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::NLPParserClone - Clones a parser from in gpMainWindow->m_NLP.

inputs
   string - Parser name of orig
   string - Parser name of new
returns
   TRUE if clone, FALSE if cant find
*/
BOOL CMIFLSocketServer::NLPParserClone (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() != 2)
      return FALSE;

   // get the parser
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   DWORD dwIndex = gpMainWindow->m_NLP.ParserFind (ps->Get());
   ps->Release();
   if (dwIndex == -1) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   dwIndex = gpMainWindow->m_NLP.ParserClone (dwIndex);
   PCCircumrealityNLPParser pParser = gpMainWindow->m_NLP.ParserGet (dwIndex);
   if (!pParser) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   // set the name
   ps = plParams->Get(1)->GetString(pVM);
   MemZero (&pParser->m_memName);
   MemCat (&pParser->m_memName, ps->Get());
   ps->Release();

   // remove
   pRet->SetBOOL (TRUE);
   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::NLPRuleSetEnum - Enumerates the rule sets in gpMainWindow->m_NLP.

inputs
   string - Parser name
   bool - if TRUE then only enumerate enabled rules, FALSE then disabled. Otherwise, all rules
returns
   List continaing the strings for the parser names
*/
BOOL CMIFLSocketServer::NLPRuleSetEnum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() != 2)
      return FALSE;

   // get the parser
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   DWORD dwIndex = gpMainWindow->m_NLP.ParserFind (ps->Get());
   ps->Release();
   PCCircumrealityNLPParser pParser = gpMainWindow->m_NLP.ParserGet (dwIndex);
   if (!pParser) {
      pRet->SetBOOL (NULL);
      return TRUE;
   }

   // which rules
   BOOL fEnabled = plParams->Get(1)->GetBOOL(pVM);
   BOOL fIsBOOL = (plParams->Get(1)->TypeGet() == MV_BOOL);

   PCMIFLVarList pl = new CMIFLVarList;
   pRet->SetList (pl);
   pl->Release(); // can do this safely because know that pRet will hold onto the list

   DWORD i;
   CMIFLVar var;
   for (i = 0; i < pParser->RuleSetNum(); i++) {
      PCCircumrealityNLPRuleSet pRuleSet = pParser->RuleSetGet (i);
      if (!pRuleSet)
         continue;

      if (fIsBOOL && (fEnabled != pRuleSet->m_fEnabled))
         continue;   // dont want to enumerate this

      var.SetString ((PWSTR)pRuleSet->m_memName.p);
      pl->Add (&var, TRUE);
   } // i

   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::NLPRuleSetRemove - Removes a rule sets in gpMainWindow->m_NLP.

inputs
   string - Parser name
   string - Rule set
returns
   TRUE if found the rule set and removed, FALSE if not
*/
BOOL CMIFLSocketServer::NLPRuleSetRemove (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() != 2)
      return FALSE;

   // get the parser
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   DWORD dwIndex = gpMainWindow->m_NLP.ParserFind (ps->Get());
   ps->Release();
   PCCircumrealityNLPParser pParser = gpMainWindow->m_NLP.ParserGet (dwIndex);
   if (!pParser) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   // find the rule set
   ps = plParams->Get(1)->GetString(pVM);
   dwIndex = pParser->RuleSetFind (ps->Get());
   ps->Release();
   if (dwIndex == -1) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   pParser->RuleSetRemove (dwIndex);
   pRet->SetBOOL (TRUE);
   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::NLPRuleSetEnableGet - Returns if a rule set is enabled in gpMainWindow->m_NLP.

inputs
   string - Parser name
   string - Rule set.
returns
   TRUE if the rule set is enabled, FALSE if not, NULL if can't find
*/
BOOL CMIFLSocketServer::NLPRuleSetEnableGet (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() != 2)
      return FALSE;

   // get the parser
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   DWORD dwIndex = gpMainWindow->m_NLP.ParserFind (ps->Get());
   ps->Release();
   PCCircumrealityNLPParser pParser = gpMainWindow->m_NLP.ParserGet (dwIndex);
   if (!pParser) {
      pRet->SetNULL ();
      return TRUE;
   }

   // find the rule set
   ps = plParams->Get(1)->GetString(pVM);
   dwIndex = pParser->RuleSetFind (ps->Get());
   ps->Release();
   if (dwIndex == -1) {
      pRet->SetNULL();
      return TRUE;
   }

   PCCircumrealityNLPRuleSet pRuleSet = pParser->RuleSetGet(dwIndex);
   pRet->SetBOOL (pRuleSet->m_fEnabled);
   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::NLPRuleSetEnableAll - Enabled/disables all the rule sets
for the parser.

inputs
   string - Parser name
   bool - enable (FALSE for disable)
returns
   TRUE if the rule set is enabled, FALSE if not, NULL if can't find
*/
BOOL CMIFLSocketServer::NLPRuleSetEnableAll (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() != 2)
      return FALSE;

   // get the parser
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   DWORD dwIndex = gpMainWindow->m_NLP.ParserFind (ps->Get());
   ps->Release();
   PCCircumrealityNLPParser pParser = gpMainWindow->m_NLP.ParserGet (dwIndex);
   if (!pParser) {
      pRet->SetNULL ();
      return TRUE;
   }

   BOOL fEnable = plParams->Get(1)->GetBOOL(pVM);

   // loop
   DWORD i;
   DWORD dwNum = pParser->RuleSetNum();
   for (i = 0; i < dwNum; i++) {
      PCCircumrealityNLPRuleSet pRule = pParser->RuleSetGet (i);
      pRule->m_fEnabled = fEnable;
   } // i

   pRet->SetBOOL (fEnable);
   return TRUE;
}

/*************************************************************************************
CMIFLSocketServer::NLPRuleSetExists - Checks to see if a rule set exists.

inputs
   string - Parser name
   string - Rule set.
   number - Language ID. Make sure the rule exists for the given language. If
            NULL then any language is OK.
returns
   TRUE if the rule set exists, FALSE if doesn't.
*/
BOOL CMIFLSocketServer::NLPRuleSetExists (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() != 3)
      return FALSE;

   // get the parser
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   DWORD dwIndex = gpMainWindow->m_NLP.ParserFind (ps->Get());
   ps->Release();
   PCCircumrealityNLPParser pParser = gpMainWindow->m_NLP.ParserGet (dwIndex);
   if (!pParser) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   // find the rule set
   ps = plParams->Get(1)->GetString(pVM);
   dwIndex = pParser->RuleSetFind (ps->Get());
   ps->Release();
   if (dwIndex == -1) {
      pRet->SetBOOL(FALSE);
      return TRUE;
   }

   PCCircumrealityNLPRuleSet pRuleSet = pParser->RuleSetGet(dwIndex);
   LANGID lid = (LANGID)plParams->Get(2)->GetDouble(pVM);
   if (lid) {
      LANGID lidOld = pRuleSet->LanguageGet ();
      if (lidOld != lid) {
         BOOL fSet = pRuleSet->LanguageSet(lid, TRUE);
         pRuleSet->LanguageSet (lidOld, FALSE);
   
         if (!fSet) {
            pRet->SetBOOL(FALSE);
            return TRUE;
         }
      }
   }

   pRet->SetBOOL (TRUE);
   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::NLPRuleSetEnableSet - Sets if a rule set is enabled in gpMainWindow->m_NLP.

inputs
   string - Parser name
   string - Rule set.
   bool - To enable or disable
returns
   TRUE if success, FALSE if cant find rule
*/
BOOL CMIFLSocketServer::NLPRuleSetEnableSet (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() != 3)
      return FALSE;

   // get the parser
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   DWORD dwIndex = gpMainWindow->m_NLP.ParserFind (ps->Get());
   ps->Release();
   PCCircumrealityNLPParser pParser = gpMainWindow->m_NLP.ParserGet (dwIndex);
   if (!pParser) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   // find the rule set
   ps = plParams->Get(1)->GetString(pVM);
   dwIndex = pParser->RuleSetFind (ps->Get());
   ps->Release();
   if (dwIndex == -1) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   PCCircumrealityNLPRuleSet pRuleSet = pParser->RuleSetGet(dwIndex);
   pRuleSet->m_fEnabled = plParams->Get(2)->GetBOOL(pVM);
   pRet->SetBOOL (TRUE);
   return TRUE;
}

static PWSTR gpszTempRuleSet = L"!TempRuleSet!";

/*************************************************************************************
CMIFLSocketServer::NLPParse - Parses using a parser from in gpMainWindow->m_NLP.

inputs
   string - Parser name
   string - Can be NULL, or a <NLPRuleSet>...</NLPRuleSet> string that can be
            used for temporary rules (for the given parse)
   string - String to parse
returns
   List of parses. Each parse is a list, with the first element containing a probability
   score from 0..1, and the subsequent parse being one or more sentences.
   Each sentence is a list, containing the word strings of the parse.
   Instead of a word string for |GUID, these will be converted to actual object referenes.
*/
BOOL CMIFLSocketServer::NLPParse (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() != 3)
      return FALSE;

   // get the parser
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   DWORD dwIndex = gpMainWindow->m_NLP.ParserFind (ps->Get());
   ps->Release();
   PCCircumrealityNLPParser pParser = gpMainWindow->m_NLP.ParserGet (dwIndex);
   if (!pParser) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   // set the language
   pParser->LanguageSet (pVM->m_LangID, 0);
      // BUGFIX - Was 1, but go for anything if cant find exact match


   // add temporary NLP rules
   BOOL fAddedRules;
   PCMIFLVar pv = plParams->Get(1);
   switch (pv->TypeGet()) {
      case MV_NULL:
      case MV_UNDEFINED:
         fAddedRules = FALSE;
         break;
      default:
         fAddedRules = TRUE;
         break;
   }
   if (fAddedRules) {
      // if set the probably have some temporary rules to set
      PCMIFLVarString ps = pv->GetString(pVM);
      PCMMLNode2 pNode = CircumrealityParseMML(ps->Get());
      ps->Release();
      if (pNode && pNode->NameGet() && !_wcsicmp(pNode->NameGet(), CircumrealityNLPRuleSet())) {
         DWORD dwIndex = pParser->RuleSetAdd (gpszTempRuleSet);
         PCCircumrealityNLPRuleSet pRuleSet = pParser->RuleSetGet (dwIndex);
         pRuleSet->MMLFrom (pNode, pVM->m_LangID);
      }
      else
         fAddedRules = FALSE;
      if (pNode)
         delete pNode;
   }

   // parse
   ps = plParams->Get(2)->GetString(pVM);
   BOOL fRet = pParser->Parse (ps->Get());
   ps->Release();

   // remove temporary NLP rules
   if (fAddedRules) {
      DWORD dwIndex = pParser->RuleSetFind (gpszTempRuleSet);
      if (dwIndex != -1)
         pParser->RuleSetRemove (dwIndex);
   }

   if (!fRet) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }


   // create list to store hypothesis in
   PCMIFLVarList pl = new CMIFLVarList;
   pRet->SetList (pl);
   pl->Release(); // can do this safely because know that pRet will hold onto the list

   // create a hash of IDs to tokens
   CMIFLVar var;  // initialed to unknown
   CHashDWORD hToVar;
   hToVar.Init (sizeof(var), 256);  // reasonable default size

   // loop through all the hypothesis
   DWORD i;
   for (i = 0; i < pParser->HypNum(); i++) {
      PCCircumrealityNLPHyp pHyp = pParser->HypGet(i);
      if (!pHyp)
         continue;

      // create a list for the sentences
      PCMIFLVarList plProb = new CMIFLVarList;

      // add the probability as the first element
      var.SetDouble (pHyp->m_fProb);
      plProb->Add (&var, TRUE);

      // add multiple sentences
      DWORD *padw = (DWORD*) pHyp->m_memTokens.p;
      DWORD dwNum = (DWORD)pHyp->m_memTokens.m_dwCurPosn /sizeof(DWORD);
      DWORD dwSentStart, dwSentEnd;
      for (dwSentStart = 0; dwSentStart < dwNum; dwSentStart = dwSentEnd + 1) {
         // find start and end of sentence
         for (dwSentEnd = dwSentStart; dwSentEnd < dwNum; dwSentEnd++)
            if (padw[dwSentEnd] == (DWORD)-1)
               break;
         if (dwSentEnd == dwSentStart)
            continue;   // empty sentence

         // create a new list
         PCMIFLVarList plSent = new CMIFLVarList;

         // loop over all words
         DWORD j;
         for (j = dwSentStart; j < dwSentEnd; j++) {
            // is it in the hash already
            PCMIFLVar pv = (PCMIFLVar)hToVar.Find(padw[j]);
            if (pv) {
               plSent->Add (pv, TRUE);
               continue;
            }

            // else, need to add
            PWSTR psz = pParser->HypTokenGet (padw[j]);
            if (!psz)
               continue;

            // see if it's a guid
            if ((psz[0] == L'|') && (wcslen(psz) == sizeof(GUID)*2+1)) {
               GUID g;
               g = GUID_NULL;
               MMLBinaryFromString (psz + 1, (PBYTE)&g, sizeof(g));
               var.SetObject (&g);
            }
            else {
               // just keep the string
               var.SetString (psz);

               // BUGFIX - Lowecase psz if it begins with a '`'
               if (psz[0] == L'`') {
                  PCMIFLVarString ps = var.GetString(pVM);
                  psz = ps->Get();
                  _wcslwr (psz);
                  ps->Release();
               }
            }

            // add it to hash
            var.AddRef();  // so ref count will be correct
            hToVar.Add (padw[j], &var);

            // add to list
            plSent->Add (&var, TRUE);
         } // j

         // add this sentence
         var.SetList (plSent);
         plProb->Add (&var, TRUE);
         plSent->Release();
      } // dwSentStart

      // add the probability list to the main list
      var.SetList (plProb);
      pl->Add (&var, TRUE);
      plProb->Release();
   } // i

   // free up the hash contents
   for (i = 0; i < hToVar.Num(); i++) {
      PCMIFLVar pv = (PCMIFLVar)hToVar.Get(i);
      pv->SetUndefined();
   } // i

   // done
   // list has already been set above
   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::NLPRuleSetAdd - Adds a rule sets in gpMainWindow->m_NLP.

The new rule set will be enabled.

inputs
   string - Parser name
   string - Rule set. If it alreayd exists it will be overwritten.
   resource ID, string ID, or string - Adds the rule set using the given resource.
      The resource must be of type <NLPRuleSet>...</NLPRuleSet>. If the resource ID
      or string ID is passed in, and multiple languages are supported by the resource,
      then the resource will be added for each of the languages. If only one language
      is supported then it will be added for the current language.
returns
   TRUE if found the rule set was added, FALSE if not
*/
BOOL CMIFLSocketServer::NLPRuleSetAdd (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() != 3)
      return FALSE;

   // get the parser
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   DWORD dwIndex = gpMainWindow->m_NLP.ParserFind (ps->Get());
   if (dwIndex == -1)
      dwIndex = gpMainWindow->m_NLP.ParserAdd (ps->Get());  // else add it
   ps->Release();
   PCCircumrealityNLPParser pParser = gpMainWindow->m_NLP.ParserGet (dwIndex);
   if (!pParser) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   // find the rule set
   ps = plParams->Get(1)->GetString(pVM);
   dwIndex = pParser->RuleSetFind (ps->Get());
   if (dwIndex != -1)
      pParser->RuleSetRemove (dwIndex);   // remove existing
   dwIndex = pParser->RuleSetAdd (ps->Get());
   ps->Release();
   PCCircumrealityNLPRuleSet pRuleSet = pParser->RuleSetGet (dwIndex);
   if (!pRuleSet) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   // see if want complex resource adding
   BOOL fComplex = FALSE;
   PCMIFLVar pv = plParams->Get(2);
   PCMIFLString pms = NULL;
   PCMIFLResource pmr = NULL;
   switch (pv->TypeGet()) {
   case MV_STRINGTABLE:
      fComplex = TRUE;
      pms = pVM->m_pCompiled->m_pLib->StringGet (pv->GetValue());
      if (!pms || (pms->m_lString.Num() < 2))
         fComplex = FALSE; // not enough langages
      break;

   case MV_RESOURCE:
      fComplex = TRUE;
      pmr = pVM->m_pCompiled->m_pLib->ResourceGet (pv->GetValue());
      if (!pmr || (pmr->m_lPCMMLNode2.Num() < 2))
         fComplex = FALSE; // not enough languages
      break;
   }

   // if it's complex, then loop and add
   DWORD i;
   if (fComplex) {
      LANGID *plid = (LANGID*) (pms ? pms->m_lLANGID.Get(0) : pmr->m_lLANGID.Get(0));
      DWORD dwNum = pms ? pms->m_lLANGID.Num() : pmr->m_lLANGID.Num();
      for (i = 0; i < dwNum; i++) {
         // get MML
         PCMMLNode2 pNode;
         if (pms)
            pNode = CircumrealityParseMML((PWSTR) pms->m_lString.Get(i));
         else
            pNode = *((PCMMLNode2*)pmr->m_lPCMMLNode2.Get(i));
         if (!pNode) {
            pParser->RuleSetRemove (dwIndex);
            pRet->SetBOOL (FALSE);
            return TRUE;
         }

         // make sure it's the right type...
         BOOL fRet = (pNode && pNode->NameGet() && !_wcsicmp(pNode->NameGet(), CircumrealityNLPRuleSet()));
         if (fRet)
            fRet = pRuleSet->MMLFrom (pNode, plid[i]);

         // delete node if it was created by a string
         if (pms)
            delete pNode;
         if (!fRet) {
            pParser->RuleSetRemove (dwIndex);
            pRet->SetBOOL (FALSE);
            return TRUE;
         }
      } // i
   }
   else {
      PCMMLNode2 pNode;
      if (pmr)
         // right from resource, so dont bother doing extra parsing step
         pNode = pmr->Get (pVM->m_LangID);
      else {
         // string or something
         PCMIFLVarString ps = pv->GetString(pVM);
         pNode = CircumrealityParseMML(ps->Get());
         ps->Release();
      }

      if (!pNode) {
         pParser->RuleSetRemove (dwIndex);
         pRet->SetBOOL (FALSE);
         return TRUE;
      }

      // make sure it's the right type...
      BOOL fRet = (pNode && pNode->NameGet() && !_wcsicmp(pNode->NameGet(), CircumrealityNLPRuleSet()));
      if (fRet)
         fRet = pRuleSet->MMLFrom (pNode, pVM->m_LangID);

      // delete node
      if (pNode && !pmr)
         delete pNode;
      if (!fRet) {
         pParser->RuleSetRemove (dwIndex);
         pRet->SetBOOL (FALSE);
         return TRUE;
      }
   }

   // done
   pRet->SetBOOL (TRUE);
   return TRUE;
}



/*************************************************************************************
CMIFLSocketServer::NLPVerbForm - NLPCalls the VerbForm function

inputs
   string - String to parse
returns
   New string form, or NULL if error
*/
BOOL CMIFLSocketServer::NLPVerbForm (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() != 1)
      return FALSE;

   CMem mem;
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   BOOL fRet = ::NLPVerbForm(ps->Get(), pVM, &mem);
   ps->Release();
   if (fRet)
      pRet->SetString((PWSTR)mem.p);
   else
      pRet->SetNULL ();

   return TRUE;
}



/*************************************************************************************
CMIFLSocketServer::NLPNounCase - NLPCalls the NounCase function

inputs
   string - String to parse
returns
   New string form, or NULL if error
*/
BOOL CMIFLSocketServer::NLPNounCase (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   if (plParams->Num() < 2)
      return FALSE;

   // get the noun case
   PCMIFLVar pNounCase = plParams->Get(2);
   BOOL fNounCase = (pNounCase && (pNounCase->TypeGet() != MV_UNDEFINED)) ? pNounCase->GetBOOL(pVM) : TRUE;

   CMem mem;
   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   PWSTR psz = ::NLPNounCase(ps->Get(), &mem, (DWORD)plParams->Get(1)->GetDouble(pVM), fNounCase);
   ps->Release();
   if (psz)
      pRet->SetString(psz);
   else
      pRet->SetNULL ();

   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::SavedGameFilesEnum - Enumerates all the main files for saved games.

inputs
   none
returns
   List of filenames
*/
BOOL CMIFLSocketServer::SavedGameFilesEnum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 0)
      return FALSE;

   if (!gpMainWindow || !gpMainWindow->m_pInstance) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   CListVariable lName;
   gpMainWindow->m_pInstance->Enum (&lName);

   PCMIFLVarList pl = new CMIFLVarList;
   CMIFLVar v;
   if (!pl) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }
   pRet->SetList (pl);

   // copy over to list
   DWORD i;
   for (i = 0; i < lName.Num(); i++) {
      v.SetString ((PWSTR)lName.Get(i));
      pl->Add (&v, TRUE);
   } // i


   // finally
   pl->Release();
   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::SavedGameFilesDelete - Deletes a main saved-game file.

inputs
   string - filename to delete
returns
   BOOL - TRUE if deleted
*/
BOOL CMIFLSocketServer::SavedGameFilesDelete (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 1)
      return FALSE;

   if (!gpMainWindow || !gpMainWindow->m_pInstance) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   PCMIFLVarString ps = plParams->Get(0)->GetString(pVM);
   BOOL fRet = gpMainWindow->m_pInstance->Delete (ps->Get());
   ps->Release();

   pRet->SetBOOL (fRet);

   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::SavedGameFilesNum - Returns the number of saved game files.

inputs
returns
   double - number
*/
BOOL CMIFLSocketServer::SavedGameFilesNum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 0)
      return FALSE;

   if (!gpMainWindow || !gpMainWindow->m_pInstance) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   pRet->SetDouble (gpMainWindow->m_pInstance->Num());

   return TRUE;
}



/*************************************************************************************
CMIFLSocketServer::SavedGameFilesName - Returns the name of saved games based on its index.

inputs
   double - index
returns
   string - name, or NULL if bad
*/
BOOL CMIFLSocketServer::SavedGameFilesName (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 1)
      return FALSE;

   if (!gpMainWindow || !gpMainWindow->m_pInstance) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   CMem mem;
   BOOL fRet = gpMainWindow->m_pInstance->GetNum ((DWORD)plParams->Get(0)->GetDouble(pVM), &mem);
   if (fRet)
      pRet->SetString ((PWSTR)mem.p);
   else
      pRet->SetNULL();

   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::SavedGameEnum - Enumerate the saved games.

inputs
   string - file name
returns
   List of saved games. Each saved game is a sub-list with the name, followed
   by creation time, last modification time, last access time
*/
BOOL CMIFLSocketServer::SavedGameEnum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 1)
      return FALSE;

   PCInstanceFile pif = (gpMainWindow && gpMainWindow->m_pInstance) ?
      gpMainWindow->m_pInstance->InstanceFileGet (pVM, plParams->Get(0), FALSE) :
      NULL;
   if (!pif) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   CListVariable lName;
   CListFixed lMFFILEINFO;
   lMFFILEINFO.Init (sizeof(MFFILEINFO));
   pif->Enum (&lName, &lMFFILEINFO);

   PCMIFLVarList pl = new CMIFLVarList;
   CMIFLVar v;
   if (!pl) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }
   pRet->SetList (pl);

   // copy over to list
   DWORD i;
   PMFFILEINFO pfi = (PMFFILEINFO)lMFFILEINFO.Get(0);
   for (i = 0; i < lName.Num(); i++, pfi++) {
      PCMIFLVarList pSub = new CMIFLVarList;
      if (!pSub)
         continue;

      // name
      v.SetString ((PWSTR)lName.Get(i));
      pSub->Add (&v, TRUE);

      // dates
      v.SetDouble (MIFLFileTimeToDouble (&pfi->iTimeCreate));
      pSub->Add (&v, TRUE);
      v.SetDouble (MIFLFileTimeToDouble (&pfi->iTimeModify));
      pSub->Add (&v, TRUE);
      v.SetDouble (MIFLFileTimeToDouble (&pfi->iTimeAccess));
      pSub->Add (&v, TRUE);

      // add it
      v.SetList (pSub);
      pSub->Release();
      pl->Add (&v, TRUE);
   } // i


   // finally
   pl->Release();
   return TRUE;
}




/*************************************************************************************
CMIFLSocketServer::SavedGameNum - returns the numberof saved games

inputs
   string - file name
returns
   Number of saved games
*/
BOOL CMIFLSocketServer::SavedGameNum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 1)
      return FALSE;

   PCInstanceFile pif = (gpMainWindow && gpMainWindow->m_pInstance) ?
      gpMainWindow->m_pInstance->InstanceFileGet (pVM, plParams->Get(0), FALSE) :
      NULL;
   if (!pif) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   pRet->SetDouble (pif->Num());

   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::SavedGameName - returns the name of the index

inputs
   string - file name
   double - index
returns
   String of the sub-file name, or 0 if none.
*/
BOOL CMIFLSocketServer::SavedGameName (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 2)
      return FALSE;

   PCInstanceFile pif = (gpMainWindow && gpMainWindow->m_pInstance) ?
      gpMainWindow->m_pInstance->InstanceFileGet (pVM, plParams->Get(0), FALSE) :
      NULL;
   if (!pif) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   CMem mem;
   if (!pif->GetNum ((DWORD) plParams->Get(1)->GetDouble(pVM), &mem))
      pRet->SetNULL();
   else
      pRet->SetString ((PWSTR)mem.p);

   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::SavedGameInfo - Returns information about a sub-file

inputs
   string - file name
   string - sub-file name
returns
   Returns a list with creation time, last modification time, last access time.
   FALSE if error
*/
BOOL CMIFLSocketServer::SavedGameInfo (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 2)
      return FALSE;

   PCInstanceFile pif = (gpMainWindow && gpMainWindow->m_pInstance) ?
      gpMainWindow->m_pInstance->InstanceFileGet (pVM, plParams->Get(0), FALSE) :
      NULL;
   if (!pif) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   PCMIFLVarString ps = plParams->Get(1)->GetString(pVM);
   MFFILEINFO info;
   BOOL fRet = pif->Exists (ps->Get(), &info);
   ps->Release();

   if (!fRet) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   // else, list
   PCMIFLVarList pSub = new CMIFLVarList;
   if (!pSub) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   CMIFLVar v;

   // dates
   v.SetDouble (MIFLFileTimeToDouble (&info.iTimeCreate));
   pSub->Add (&v, TRUE);
   v.SetDouble (MIFLFileTimeToDouble (&info.iTimeModify));
   pSub->Add (&v, TRUE);
   v.SetDouble (MIFLFileTimeToDouble (&info.iTimeAccess));
   pSub->Add (&v, TRUE);

   // add it
   pRet->SetList (pSub);
   pSub->Release();

   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::SavedGameRemove - Deletes a saved game based on the name.

inputs
   string - file name
   string - sub-file name
returns
   List of saved games. Each saved game is a sub-list with the name, followed
   by creation time, last modification time, last access time
*/
BOOL CMIFLSocketServer::SavedGameRemove (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 2)
      return FALSE;

   PCInstanceFile pif = (gpMainWindow && gpMainWindow->m_pInstance) ?
      gpMainWindow->m_pInstance->InstanceFileGet (pVM, plParams->Get(0), FALSE) :
      NULL;
   if (!pif) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   PCMIFLVarString ps = plParams->Get(1)->GetString(pVM);
   pRet->SetBOOL (pif->Delete (ps->Get()));
   ps->Release();
   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::SavedGameSave - Saves the info

inputs
   string - filename
   string - sub-file name of the game
   bool - if TRUE then save all objects BUT exclude the objects in the list,
            else if FALSE then include only those objects in the list
   bool - if TRUE then include children of the object (for inclusion or exclusion)
   list - list of objects to exclude or include
returns
   TRUE if saved, FALSE if failed
*/
BOOL CMIFLSocketServer::SavedGameSave (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 5)
      return FALSE;

   PCInstanceFile pif = (gpMainWindow && gpMainWindow->m_pInstance) ?
      gpMainWindow->m_pInstance->InstanceFileGet (pVM, plParams->Get(0)) :
      NULL;
   if (!pif) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   BOOL fAll = plParams->Get(2)->GetBOOL(pVM);
   BOOL fChildren = plParams->Get(3)->GetBOOL(pVM);
   PCMIFLVarString ps = plParams->Get(1)->GetString(pVM);
   PCMIFLVarList pl = plParams->Get(4)->GetList();

   // figure out what objects on list
   DWORD i;
   CListFixed lExclude;
   lExclude.Init (sizeof(GUID));
   if (pl) for (i = 0; i < pl->Num(); i++) {
      PCMIFLVar pv = pl->Get(i);
      if (!pv || (pv->TypeGet() != MV_OBJECT))
         continue;

      GUID gID = pv->GetGUID();
      lExclude.Add (&gID);
   } // i
   else {
      pRet->SetBOOL (FALSE);
      goto done;
   }

   // try to save
   BOOL fRet = pif->Save (pVM, ps->Get(), fAll, fChildren, (GUID*)lExclude.Get(0), lExclude.Num());
      // BUGFIX - Was passing in FALSE for globals and del, but changed to fAll so
      // that if save all will load in deleted

   pRet->SetBOOL (fRet);

done:
   ps->Release();
   if (pl)
      pl->Release();
   return TRUE;
}



/*************************************************************************************
CMIFLSocketServer::SavedGameLoad - Loads in saved game info

inputs
   string - name of the game
   bool - If TRUE then remap objects, else replace
returns
   if remap then returns a list of all the remaps. Each remap is a sublist
   with the first being the original GUID as it was saved, and the second
   being the new GUID. Else returns TRUE if succeded,
   FALSE if faield
*/
BOOL CMIFLSocketServer::SavedGameLoad (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 3)
      return FALSE;

   PCInstanceFile pif = (gpMainWindow && gpMainWindow->m_pInstance) ?
      gpMainWindow->m_pInstance->InstanceFileGet (pVM, plParams->Get(0), FALSE) :
      NULL;
   if (!pif) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   BOOL fRemap = plParams->Get(2)->GetBOOL(pVM);
   PCMIFLVarString ps = plParams->Get(1)->GetString(pVM);
   PCMIFLVarList pl = new CMIFLVarList;
   PCHashGUID phRemap = NULL;
   CMIFLVar v;

   // load in
   BOOL fRet = pif->Load (pVM, ps->Get(), fRemap, fRemap ? &phRemap : NULL);
   if (!fRet) {
      if (phRemap)
         delete phRemap;
      pRet->SetBOOL (FALSE);
      goto done;
   }

   // add remap to list
   DWORD i;
   if (phRemap) {
      for (i = 0; i < phRemap->Num(); i++) {
         GUID *pgOrig = phRemap->GetGUID (i);
         GUID *pgNew = (GUID*) phRemap->Get(i);
         PCMIFLVarList plNew = new CMIFLVarList;
         if (!plNew)
            continue;

         v.SetObject (pgOrig);
         plNew->Add (&v, FALSE);
         v.SetObject (pgNew);
         plNew->Add (&v, FALSE);

         // add it
         v.SetList (plNew);
         plNew->Release();
         pl->Add (&v, TRUE);
      } // i

      pRet->SetList (pl);
   } // if remap
   else
      pRet->SetBOOL (TRUE);

done:
   ps->Release();
   if (pl)
      pl->Release();
   if (phRemap)
      delete phRemap;
   return TRUE;
}



/*************************************************************************************
CMIFLSocketServer::HelpGetBooks - Fills in the books list

inputs
   PCMIFLVM       pVM - VM
   PCMIFLVar      pBooks - Books
   PCListVariable plBooks - Filled in with the books
returns
   BOOL - TRUE if success
*/
BOOL CMIFLSocketServer::HelpGetBooks (PCMIFLVM pVM, PCMIFLVar pBooks, PCListVariable plBooks)
{
   plBooks->Clear();

   // produce the books name
   PCMIFLVarString pvs;
   PCMIFLVarList pvl;
   PWSTR psz;
   DWORD i;
   switch (pBooks->TypeGet()) {
   case MV_LIST:
      // get all elements of the list
      pvl = pBooks->GetList();
      for (i = 0; i < pvl->Num(); i++) {
         pvs = pvl->Get(i)->GetString(pVM);
         if (!pvs)
            continue;

         psz = pvs->Get();
         plBooks->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));

         pvs->Release();
      } // i
      pvl->Release();
      break;
   case MV_STRING:
   case MV_STRINGTABLE:
      // get the string
      pvs = pBooks->GetString (pVM);
      psz = pvs->Get();
      plBooks->Add (psz, (wcslen(psz)+1)*sizeof(WCHAR));
      pvs->Release();
      break;
   }
   // if no entries then at least one
   if (!plBooks->Num())
      plBooks->Add (L"", sizeof(WCHAR));

   return TRUE;
}



/*************************************************************************************
CMIFLSocketServer::HelpArticle - Returns the <Help>...</Help> resource for an article.

inputs
   #1) Actor referencing help
   #2) string with the article's name, requiring an exact (case insensative) match
   #3) List of acceptable books, a string for the acceptable book, or NULL to use an "" for an acceptable book
returns
   NULL if error.
   
   Otherwise, list: 1st item is MML text for the article. 2nd is primary directory.
   3rd is secondary directory.
*/
BOOL CMIFLSocketServer::HelpArticle (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 3)
      return FALSE;

   // fill in the books
   CListVariable lBooks;
   HelpGetBooks (pVM, plParams->Get(2), &lBooks);

   PCMIFLVar pArt = plParams->Get(1);
   PCMIFLVarString pvs = pArt->GetString(pVM);
   PCResHelp ph = NULL;
   PWSTR psz = gpMainWindow ? gpMainWindow->HelpGetArticle (pvs->Get(), &lBooks, &ph,
      plParams->Get(0), pVM) : NULL;
   pvs->Release();

   if (!psz) {
      pRet->SetNULL ();
      return TRUE;
   }

   // else, make list
   PCMIFLVarList pvl = new CMIFLVarList;

   // first is string
   pRet->SetString (psz);
   pvl->Add (pRet, TRUE);

   // second is dir
   if (ph) {
      pRet->SetString ((PWSTR) ph->m_aMemHelp[0].p);
      pvl->Add (pRet, TRUE);
   }

   // third is 2ndary dir
   if (ph && ((PWSTR) ph->m_aMemHelp[1].p)[0]) {
      pRet->SetString ((PWSTR) ph->m_aMemHelp[1].p);
      pvl->Add (pRet, TRUE);
   }

   pRet->SetList (pvl);
   pvl->Release();

   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::HelpContents - Returns a list of contents at the given level.

inputs
   #1) Actor referencing help
   #2) contents string, such as "major category/minor category/sub category"
   #3) List of acceptable books, a string for the acceptable book, or NULL to use an "" for an acceptable book
returns
   NULL if error. Other wise a list.

   THe first element of the list contains a list of all articles at this level
   of contents. Each article is represented by a list with the article name (string) followed
   by a short description. (string)

   The second element of the list is a list of all sub-categories at this level.
   Each sub-category is a string.
*/
BOOL CMIFLSocketServer::HelpContents (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 3)
      return FALSE;

   // fill in the books
   CListVariable lBooks;
   HelpGetBooks (pVM, plParams->Get(2), &lBooks);

   PCMIFLVar pArt = plParams->Get(1);
   PCMIFLVarString pvs = pArt->GetString(pVM);
   CListFixed lPCResHelp;
   CListVariable lSubDir;
   if (gpMainWindow)
      gpMainWindow->HelpContents (pvs->Get(), &lBooks, &lPCResHelp, &lSubDir,
         plParams->Get(0), pVM);
   pvs->Release();

   if (!lPCResHelp.Num() && !lSubDir.Num()) {
      pRet->SetNULL();
      return TRUE;
   }

   // else, create the list
   PCMIFLVarList pvlMain = new CMIFLVarList;

   // articles list
   PCMIFLVarList pvlArt = new CMIFLVarList;
   PCResHelp *pph = (PCResHelp*) lPCResHelp.Get(0);
   DWORD i;
   for (i = 0; i < lPCResHelp.Num(); i++) {
      PCResHelp ph = pph[i];

      PCMIFLVarList pvlElem = new CMIFLVarList;

      // name
      pRet->SetString ((PWSTR) ph->m_memName.p);
      pvlElem->Add (pRet, TRUE);

      // short description
      pRet->SetString ((PWSTR) ph->m_memDescShort.p);
      pvlElem->Add (pRet, TRUE);

      // add to main list
      pRet->SetList (pvlElem);
      pvlElem->Release();
      pvlArt->Add (pRet, TRUE);
   } // i

   // subdir lit
   PCMIFLVarList pvlDir = new CMIFLVarList;
   for (i = 0; i < lSubDir.Num(); i++) {
      pRet->SetString ((PWSTR)lSubDir.Get(i));
      pvlDir->Add (pRet, TRUE);
   } // i

   // add
   pRet->SetList (pvlArt);
   pvlArt->Release();
   pvlMain->Add (pRet, TRUE);

   pRet->SetList (pvlDir);
   pvlDir->Release();
   pvlMain->Add (pRet, TRUE);

   // done
   pRet->SetList (pvlMain);
   pvlMain->Release();
   return TRUE;
}



/*************************************************************************************
CMIFLSocketServer::HelpSearch - Searches for all the keywords in the string.

inputs
   #1) Actor referencing help
   #2) keyword string, such as "new york city"
   #3) List of acceptable books, a string for the acceptable book, or NULL to use an "" for an acceptable book
returns
   NULL if error. Other wise a list of search results.

   Each search result is a sub-list, with the first element being the article
   name, followed by a short description, followed by an integer search score.
*/
BOOL CMIFLSocketServer::HelpSearch (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 3)
      return FALSE;

   // fill in the books
   CListVariable lBooks;
   HelpGetBooks (pVM, plParams->Get(2), &lBooks);

   PCMIFLVar pArt = plParams->Get(1);
   PCMIFLVarString pvs = pArt->GetString(pVM);
   CListFixed lPCResHelp, lScore;
   if (gpMainWindow)
      gpMainWindow->HelpSearch (pvs->Get(), &lBooks, &lPCResHelp, &lScore,
         plParams->Get(0), pVM);
   pvs->Release();

   if (!lPCResHelp.Num()) {
      pRet->SetNULL();
      return TRUE;
   }

   // else, create the list
   PCMIFLVarList pvlMain = new CMIFLVarList;

   // articles list
   PCResHelp *pph = (PCResHelp*) lPCResHelp.Get(0);
   DWORD *padw = (DWORD*) lScore.Get(0);
   DWORD i;
   for (i = 0; i < lPCResHelp.Num(); i++) {
      PCResHelp ph = pph[i];

      PCMIFLVarList pvlElem = new CMIFLVarList;

      // name
      pRet->SetString ((PWSTR) ph->m_memName.p);
      pvlElem->Add (pRet, TRUE);

      // short description
      pRet->SetString ((PWSTR) ph->m_memDescShort.p);
      pvlElem->Add (pRet, TRUE);

      // score
      pRet->SetDouble (padw[i]);
      pvlElem->Add (pRet, TRUE);

      // add to main list
      pRet->SetList (pvlElem);
      pvlElem->Release();
      pvlMain->Add (pRet, TRUE);
   } // i


   // done
   pRet->SetList (pvlMain);
   pvlMain->Release();
   return TRUE;
}

/*************************************************************************************
CMIFLSocketServer::BinaryDataSave - Saves the binary data to the database.

inputs
   #1) File name
   #2) Binary data as a string with 1+ added to all the data
returns
   TRUE if success, FALSE if error
*/
BOOL CMIFLSocketServer::BinaryDataSave (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 2)
      return FALSE;
   PCMegaFile pmf = gpMainWindow->BinaryDataMega ();
   if (!pmf) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   PCMIFLVarString pvsName = plParams->Get(0)->GetString (pVM);
   PCMIFLVarString pvsData = plParams->Get(1)->GetString (pVM);

   // convert data to binary
   PWSTR psz = pvsData->Get();
   DWORD dwLen = (DWORD)wcslen(psz);
   CMem mem;
   if (!mem.Required (dwLen)) {
      pvsName->Release();
      pvsData->Release();
      pRet->SetBOOL (FALSE);
      return TRUE;
   }
   PBYTE pb = (PBYTE)mem.p;
   DWORD i;
   for (i = 0; i < dwLen; i++, pb++, psz++)
      *pb = (BYTE)(*psz-1);

   BOOL fRet = pmf->Save (pvsName->Get(), mem.p, dwLen);

   // in the end relelase all
   pvsName->Release();
   pvsData->Release();

   pRet->SetBOOL (fRet);
   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::BinaryDataRemove - Deletes the file

inputs
   #1) File name
returns
   TRUE if success, FALSE if error
*/
BOOL CMIFLSocketServer::BinaryDataRemove (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 1)
      return FALSE;
   PCMegaFile pmf = gpMainWindow->BinaryDataMega ();
   if (!pmf) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   PCMIFLVarString pvsName = plParams->Get(0)->GetString (pVM);

   BOOL fRet = pmf->Delete (pvsName->Get());

   // in the end relelase all
   pvsName->Release();

   pRet->SetBOOL (fRet);
   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::BinaryDataRename - Renames the file

inputs
   #1) File name (original)
   #2) File name (new)
returns
   TRUE if success, FALSE if error
*/
BOOL CMIFLSocketServer::BinaryDataRename (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 2)
      return FALSE;
   PCMegaFile pmf = gpMainWindow->BinaryDataMega ();
   if (!pmf) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   PCMIFLVarString pvsName = plParams->Get(0)->GetString (pVM);
   PCMIFLVarString pvsName2 = plParams->Get(1)->GetString (pVM);

   // make sure original file exists
   MFFILEINFO info;
   BOOL fRet = pmf->Exists (pvsName->Get(), &info);
   if (!fRet)
      goto done;

   // make sure new file does not exist
   fRet = !pmf->Exists (pvsName2->Get());
   if (!fRet)
      goto done;

   // since no rename, copy
   __int64 iSize;
   PVOID pLoad;
   pLoad = pmf->Load (pvsName->Get(), &iSize);
   if (!pLoad)
      goto done;

   // save
   fRet = pmf->Save (pvsName2->Get(), pLoad, iSize, &info.iTimeCreate, &info.iTimeModify, &info.iTimeAccess);
   MegaFileFree (pLoad);
   if (!fRet)
      goto done;

   // delete old one
   pmf->Delete (pvsName->Get());
   fRet = TRUE;

done:
   // in the end relelase all
   pvsName->Release();
   pvsName2->Release();

   pRet->SetBOOL (fRet);
   return TRUE;
}


/*************************************************************************************
CMIFLSocketServer::BinaryDataEnum - Returns a list of files beginning with the given prefix

inputs
   #1) File name prefix. If NULL returns all files
returns
   List of file names
*/
BOOL CMIFLSocketServer::BinaryDataEnum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 1)
      return FALSE;
   PCMegaFile pmf = gpMainWindow->BinaryDataMega ();
   if (!pmf) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   PCMIFLVar pv = plParams->Get(0);
   PCMIFLVarString pvsName = ((pv->TypeGet() == MV_STRING) || (pv->TypeGet() == MV_STRINGTABLE)) ?
      pv->GetString (pVM) : NULL;

   CListVariable lv;
   BOOL fRet = pmf->Enum (&lv, NULL, pvsName ? pvsName->Get() : NULL);
   if (pvsName)
      pvsName->Release();
   if (!fRet) {
      pRet->SetNULL ();
      return TRUE;
   }

   PCMIFLVarList pvl = new CMIFLVarList;
   if (!pvl) {
      pRet->SetNULL ();
      return TRUE;
   }

   DWORD i;
   for (i = 0; i < lv.Num(); i++) {
      PWSTR psz = (PWSTR) lv.Get(i);

      pRet->SetString (psz);
      pvl->Add (pRet, TRUE);
   } // i

   pRet->SetList (pvl);
   pvl->Release();
   return TRUE;
}



/*************************************************************************************
CMIFLSocketServer::BinaryDataNum - Returns the number of files in the database

inputs
returns
   Number
*/
BOOL CMIFLSocketServer::BinaryDataNum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 0)
      return FALSE;
   PCMegaFile pmf = gpMainWindow->BinaryDataMega ();
   if (!pmf) {
      pRet->SetBOOL (FALSE);
      return TRUE;
   }

   pRet->SetDouble (pmf->Num());
   return TRUE;
}



/*************************************************************************************
CMIFLSocketServer::BinaryDataGetNum - Returns the file name given the index

inputs
   #1) Index, 0.. BinaryDataNum()-1
returns
   String with file name, or NULL if cant get index
*/
BOOL CMIFLSocketServer::BinaryDataGetNum (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 1)
      return FALSE;
   PCMegaFile pmf = gpMainWindow->BinaryDataMega ();
   if (!pmf) {
      pRet->SetNULL();
      return TRUE;
   }

   DWORD dwIndex = (DWORD) plParams->Get(0)->GetDouble (pVM);
   CMem mem;
   BOOL fRet = pmf->GetNum (dwIndex, &mem);
   if (!fRet) {
      pRet->SetNULL();
      return TRUE;
   }

   pRet->SetString ((PWSTR)mem.p);

   return TRUE;
}

/*************************************************************************************
CMIFLSocketServer::BinaryDataLoad - Loads a file from the database

inputs
   #1) File name
returns
   String with 1+ the values, or NULL if error
*/
BOOL CMIFLSocketServer::BinaryDataLoad (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 1)
      return FALSE;
   PCMegaFile pmf = gpMainWindow->BinaryDataMega ();
   if (!pmf) {
      pRet->SetNULL();
      return TRUE;
   }

   PCMIFLVarString pvsName = plParams->Get(0)->GetString (pVM);

   // load in
   __int64 iSize;
   PVOID pLoad;
   pLoad = pmf->Load (pvsName->Get(), &iSize);
   pvsName->Release();
   if (!pLoad) {
      pRet->SetNULL();
      return TRUE;
   }

   // create string for this
   CMem mem;
   DWORD dwSize = iSize;
   if ( ((__int64)dwSize != iSize) || !mem.Required((dwSize+1)*sizeof(WCHAR)) ) {
      pRet->SetNULL();
      MegaFileFree (pLoad);
      return TRUE;
   }

   DWORD i;
   PWSTR psz = (PWSTR) mem.p;
   PBYTE pb = (PBYTE) pLoad;
   for (i = 0; i < dwSize; i++, psz++, pb++)
      *psz = (WCHAR)*pb + 1;
   *psz = 0;   // null terminate
   MegaFileFree (pLoad);

   pRet->SetString ((PWSTR)mem.p);
   return TRUE;
}

/*************************************************************************************
CMIFLSocketServer::BinaryDataQuery - Queries information from the database

inputs
   #1) File name
returns
   If no file then NULL. If find one then list with [Size in bytes, create, last write, last access]
*/
BOOL CMIFLSocketServer::BinaryDataQuery (PCMIFLVM pVM, PCMIFLVarList plParams, PCMIFLVar pRet)
{
   // no parameters
   if (plParams->Num() != 1)
      return FALSE;
   PCMegaFile pmf = gpMainWindow->BinaryDataMega ();
   if (!pmf) {
      pRet->SetNULL ();
      return TRUE;
   }

   PCMIFLVarString pvsName = plParams->Get(0)->GetString (pVM);

   // make sure original file exists
   MFFILEINFO info;
   BOOL fRet = pmf->Exists (pvsName->Get(), &info);
   pvsName->Release();
   if (!fRet) {
      pRet->SetNULL();
      return TRUE;
   }

   PCMIFLVarList pvl = new CMIFLVarList;
   if (!pvl) {
      pRet->SetNULL();
      return TRUE;
   }

   // first element is the size
   pRet->SetDouble (info.iDataSize);
   pvl->Add (pRet, TRUE);

   // second is time create, then modify, then access
   pRet->SetDouble (MIFLFileTimeToDouble (&info.iTimeCreate));
   pvl->Add (pRet, TRUE);
   pRet->SetDouble (MIFLFileTimeToDouble (&info.iTimeModify));
   pvl->Add (pRet, TRUE);
   pRet->SetDouble (MIFLFileTimeToDouble (&info.iTimeAccess));
   pvl->Add (pRet, TRUE);

   pRet->SetList (pvl);
   pvl->Release();
   return TRUE;
}



// BUGBUG - want to send the LANGID to the thread creation UI so that can have
// different languages saved in each thread. (So can keep the commands all in
// the same language)

// BUGBUG - what to do if an object that's defined in the MIFL code gets picked
// up by user and then cached to database. World restarts... is object deleted, or
// is the object respawned and allowed a second existence by the user?

// BUGBUG - at one point leaked a lot of something that was 40 bytes long. Not sure what
// => need better memory tracking, as listed above
