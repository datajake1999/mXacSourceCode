/*************************************************************************
DeepThought.cpp - handle the deep thoughts section.

begun 8/27/2000 by Mike Rozak
Copyright 2000 by Mike Rozak. All rights reserved
*/

#include <windows.h>
#include "escarpment.h"
#include "dragonfly.h"
#include "resource.h"


static PWSTR gaszShort[TOPN] = {L"short1", L"short2", L"short3", L"short4", L"short5"};
static PWSTR gaszLong[TOPN] = {L"long1", L"long2", L"long3", L"long4", L"long5"};
static PWSTR gaszChange[TOPN] = {L"change1", L"change2", L"change3", L"change4", L"change5"};
static PWSTR gaszWorld[TOPN] = {L"world1", L"world2", L"world3", L"world4", L"world5"};
static PWSTR gaszDTPerson[TOPN] = {L"person1", L"person2", L"person3", L"person4", L"person5"};
static PWSTR gaszGoal[TOPN] = {L"goal1", L"goal2", L"goal3", L"goal4", L"goal5"};

static BOOL gfQuoteRead = FALSE;    // set to TRUE if the custom quotes have been read in
static CListVariable gListQuote;    // list of quote strings
static CListVariable gListQuoteAuthor; // list of authors for the quote strings

/*************************************************************************
QuotesGet - Reads in all the quotes from disk, into gListQuote and gListQuoteAuthor.
If they've already been read in once then it doesn't bother again.
*/
void QuotesGet (void)
{
   HANGFUNCIN;
   if (gfQuoteRead)
      return;
   gfQuoteRead = TRUE;

   PCMMLNode pNode;
   pNode = FindMajorSection (gszDeepThoughtsNode);
   if (!pNode)
      return;

   // get the number
   DWORD dwNum;
   dwNum = NodeValueGetInt (pNode, L"NumQuotes", 0);

   DWORD i;
   for (i = 0; i < dwNum; i++) {
      PWSTR pszQuote, pszAuthor;
      WCHAR szTemp[64];
      swprintf (szTemp, L"QuoteString%d", (int) i);   // BUGFIX - So can read in quotes properly
      pszQuote = NodeValueGet (pNode, szTemp);
      swprintf (szTemp, L"QuoteAuthor%d", (int) i);
      pszAuthor = NodeValueGet (pNode, szTemp);
      if (!pszQuote || !pszQuote[0] || !pszAuthor)
         continue;

      // add it
      gListQuote.Add (pszQuote, (wcslen(pszQuote)+1)*2);
      gListQuoteAuthor.Add (pszAuthor, (wcslen(pszAuthor)+1)*2);
   }

   gpData->NodeRelease(pNode);
   gpData->Flush();
}


/*************************************************************************
QuotesSet - Writes all the quotes (in gListQuote, and gListQuoteAuthor)
to disk
*/
void QuotesSet (void)
{
   HANGFUNCIN;
   PCMMLNode pNode;
   pNode = FindMajorSection (gszDeepThoughtsNode);
   if (!pNode)
      return;

   // set the number
   DWORD dwNum;
   dwNum = gListQuote.Num();
   NodeValueSet (pNode, L"NumQuotes", dwNum);

   // write out
   DWORD i;
   for (i = 0; i < dwNum; i++) {
      WCHAR szTemp[64];
      swprintf (szTemp, L"QuoteString%d", (int) i);   // BUGFIX - So can read in quotes properly
      NodeValueSet (pNode, szTemp, (PWSTR) gListQuote.Get(i));
      swprintf (szTemp, L"QuoteAuthor%d", (int) i);   // BUGFIX - So can read in quotes properly
      NodeValueSet (pNode, szTemp, (PWSTR) gListQuoteAuthor.Get(i));
   }

   gpData->NodeRelease(pNode);
   gpData->Flush();
}

/*************************************************************************
DeepFillStruct - Fills in a DEEPTHOUGHTS structure with the current
values for the top-N lists. This also returns a PCMMLNode to the
Deep thoughts node WHICH MUST BE RELEASED by the caller.

inputs
   DEEPTHOUGHTS *pdt - filled in. The strings are valid until PCMMLNode is
   released.

returns
   PCMMLNode - For deep thoughts node. NULL if error
*/
PCMMLNode DeepFillStruct (DEEPTHOUGHTS *pdt)
{
   HANGFUNCIN;
   PCMMLNode pNode;
   pNode = FindMajorSection (gszDeepThoughtsNode);
   if (!pNode)
      return NULL;

   // fill the structure
   if (pdt) {
      DWORD i;
      PWSTR psz;

      memset (pdt, 0, sizeof(*pdt));
      for (i = 0; i < TOPN; i++)
         pdt->adwPerson[i] = (DWORD) -1;

      for (i = 0; i < TOPN; i++) {
         psz = NodeValueGet(pNode, gaszShort[i]);
         if (psz && psz[0])
            pdt->apszShort[i] = psz;

         psz = NodeValueGet(pNode, gaszLong[i]);
         if (psz && psz[0])
            pdt->apszLong[i] = psz;

         psz = NodeValueGet(pNode, gaszChange[i]);
         if (psz && psz[0])
            pdt->apszChange[i] = psz;

         psz = NodeValueGet(pNode, gaszWorld[i]);
         if (psz && psz[0])
            pdt->apszWorld[i] = psz;

         DWORD dwNode;
         dwNode = (DWORD) NodeValueGetInt (pNode, gaszDTPerson[i], -1);
         psz = PeopleIndexToName(PeopleDatabaseToIndex(dwNode));
         if (psz && psz[0]) {
            pdt->adwPerson[i] = dwNode;
            pdt->apszPerson[i] = psz;
         }

      }
   }


   return pNode;
}

/*************************************************************************
DeepFromControls - Read 5 edit controls (goalX) in the page and write
out their values to the deep thoughts node.

inputs
   PCEscPage      pPage - Page to find the "goalX" controls in
   PWSTR          *papsz - Should be gaszShort, gaszLong, gaszChange, or gaszWorld
returns
   none
*/
void DeepFromControls (PCEscPage pPage, PWSTR *papsz)
{
   HANGFUNCIN;
   PCMMLNode   pNode;
   pNode = FindMajorSection (gszDeepThoughtsNode);
   if (!pNode)
      return;

   // get and write
   WCHAR szTemp[256];
   DWORD i, dwNeeded;
   PCEscControl pControl;
   for (i = 0; i < TOPN; i++) {
      szTemp[0] = 0;
      pControl = pPage->ControlFind (gaszGoal[i]);
      if (!pControl)
         continue;
      pControl->AttribGet (gszText, szTemp, sizeof(szTemp), &dwNeeded);

      // write out
      NodeValueSet (pNode, papsz[i], szTemp);
   }
   gpData->NodeRelease(pNode);
   gpData->Flush();
}


/**********************************************************************
DeepToControls - Fills in the goalx edit controls.
inputs
   PCEscPage      pPage - Page to find the "goalX" controls in
   PWSTR          *papsz - Should be gaszShort, gaszLong, gaszChange, or gaszWorld
returns
   none
*/
void DeepToControls (PCEscPage pPage, PWSTR *papsz)
{
   HANGFUNCIN;
   PCMMLNode   pNode;
   pNode = FindMajorSection (gszDeepThoughtsNode);
   if (!pNode)
      return;

   // get and write
   DWORD i;
   PCEscControl pControl;
   PWSTR psz;
   for (i = 0; i < TOPN; i++) {
      psz = NodeValueGet (pNode, papsz[i]);
      pControl = pPage->ControlFind (gaszGoal[i]);
      if (pControl && psz)
         pControl->AttribSet (gszText, psz);
   }
   gpData->NodeRelease(pNode);
}



/***********************************************************************
DeepShortPage - Page callback for new user (not quick-add though)
*/
BOOL DeepShortPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      DeepToControls (pPage, gaszShort);
      break;

   case ESCM_LINK:
      // save away just in case
      DeepFromControls (pPage, gaszShort);
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}



/***********************************************************************
DeepLongPage - Page callback for new user (not quick-add though)
*/
BOOL DeepLongPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      DeepToControls (pPage, gaszLong);
      break;

   case ESCM_LINK:
      // save away just in case
      DeepFromControls (pPage, gaszLong);
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
DeepChangePage - Page callback for new user (not quick-add though)
*/
BOOL DeepChangePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      DeepToControls (pPage, gaszChange);
      break;

   case ESCM_LINK:
      // save away just in case
      DeepFromControls (pPage, gaszChange);
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}


/***********************************************************************
DeepWorldPage - Page callback for new user (not quick-add though)
*/
BOOL DeepWorldPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      DeepToControls (pPage, gaszWorld);
      break;

   case ESCM_LINK:
      // save away just in case
      DeepFromControls (pPage, gaszWorld);
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}

/***********************************************************************
DeepPeoplePage - Page callback for new user (not quick-add though)
*/
BOOL DeepPeoplePage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         // fill in the list of people
         DEEPTHOUGHTS dt;
         PCMMLNode pNode;
         pNode = DeepFillStruct (&dt);
         if (!pNode)
            break;
         gpData->NodeRelease(pNode);

         DWORD i;
         PCEscControl pControl;
         for (i = 0; i < TOPN; i++) {
            pControl = pPage->ControlFind (gaszDTPerson[i]);
            if (!pControl)
               continue;
            if (dt.adwPerson[i] != (DWORD)-1)
               pControl->AttribSetInt (gszCurSel, PeopleDatabaseToIndex(dt.adwPerson[i]));
         }
      }
      break;

   case ESCM_LINK:
      {
         // save away just in case

         PCMMLNode pNode;
         pNode = FindMajorSection (gszDeepThoughtsNode);
         if (!pNode)
            break;

         DWORD i;
         PCEscControl pControl;
         for (i = 0; i < TOPN; i++) {
            pControl = pPage->ControlFind (gaszDTPerson[i]);
            if (!pControl)
               continue;
            NodeValueSet (pNode, gaszDTPerson[i], (int)
               PeopleIndexToDatabase(pControl->AttribGetInt(gszCurSel)));
         }
         gpData->NodeRelease(pNode);
         gpData->Flush();
      }
      break;   // default behavior

   };


   return DefPage (pPage, dwMessage, pParam);
}

// BUGBUG - 2.0 - Allow people to specify their own deep thoughts

/**************************************************************************
DeepThoughtGenerate - Generates a randomly deep thought and returns
   a pointer to it. The pointer is gMemTemp.p

reutnrs
   PWSTR - string
*/
char gszMikeRozak[] = "Mike Rozak";
char gszAnonymous[] = "Anonymous";
char gszFrankHerbert[]= "Frank Herbert";
char gszAesop[] = "Aesop";
char gszWilliamShakespeare[] = "William Shakespeare";
char gszHawking[] = "Stephen Hawking";
char gszEinstein[]= "Albert Einstein";
char gszMarkTwain[] = "Mark Twain";
char gszCharlesDickens[] = "Charles Dickens";
char gszWoodyAllen[] = "Woody Allen";
char gszVoltaire[] = "Voltaire";
char gszAristotle[] = "Aristotle";
char gszThoreau[] = "Henry David Thoreau";
char gszAmericanIndianProverb[] = "American Indian Proverb";
char gszIndianProverb[] = "Indian Proverb";
char gszJapaneseProverb[] = "Japanese Proverb";
char gszSlovenianProverb[] = "Slovenian Proverb";
char gszChineseProverb[] = "Chinese Proverb";
char gszGermanProverb[] = "German Proverb";
char gszNigerianProverb[] = "Nigerian Proverb";
char gszFrenchProverb[] = "French Proverb";
char gszItalianProverb[] = "Italian Proverb";
char gszMuslimProverb[] = "Muslim Proverb";
char gszPortugueseProverb[] = "Portuguese Proverb";
char gszYiddishProverb[] = "Yiddish Proverb";
char gszDanishProverb[] = "Danish Proverb";
char gszRussianProverb[] = "Russian Proverb";
char gszAfricanProverb[] = "African Proverb";
char gszEnglishProverb[] = "English Proverb";
char gszAssyrianProverb[] = "Assyrian Proverb";
char gszYugoslavProverb[] = "Yugoslav Proverb";
char gszJefferson[] = "Thomas Jefferson";
char gszGandhi[] = "Mahatma Gandhi";
char gszLincoln[] = "Abraham Lincoln";
char gszEmerson[] = "Ralph Waldo Emerson";
char gszSocrates[] = "Socrates";
char gszFitzgerald[] = "F. Scott Fitzgerald";
char gszFuller[] = "Buckminster Fuller";
char gszFeynman[] = "Richard Fenyman";
char gszRabbinicalSaying[] = "Rabbinical Saying";
char gszPlato[] = "Plato";

PSTR gpaszDeepThought[] = {
   "The future is an imaginary version of the past.", gszMikeRozak,
   "How much of your time is dictated by you? How much by others?.", gszMikeRozak,
   "How many decisions are made for you?", gszMikeRozak,
   "The large decisions like choosing a college or whom to marry "
      "have the greatest impact on your future, but the hoards of small decisions, such "
      "as whether you will study or what you will wear today, affect your menu of large decisions.", gszMikeRozak,
   "Work is the prostitution of life. Spend your money wisely.", gszMikeRozak,
   "Teenagers are the lowest form of human life. Luckily they grow out of it.", gszMikeRozak,
   "The effectiveness of a meeting is inversely proportional to the square of the number of participants. "
      "Invite as few people to a meeting as possible.", gszMikeRozak,
   "It seems to me that some people's sole purpose in life is to make it harder for the rest of us.", gszMikeRozak,
   "Do work to avoid work.", gszMikeRozak,
   "Does your waking life consist solely of working and recouperating from work?", gszMikeRozak,
   "My pets have a better life than do I.", gszMikeRozak,
   "Spend some time every night considering what work you should do tomorrow; "
      "Spend even more figuring out what isn't worth doing.", gszMikeRozak,
   "Efficiency is not completing a set of tasks quicker, "
      "but knowing which tasks don't need to be started in the first place.", gszMikeRozak,
   "Some people's dogs are more intelligent than they are.", gszMikeRozak,
   "When I was a child I wondered why adults never played. Now I know; "
      "They're too exhausted from work.", gszMikeRozak,
   "Have you ever had an arguement with your toaster? You will...", gszMikeRozak,
   "Cars, jewelry, electronics, boats, and houses are merely toys targeted at adults.", gszMikeRozak,
   "Your possesions own you more than you own them.", gszMikeRozak,
   "The great pyramids probably bankrupted ancient Egypt.", gszMikeRozak,
   "When I was a child I saw things in black or white, good or bad, right or wrong. "
      "As I gew older I perceived shades of gray. "
      "The constrast gradually disappeared, so when I became a teenager it all looked the same. "
      "Now I see in color; Something is what it is. "
      "Attributing goodness or badness all depends upon your critera.", gszMikeRozak,
   "My world consists of a few buildings and the roads that connect them. "
      "How is this different than a cage?", gszMikeRozak,
   "Tourism is paying lots of money and travelling long distances to see something you've already seen in a photograph.", gszMikeRozak,
   "It's easy to lose sight of our goals in the turbulance of petty worries.", gszMikeRozak,
   "You will never have everything you want. So why try?", gszMikeRozak,
   "As a general rule, if you ask someone how much money they'd like to earn they "
      "give you a figure twice their current income. By induction, one can "
      "assume that no amount of money is ever enough. So why stive for more?", gszMikeRozak,
   "A person's most treasured posession is the only one he owns; "
      "The value of a posession is inversely proportional to the number of posessions owned.", gszMikeRozak,
   "Life is too short to worry about dying.", gszMikeRozak,
   "Experts break their own rules.", gszMikeRozak,
   "Individual life is very fragile. Expect some dings, chips, and cracks, and even the inevitable shattering. "
      "Something so beautiful is always delicate.", gszMikeRozak,
   "Most life is wasted waiting for life to happen.", gszMikeRozak,
   "The individual defeats the dictator.", gszMikeRozak,
   "Money often controls those that own it.", gszMikeRozak,
   "Ants look much larger when you are one.", gszMikeRozak,
   "I once heard someone say, \"I don't sit and think because it's scary.\"", gszMikeRozak,
   "Capitalism decays into aristocracy.", gszMikeRozak,
   "The act of remembering is a process of forgetting.", gszMikeRozak,
   "The small details of life obscure the larger and more important ones.", gszMikeRozak,
   "We are bombarded with so much information that we don't have time to determine which information is actually useful.", gszMikeRozak,
   "I have been trapped by posessions. Every object requires work/money to buy, "
      "effort to clean and maintain, and causes worry that it might be stolen/broken.", gszMikeRozak,
   "I am the eye at the center of the universe peering out upon myself.", gszMikeRozak,
   "Someone is always willing to tell you what to do.", gszMikeRozak,
   "Many people go their whole lives without making a decision for themselves.", gszMikeRozak,
   "The goal of the software industry should not be to make the computers more intelligent, "
      "but to make the people using the computers more intelligent.", gszMikeRozak,
   "Who is the master and who is the servant, the dog or the human?", gszMikeRozak,
   "Because of the one-pointed time awareness in which the conventional mind remains immersed, "
      "humans tend to think of everything in a sequential, word oriented framework. "
      "This mental trap produces very short-term concepts of effectiveness and consequences "
      "a condition of constant, unplanned response to crisis.", gszFrankHerbert,
   "Decide what is important and own it. Let chaos rule the rest.", gszMikeRozak,
   "The way to change the future is to use temporal and positional distortions to "
      "make yourself look larger than you really are.", gszMikeRozak,
   "People like to collect stuff; this is little different than some birds' obsession for collecting sparkling objects.", gszMikeRozak,
   "Sometimes my most productive days are the ones that I do nothing but plan.", gszMikeRozak,
   "Better be wise by the misfortunes of others than by your own.", gszAesop,
   "We would often be sorry if our wishes were gratified.", gszAesop,
   "Go to your bosom; Knock there, and ask your heart what it doth know. ", gszWilliamShakespeare,
   "The better part of valor is discretion, in the which better part I have saved my life. ", gszWilliamShakespeare,
   "If all the year were playing holidays; To sport would be as tedious as to work. ", gszWilliamShakespeare,
   "We know what we are, but know not what we may be. ", gszWilliamShakespeare,
   "O that a man might know the end of this day's business ere it come! ", gszWilliamShakespeare,
   "There are more things in heaven and earth, Horatio, than are dreamt of in your philosophy. ", gszWilliamShakespeare,
   "Simply the thing that I am shall make me live. ", gszWilliamShakespeare,
   "Life is a tale told by an idiot -- full of sound and fury, signifying nothing. ", gszWilliamShakespeare,
   "It is not in the stars to hold our destiny but in ourselves. ", gszWilliamShakespeare,
   "Strong reasons make strong actions. ", gszWilliamShakespeare,
   "There is a history in all men's lives. ", gszWilliamShakespeare,
   "Some men never seem to grow old. Always active in thought, always ready to adopt new ideas, they are never chargeable with foggyism. Satisfied, yet ever dissatisfied, settled, yet ever unsettled, they always enjoy the best of what is, are the first to find the best of what will be. ", gszWilliamShakespeare,
   "This page intentionally left blank.", "Corporation speak",
   "Equations are just the boring part of mathematics. I attempt to see things in terms of geometry. ", gszHawking,
   "We are just an advanced breed of monkeys on a minor planet of a very average star. But we can understand the Universe. That makes us something very special. ", gszHawking,
   "As far as the laws of mathematics refer to reality, they are not certain; and as far as they are certain, they do not refer to reality. ", gszEinstein,
   "Only two things are infinite, the universe and human stupidity, and I'm not sure about the former. ", gszEinstein,
   "Before God we are all equally wise - and equally foolish. ", gszEinstein,
   "I never think of the future - it comes soon enough. ", gszEinstein,
   "If I had only known, I would have been a locksmith. ", gszEinstein,
   "The hardest thing in the world to understand is the income tax. ", gszEinstein,
   "The most incomprehensible thing about the world is that it is at all comprehensible. ", gszEinstein,
   "If the facts don't fit the theory, change the facts. ", gszEinstein,
   "Imagination is more important than knowledge... ", gszEinstein,
   "The most beautiful thing we can experience is the mysterious. It is the source of all true art and science. ", gszEinstein,
   "The important thing is not to stop questioning. ", gszEinstein,
   "We should take care not to make the intellect our god; it has, of course, powerful muscles, but no personality. ", gszEinstein,
   "Reading, after a certain age, diverts the mind too much from its creative pursuits. Any man who reads too much and uses his own brain too little falls into lazy habits of thinking. ", gszEinstein,
   "Everything should be made as simple as possible, but not one bit simpler. ", gszEinstein,
   "Try not to become a man of success but rather to become a man of value. ", gszEinstein,
   "In the middle of difficulty lies opportunity. ", gszEinstein,
   "When you are courting a nice girl an hour seems like a second. When you sit on a red-hot cinder a second seems like an hour. That's relativity. ", gszEinstein,
   "To punish me for my contempt for authority, fate made me an authority myself. ", gszEinstein,
   "It is, in fact, nothing short of a miracle that the modern methods of instruction have not yet entirely strangled the holy curiosity of inquiry. ", gszEinstein,
   "The problems that exist in the world today cannot be solved by the level of thinking that created them. ", gszEinstein,
   "Problems cannot be solved at the same level of awareness that created them. ", gszEinstein,
   "A human being is part of a whole, called by us the Universe, a part limited in time and "
      "space. He experiences himself, his thoughts and feelings, as something separated "
      "from the rest--a kind of optical delusion of his consciousness. This delusion is a "
      "kind of prison for us, restricting us to our personal desires and to affection for "
      "a few persons nearest us. Our task must be to free ourselves from this prison by "
      "widening our circles of compassion to embrace all living creatures and the whole "
      "of nature in its beauty. ", gszEinstein,
   "Concern for man himself and his fate must always form the chief interest of all technical endeavor. Never forget this in the midst of your diagrams and equations. ", gszEinstein,
   "Truth is more of a stranger than fiction. ", gszMarkTwain,
   "Man is the Only Animal that Blushes. Or needs to. ", gszMarkTwain,
   "The report of my death was an exaggeration. ", gszMarkTwain,
   "Familiarity breeds contempt--and children. ", gszMarkTwain,
   "A banker is a fellow who lends you his umbrella when the sun is shining, but wants it back the minute it begins to rain. ", gszMarkTwain,
   "All you need in this life is ignorance and confidence, and then success is sure. ", gszMarkTwain,
   "Always do right. This will gratify some people and astonish the rest. ", gszMarkTwain,
   "Be careful about reading health books. You may die of a misprint. ", gszMarkTwain,
   "Clothes make the man. Naked people have little or no influence on society. ", gszMarkTwain,
   "Courage is resistance to fear, mastery of fear - not absence of fear. ", gszMarkTwain,
   "Don't go around saying the world owes you a living. The world owes you nothing. It was here first. ", gszMarkTwain,
   "Get your facts first, and then you can distort them as much as you please. ", gszMarkTwain,
   "If you pick up a starving dog and make him prosperous, he will not bite you. This is the principle difference between a dog and a man. ", gszMarkTwain,
   "It is better to keep your mouth closed and let people think you are a fool than to open it and remove all doubt. ", gszMarkTwain,
   "It usually takes more than three weeks to prepare a good impromptu speech. ", gszMarkTwain,
   "Never put off until tomorrow what you can do the day after tomorrow. ", gszMarkTwain,
   "The man who doesn't read good books has no advantage over the man who can't read them. ", gszMarkTwain,
   "Always acknowledge a fault. This will throw those in authority off their guard and give you an opportunity to commit more. ", gszMarkTwain,
   "If you tell the truth you don't have to remember anything. ", gszMarkTwain,
   "I didn't attend the funeral, but I sent a nice letter saying that I approved of it. ", gszMarkTwain,
   "When we remember we are all mad, the mysteries disappear and life stands explained. ", gszMarkTwain,
   "Whenever you find that you are on the side of the majority, it is time to reform. ", gszMarkTwain,
   "It is by the goodness of God that in our country we have those three unspeakably precious things: freedom of speech, freedom of conscience, and the prudence never to practice either. ", gszMarkTwain,
   "Don't part with your illusions. When they are gone you may still exist but you have ceased to live. ", gszMarkTwain,
   "Let us so live that when we come to die even the undertaker will be sorry. ", gszMarkTwain,
   "Keep away from people who try to belittle your ambitions. Small people always do that, but the really great make you feel that you, too, can become great. ", gszMarkTwain,
   "All you need in this life is ignorance and confidence -- and then success is sure. ", gszMarkTwain,
   "I believe I have no prejudices whatsoever. All I need to know is that a man is a member of the human race. That's bad enough for me. ", gszMarkTwain,
   "We are always too busy for our children; we never give them the time or interest they deserve. We lavish gifts upon them; but the most precious gift, our personal association, which means so much to them, we give grudgingly. ", gszMarkTwain,
   "Many a small thing has been made large by the right kind of advertising. ", gszMarkTwain,
   "Train up a fig tree in the way it should go, and when you are old sit under the shade of it. ", gszCharlesDickens,
   "Reflect on your present blessings, of which every man has many; not on your past misfortunes, of which all men have some. ", gszCharlesDickens,
   "It's not that I'm afraid to die, I just don't want to be there when it happens. ", gszWoodyAllen,
   "Why are our days numbered and not, say, lettered? ", gszWoodyAllen,
   "If it turns out that there is a God, I don't think that he's evil. But the worst that you can say about him is that basically he's an underachiever. ", gszWoodyAllen,
   "My one regret in life is that I am not someone else. ", gszWoodyAllen,
   "I don't want to achieve immortality through my work... I want to achieve it through not dying. ", gszWoodyAllen,
   "It is impossible to travel faster than the speed of light, and certainly not desirable, as one's hat keeps blowing off. ", gszWoodyAllen,
   "Students achieving Oneness will move on to Twoness. ", gszWoodyAllen,
   "Interestingly, according to modern astronomers, space is finite. This is a very comforting thought-- particularly for people who can never remember where they have left things. ", gszWoodyAllen,
   "Most of the time I don't have much fun. The rest of the time I don't have any fun at all. ", gszWoodyAllen,
   "A witty saying proves nothing. ", gszVoltaire,
   "If God did not exist, it would be necessary to invent him. ", gszVoltaire,
   "God is a comedian playing to an audience too afraid to laugh. ", gszVoltaire,
   "Work saves us from three great evils: boredom, vice and need. ", gszVoltaire,
   "Every man is guilty of all the good he didn't do. ", gszVoltaire,
   "Man is free at the moment he wishes to be. ", gszVoltaire,
   "Common sense is not so common. ", gszVoltaire,
   "Doubt is not a pleasant condition but certainty is an absurd one. ", gszVoltaire,
   "I believe that there never was a creator of a philosophical system who did not confess "
      "at the end of his life that he had wasted his time. It must be admitted that the "
      "inventors of the mechanical arts have been much more useful to men that the "
      "inventors of syllogisms. He who imagined a ship towers considerably above him "
      "who imagined innate ideas. ", gszVoltaire,
   "We never live; we are always in the expectation of living. ", gszVoltaire,
   "It is the mark of an educated mind to be able to entertain a thought without accepting it. ", gszAristotle,
   "We are what we repeatedly do. ", gszAristotle,
   "We should distrust any enterprise that requires new clothes. ", gszThoreau,
   "What is the use of a house if you haven't got a tolerable planet to put it on? ", gszThoreau,
   "Thank God men cannot as yet fly and lay waste the sky as well as the earth! ", gszThoreau,
   "Men have become the tools of their tools. ", gszThoreau,
   "Any fool can make a rule, and any fool will mind it. ", gszThoreau,
   "That man is the richest whose pleasures are the cheapest. ", gszThoreau,
   "[Water is] the only drink for a wise man. ", gszThoreau,
   "Men are born to succeed, not fail. ", gszThoreau,
   "Our houses are such unwieldy property that we are often imprisoned rather than housed in them. ", gszThoreau,
   "If a man does not keep pace with his companions, perhaps it is because he hears a different drummer. Let him step to the music which he hears, however measured or far away. ", gszThoreau,
   "He enjoys true leisure who has time to improve his soul's estate. ", gszThoreau,
   "What people say you cannot do, you try and find that you can. ", gszThoreau,
   "Live each season as it passes; breathe the air, drink the drink, taste the fruit, and resign yourself to the influences of each. ", gszThoreau,
   "Go Confidently in the direction of your dreams. Live the life you've imagined. ", gszThoreau,
   "Our life is frittered away by detail. Simplify, simplify. ", gszThoreau,
   "Most men would feel insulted if it were proposed to employ them in throwing stones "
      "over a wall, and then in throwing them back, merely that they might earn their "
      "wages. But many are no more worthily employed now. ", gszThoreau,
   "A man is rich in proportion to the number of things he can afford to let alone. ", gszThoreau,
   "The way by which you may get money almost without exception leads downward. ", gszThoreau,
   "A simple and independent mind does not toil at the bidding of any prince. ", gszThoreau,
   "However mean your life is, meet it and live it; do not shun it and call it hard names. It is not so bad as you are. It looks poorest when you are the richest. ", gszThoreau,
   "As for the pyramids, there is nothing to wonder at in them so much as the fact "
      "that so many men could be found degraded enough to spend their lives constructing "
      "a tomb for some ambitious booby, whom it would have been wiser and manlier "
      "to have drowned in the Nile, and then given his body to the dogs. ", gszThoreau,
   "Never criticize a man until you've walked a mile in his moccasins. ", gszAmericanIndianProverb,
   "Keep five yards from a carriage, ten yards from a horse, and a hundred yards from an elephant; but the "
      "distance one should keep from a wicked man cannot be measured. ", gszIndianProverb,
   "Call on God, but row away from the rocks. ", gszIndianProverb,
   "The reverse side also has a reverse side. ", gszJapaneseProverb,
   "If you believe everything you read, better not read. ", gszJapaneseProverb,
   "Speak the truth, but leave immediately after. ", gszSlovenianProverb,
   "The gem cannot be polished without friction, nor man perfected without trials. ", gszChineseProverb,
   "Who begins too much accomplishes little. ", gszGermanProverb,
   "Hold a true friend with both hands. ", gszNigerianProverb,
   "Give a man a fish and you feed him for a day. Teach a man to fish and you feed him for a lifetime. ", gszChineseProverb,
   "He who would leap high must take a long run. ", gszDanishProverb,
   "When you have only two pennies left in the world, buy a loaf of bread with one, and a lily with the other. ", gszChineseProverb,
   "Fall seven times, stand up eight. ", gszJapaneseProverb,
   "Wait until it is night before saying that is has been a fine day. ", gszFrenchProverb,
   "To know the road ahead, ask those coming back. ", gszChineseProverb,
   "Be not afraid of growing slowly, be afraid only of standing still. ", gszChineseProverb,
   "The palest ink is better than the best memory. ", gszChineseProverb,
   "Trust in Allah, but tie your camel. ", gszMuslimProverb,
   "It is not the horse that draws the cart, but the oats. ", gszRussianProverb,
   "He who cannot agree with his enemies is controlled by them. ", gszChineseProverb,
   "Indecision is like a stepchild: if he does not wash his hands, he is called dirty, if he does, he is wasting water. ", gszAfricanProverb,
   "Proportion your expenses to what you have, not what you expect. ", gszEnglishProverb,
   "If we don't change our direction we're likely to end up where we're headed. ", gszChineseProverb,
   "It is better to light one candle than to curse the darkness. ", gszChineseProverb,
   "A good rest is half the work. ", gszYugoslavProverb,
   "Tell me your friends, and I'll tell you who you are. ", gszAssyrianProverb,
   "Between saying and doing many a pair of shoes is worn out. ", gszItalianProverb,
   "An inch of time cannot be bought with an inch of gold. ", gszChineseProverb,
   "Live to live and you will learn to live ", gszPortugueseProverb,
   "A man should live if only to satisfy his curiosity. ", gszYiddishProverb,
   "Determine never to be idle...It is wonderful how much may be done if we are always doing. ", gszJefferson,
   "Delay is preferable to error. ", gszJefferson,
   "I believe in equality for everyone, except reporters and photographers. ", gszGandhi,
   "Whatever you do will be insignificant, but it is very important that you do it. ", gszGandhi,
   "We must become the change we want to see. ", gszGandhi,
   "Always bear in mind that your own resolution to success is more important than any other one thing. ", gszLincoln,
   "Whatever you are, be a good one. ", gszLincoln,
   "The best thing about the future is that it comes one day at a time. ", gszLincoln,
   "I don't know who my grandfather was; I'm much more concerned to know what his grandson will be. ", gszLincoln,
   "My father taught me to work; he did not teach me to love it. ", gszLincoln,
   "You cannot escape the responsibility of tomorrow by evading it today. ", gszLincoln,
   "Things may come to those who wait, but only the things left by those who hustle. ", gszLincoln,
   "Do not be too timid and squeamish about your actions. All life is an experiment. ", gszEmerson,
   "A man builds a fine house; and now he has a master, and a task for life; he is to furnish, watch, show it, and keep it in repair, the rest of his days. ", gszEmerson,
   "The world belongs to the energetic. ", gszEmerson,
   "People only see what they are prepared to see. ", gszEmerson,
   "I hate quotations. ", gszEmerson,
   "What lies behind us and what lies before us are tiny matters compared to what lies within us. ", gszEmerson,
   "Life is a perpetual instruction in cause and effect. ", gszEmerson,
   "Finish each day and be done with it. You have done what you could. Some blunders "
      "and absurdities no doubt crept in, forget them as soon as you can. Tomorrow "
      "is a new day, you shall begin it well and serenely... ", gszEmerson,
   "Without ambition one starts nothing. Without work one finishes nothing. The prize "
      "will not be sent to you. You have to win it. The man who knows how will always "
      "have a job. The man who also knows why will always be his boss. As to methods "
      "there may be a million and then some, but principles are few. The man who grasps "
      "principles can successfully select his own methods. The man who tries methods, "
      "ignoring principles, is sure to have trouble. ", gszEmerson,
   "It is very easy in the world to live by the opinion of the world. It is very easy "
      "in solitude to be self-centered. But the finished man is he who in the midst of "
      "the crowd keeps with perfect sweetness the independence of solitude. I knew a "
      "man of simple habits and earnest character who never put out his hands nor "
      "opened his lips to court the public, and having survived several rotten reputations "
      "of younger men, honor came at last and sat down with him upon his private "
      "bench from which he had never stirred. ", gszEmerson,
   "Work is victory. ", gszEmerson,
   "If a man's eye is on the Eternal, his intellect will grow. ", gszEmerson,
   "We are prisoners of ideas. ", gszEmerson,
   "The measure of a master is his success in bringing all men around to his opinion twenty years later. ", gszEmerson,
   "No great man ever complains of want of opportunity. ", gszEmerson,
   "We are students of words; we are shut up in schools, and colleges, and recitation "
      "rooms, for ten or fifteen years, and come out at last with a bag of wind, a "
      "memory of words, and do not know a thing ", gszEmerson,
   "Do not be too timid and squeamish about your actions. All life is an experiment. "
      "The more experiments you make the better. What if they are a little course, and "
      "you may get your coat soiled or torn? What if you do fail, and get fairly rolled "
      "in the dirt once or twice. Up again, you shall never be so afraid of a tumble. ", gszEmerson,
   "True knowledge exists in knowing that you know nothing. ", gszSocrates,
   "And in knowing that you know nothing, that makes you the smartest of all. ", gszSocrates,
   "He is richest who is content with the least. ", gszSocrates,
   "The test of a first-fate intelligence is the ability to hold two opposed ideas "
      "in mind at the same time and still retain the ability to function. One should, "
      "for example, be able to see that things are hopeless and yet be determined to "
      "make them otherwise. ", gszFitzgerald,
   "Sometimes I think we're alone. Sometimes I think we're not. In either case, the thought is staggering. ", gszFuller,
   "When I'm working on a problem, I never think about beauty. I think only how to solve the problem. But when I have finished, if the solution is not beautiful, I know it is wrong. ", gszFuller,
   "Nature is trying very hard to make us succeed, but nature does not depend on us. We are not the only experiment. ", gszFuller,
   "Don't limit a child to your own learning, for he was born in another time. ", gszRabbinicalSaying,
   "The beginning is the most important part of the work. ", gszPlato,
   "Necessity, who is the mother of invention. ", gszPlato,
   "Thinking is the talking of the soul with itself. ", gszPlato,
   "To do nothing is sometimes a good remedy. ", "Hippocrates",
   "The first principle is that you must not fool yourself - and you are the easiest person to fool. ", gszFeynman,
   "I was born not knowing and have had only a little time to change that here and there. ", gszFeynman,
   "6 * 9 = 42", "Douglas Adams",
   "A stitch in time saves nine.", gszAnonymous
   };

PWSTR DeepThoughtGenerate (void)
{
   HANGFUNCIN;
   WCHAR szTemp[2048];  // large
   // BUGFIX - use "6" instead of "3" to make a bit more random
   DFDATE today = Today();
   DWORD dwRand = RepeatableRandom(today + (today << 8),6);
   DWORD dwIndex;

   // try custom quotes
   QuotesGet();
   if (gListQuote.Num())
      dwIndex = (DWORD) dwRand % gListQuote.Num();
   else
      dwIndex = (DWORD) dwRand % (sizeof(gpaszDeepThought)/sizeof(PSTR)/2);
#if 0
   int   i;
   for (i = 0; i < 32; i++) {
      char szaTemp[64];
      DFDATE today = Today();
      today = MinutesToDFDATE (DFDATEToMinutes(today)+i*24*60);
      DWORD dwRand = RepeatableRandom(today + (today << 8),6);
      sprintf (szaTemp, "Deep rand=%d : %d\n", (int) dwRand,
         dwRand % (sizeof(gpaszDeepThought)/sizeof(PSTR)/2));
      OutputDebugString (szaTemp);
   }
#endif

   // convert it
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<big>");
   if (gListQuote.Num()) {
      MemCatSanitize (&gMemTemp, (PWSTR) gListQuote.Get(dwIndex));
   }
   else {
      MultiByteToWideChar (CP_ACP, 0, gpaszDeepThought[dwIndex*2], -1, szTemp, sizeof(szTemp)/2);
      MemCatSanitize (&gMemTemp, szTemp);
   }
   MemCat (&gMemTemp, L"</big><br/><align align=right><font color=#c0c080><italic>- ");
   if (gListQuote.Num()) {
      PWSTR psz = (PWSTR) gListQuoteAuthor.Get(dwIndex);
      if (!psz || !psz[0])
         psz = L"Unknown";
      MemCatSanitize (&gMemTemp, psz);
   }
   else {
      MultiByteToWideChar (CP_ACP, 0, gpaszDeepThought[dwIndex*2+1], -1, szTemp, sizeof(szTemp)/2);
      MemCatSanitize (&gMemTemp, szTemp);
   }
   MemCat (&gMemTemp, L"</italic></font></align>");

   return (PWSTR) gMemTemp.p;
}



/*****************************************************************************
QuotesPage - Override page callback.
*/
BOOL QuotesPage (PCEscPage pPage, DWORD dwMessage, PVOID pParam)
{
   HANGFUNCIN;
   static CMem memStartQuote, memStartAuthor;

   switch (dwMessage) {
   case ESCM_INITPAGE:
      {
         QuotesGet();

         // BUGFIX - set the start quote and author. So can edit quote
         PWSTR pszQuote = (PWSTR)memStartQuote.p;
         PWSTR pszAuthor = (PWSTR)memStartAuthor.p;
         PCEscControl pControl;
         pControl = pPage->ControlFind (L"notetext");
         if (pszQuote && pszQuote[0] && pControl)
            pControl->AttribSet (gszText, pszQuote);
         pControl = pPage->ControlFind (L"author");
         if (pszAuthor && pszAuthor[0] && pControl)
            pControl->AttribSet (gszText, pszAuthor);
         if (pszQuote)
            pszQuote[0] = 0;  // so dont use next time
         if (pszAuthor)
            pszAuthor[0] = 0; // so dont use next time
      }
      break;   // make sure to continue on


   case ESCN_BUTTONPRESS:
      {
         PESCNBUTTONPRESS p = (PESCNBUTTONPRESS) pParam;

         // only handle button press of "addproject"
         if (!p->pControl || !p->pControl->m_pszName)
            break;

         if (!_wcsicmp(p->pControl->m_pszName, L"add")) {
            // get the text
            WCHAR szHuge[256], szAuthor[64];
            PCEscControl   pControl;
            DWORD dwNeeded;
            szHuge[0] = 0;
            szAuthor[0] = 0;
            pControl = pPage->ControlFind (L"Notetext");
            if (pControl)
               pControl->AttribGet (gszText, szHuge, sizeof(szHuge), &dwNeeded);
            pControl = pPage->ControlFind (L"author");
            if (pControl)
               pControl->AttribGet (gszText, szAuthor, sizeof(szAuthor), &dwNeeded);

            if (!szHuge[0]) {
               pPage->MBWarning (L"You must type in some text for the quote.");
               return TRUE;
            }

            // add it
            gListQuote.Add (szHuge, (wcslen(szHuge)+1)*2);
            gListQuoteAuthor.Add (szAuthor, (wcslen(szAuthor)+1)*2);
            QuotesSet ();

            // tell user this
            EscChime (ESCCHIME_INFORMATION);
            EscSpeak (L"Quote added.");

            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

      }
      break;


   case ESCM_SUBSTITUTION:
      {
         PESCMSUBSTITUTION p = (PESCMSUBSTITUTION) pParam;

         // only care about projectlist
         if (_wcsicmp(p->pszSubName, L"CURRENTQUOTES"))
            break;

         QuotesGet();


         MemZero (&gMemTemp);


         DWORD i;
         for (i = 0; i < gListQuote.Num(); i++) {
            PWSTR pszQuote = (PWSTR) gListQuote.Get(i);
            PWSTR pszAuthor = (PWSTR) gListQuoteAuthor.Get(i);
            
            MemCat (&gMemTemp, L"<tr><td>");

            MemCat (&gMemTemp, L"<a href=qt:");
            MemCat (&gMemTemp, (int) i);
            MemCat (&gMemTemp, L">");
            MemCatSanitize (&gMemTemp, pszQuote);
            MemCat (&gMemTemp, L" - ");
            MemCatSanitize (&gMemTemp, pszAuthor);
            MemCat (&gMemTemp, L"</a>");

            MemCat (&gMemTemp, L"</td></tr>");
         }


         p->pszSubString = (PWSTR) gMemTemp.p;
         return TRUE;
      }
      break;

   case ESCM_LINK:
      {
         PESCMLINK p = (PESCMLINK) pParam;

         if (!p->psz)
            break;

         if ((p->psz[0] == L'q') && (p->psz[1] == L't') && (p->psz[2] == L':')) {
            DWORD dwIndex = _wtoi(p->psz + 3);

            // BUGFIX - If hold control key down then copy it back to edit...
            BOOL fControl = (GetKeyState (VK_CONTROL) < 0);

            if (!fControl) {
               if (IDYES != pPage->MBYesNo (L"Are you sure you wish to remove the quote?"))
                  return TRUE;
            }
            else {
               PWSTR pszQuote = (PWSTR)gListQuote.Get(dwIndex);
               PWSTR pszAuthor = (PWSTR)gListQuoteAuthor.Get(dwIndex);
               if (pszQuote) {
                  MemZero (&memStartQuote);
                  MemCat (&memStartQuote, pszQuote);
               }
               if (pszAuthor) {
                  MemZero (&memStartAuthor);
                  MemCat (&memStartAuthor, pszAuthor);
               }
            }

            gListQuote.Remove (dwIndex);
            gListQuoteAuthor.Remove (dwIndex);
            QuotesSet ();

            pPage->Exit (gszRedoSamePage);
            return TRUE;
         }

      }
      break;

   };


   return DefPage (pPage, dwMessage, pParam);
}


