/**************************************************************************************
Quote.cpp - Code for quote of the day.

begun 23/11/02 by Mike Rozak
Copyright 2002 Mike Rozak
*/

#include <windows.h>
#include <objbase.h>
#include <dsound.h>
#include "escarpment.h"
#include "..\..\m3d\M3D.h"
#include "..\..\m3d\mifl.h"
#include "..\mif.h"
#include "..\buildnum.h"
#include "mifclient.h"
#include "resource.h"



/**************************************************************************
DeepThoughtGenerate - Generates a randomly deep thought and returns
   a pointer to it. The pointer is gMemTemp.p

reutnrs
   PWSTR - string
*/
PSTR gszMikeRozak = "Mike Rozak";
PSTR gszAnonymous = "Anonymous";
PSTR gszFrankHerbert= "Frank Herbert";
PSTR gszAesop = "Aesop";
PSTR gszWilliamShakespeare = "William Shakespeare";
PSTR gszHawking = "Stephen Hawking";
PSTR gszEinstein= "Albert Einstein";
PSTR gszMarkTwain = "Mark Twain";
PSTR gszCharlesDickens = "Charles Dickens";
PSTR gszWoodyAllen = "Woody Allen";
PSTR gszVoltaire = "Voltaire";
PSTR gszAristotle = "Aristotle";
PSTR gszThoreau = "Henry David Thoreau";
PSTR gszAmericanIndianProverb = "American Indian Proverb";
PSTR gszIndianProverb = "Indian Proverb";
PSTR gszJapaneseProverb = "Japanese Proverb";
PSTR gszSlovenianProverb = "Slovenian Proverb";
PSTR gszChineseProverb = "Chinese Proverb";
PSTR gszGermanProverb = "German Proverb";
PSTR gszNigerianProverb = "Nigerian Proverb";
PSTR gszFrenchProverb = "French Proverb";
PSTR gszItalianProverb = "Italian Proverb";
PSTR gszMuslimProverb = "Muslim Proverb";
PSTR gszPortugueseProverb = "Portuguese Proverb";
PSTR gszYiddishProverb = "Yiddish Proverb";
PSTR gszDanishProverb = "Danish Proverb";
PSTR gszRussianProverb = "Russian Proverb";
PSTR gszAfricanProverb = "African Proverb";
PSTR gszEnglishProverb = "English Proverb";
PSTR gszAssyrianProverb = "Assyrian Proverb";
PSTR gszYugoslavProverb = "Yugoslav Proverb";
PSTR gszJefferson = "Thomas Jefferson";
PSTR gszGandhi = "Mahatma Gandhi";
PSTR gszLincoln = "Abraham Lincoln";
PSTR gszEmerson = "Ralph Waldo Emerson";
PSTR gszSocrates = "Socrates";
PSTR gszFitzgerald = "F. Scott Fitzgerald";
PSTR gszFuller = "Buckminster Fuller";
PSTR gszFeynman = "Richard Fenyman";
PSTR gszRabbinicalSaying = "Rabbinical Saying";
PSTR gszPlato = "Plato";
PSTR gszHowardPayne = "Howard Payne";
PSTR gszThomasHood = "Thomas Hood";
PSTR gszNewTestament = "New Testament";
PSTR gszSirEdwardCoke = "Sir Edward Coke";
PSTR gszAlfredTennyson = "Alfred Tennyson";
PSTR gszPlinyTheElder = "Pliny the Elder";
PSTR gszJohannVonGoethe = "Johann Von Goethe";
PSTR gszElielSaarinen = "Eliel Saarinen";
PSTR gszTupacShakur = "Tupak Shakur";
PSTR gszRandallMcBrideJr = "Randall McBride Jr.";
PSTR gszJulesDeGautier = "Jules De Gautier";
PSTR gszHenryWatson = "Henry Watson";
PSTR gszCarlSagan = "Carl Sagan";
PSTR gszJosephWeizenbaum = "Joseph Weizenbaum";
PSTR gszPhilipJohnson = "Philip Johnson";
PSTR gszFrankLloydWright = "Frank Lloyd Wright";
PSTR gszLouisHenriSullivan = "Louis Henri Sullivan";
PSTR gszStephenCovey = "Stephen Covey";
PSTR gszRobertKennedy = "Robert Kennedy";
PSTR gszSomersetMaugham = "Somerset Maugham";
PSTR gszOscarWilde = "Oscar Wilde";
PSTR gszPeterNivoZarlenga = "Peter Nivo Zarlenga";
PSTR gszJosephBrodsky = "Joseph Brodsky";
PSTR gszMarcusTulliusCicero = "Marcus Tullius Cicero";
PSTR gszSpanishProverb = "Spanish Proverb";
PSTR gszJRRTolkien = "J.R.R. Tolkien";
PSTR gszVictorDaniels = "Victor Daniels";
PSTR gszHenryMiller = "Henry Miller";
PSTR gszEricPio = "EricPio";
PSTR gszJeanJacquesRousseau = "Jean Jacques Rousseau";
PSTR gszAlanWatts = "Alan Watts";
PSTR gszRobertsonDavies = "Robertson Davies";
PSTR gszMotherTeresa = "Mother Teresa";
PSTR gszDouglasAdams = "Douglas Adams";
PSTR gszGilbertChesterton = "Gilbert Chesterton";
PSTR gszDanCruickshank = "Dan Cruickshank";
PSTR gszRalphErskine = "Ralph Erskine";
PSTR gszFaithPopcorn = "Faith Popcorn";
PSTR gszHLMencken = "H.L. Mencken";
PSTR gszGeorgeBernardShaw = "George Bernard Shaw";
PSTR gszGarrisonKeillor = "Garrison Keillor";
PSTR gszLizaMinnelli = "Lisa Minnelli";
PSTR gszMarkHamilton = "Mark Hamilton";
PSTR gszRBuckminsterFuller = "R. Buckminster Fuller";


#define DRTWEB "http://www.mXac.com.au/DRT/"

PSTR gpaszDeepThought[] = {
   "Our culture is obsessed with veneers at the expense of substance.", gszMikeRozak, DRTWEB "Thought1.htm",
   "Buildings that shelter from nature so much that we never experience it also distort our "
      "perception of reality.", gszMikeRozak, DRTWEB "Thought2.htm",
   "Contemporary housing is inhumane, since it is clearly not designed for human primates.", gszMikeRozak, DRTWEB "Thought3.htm",
   "People purchase wontonly in the subconsious hope it will change their unhappy lives.", gszMikeRozak, DRTWEB "Thought4.htm",
   "Ask anyone if they'd like an extra room or two added onto their house and they'd answer yes. Why?", gszMikeRozak, DRTWEB "Thought5.htm",
   "En-masse housing is like a Big Mac - designed to offend no one.", gszMikeRozak, DRTWEB "Thought6.htm",
   "Building a house is an exercise in material science.", gszMikeRozak, NULL,
   "It's easy to lose sight of our goals in the turbulance of petty worries.", gszMikeRozak, NULL,
   "Experts break their own rules.", gszMikeRozak, NULL,
   "Most life is wasted waiting for life to happen.", gszMikeRozak, NULL,
   "The small details of life obscure the larger and more important ones.", gszMikeRozak, NULL,
   "I am the eye at the center of the universe peering out upon myself.", gszMikeRozak, NULL,
   "Decide what is important and own it. Let chaos rule the rest.", gszMikeRozak, NULL,
   "There are more things in heaven and earth, Horatio, than are dreamt of in your philosophy. ", gszWilliamShakespeare, NULL,
   "We are just an advanced breed of monkeys on a minor planet of a very average star. But we can understand the Universe. That makes us something very special. ", gszHawking, NULL,
   "Imagination is more important than knowledge... ", gszEinstein, NULL,
   "Everything should be made as simple as possible, but not one bit simpler. ", gszEinstein, NULL,
   "In the middle of difficulty lies opportunity. ", gszEinstein, NULL,
   "The problems that exist in the world today cannot be solved by the level of thinking that created them. ", gszEinstein, NULL,
   "Problems cannot be solved at the same level of awareness that created them. ", gszEinstein, NULL,
   "A human being is part of a whole, called by us the Universe, a part limited in time and "
      "space. He experiences himself, his thoughts and feelings, as something separated "
      "from the rest--a kind of optical delusion of his consciousness. This delusion is a "
      "kind of prison for us, restricting us to our personal desires and to affection for "
      "a few persons nearest us. Our task must be to free ourselves from this prison by "
      "widening our circles of compassion to embrace all living creatures and the whole "
      "of nature in its beauty. ", gszEinstein, NULL,
   "Concern for man himself and his fate must always form the chief interest of all technical endeavor. Never forget this in the midst of your diagrams and equations. ", gszEinstein, NULL,
   "A banker is a fellow who lends you his umbrella when the sun is shining, but wants it back the minute it begins to rain. ", gszMarkTwain, NULL,
   "All you need in this life is ignorance and confidence, and then success is sure. ", gszMarkTwain, NULL,
   "Clothes make the man. Naked people have little or no influence on society. ", gszMarkTwain, NULL,
   "Never put off until tomorrow what you can do the day after tomorrow. ", gszMarkTwain, NULL,
   "Whenever you find that you are on the side of the majority, it is time to reform. ", gszMarkTwain, NULL,
   "Don't part with your illusions. When they are gone you may still exist but you have ceased to live. ", gszMarkTwain, NULL,
   "Keep away from people who try to belittle your ambitions. Small people always do that, but the really great make you feel that you, too, can become great. ", gszMarkTwain, NULL,
   "All you need in this life is ignorance and confidence -- and then success is sure. ", gszMarkTwain, NULL,
   "Train up a fig tree in the way it should go, and when you are old sit under the shade of it. ", gszCharlesDickens, NULL,
   "I don't want to achieve immortality through my work... I want to achieve it through not dying. ", gszWoodyAllen, NULL,
   "Every man is guilty of all the good he didn't do. ", gszVoltaire, NULL,
   "What is the use of a house if you haven't got a tolerable planet to put it on? ", gszThoreau, NULL,
   "Men have become the tools of their tools. ", gszThoreau, NULL,
   "Our houses are such unwieldy property that we are often imprisoned rather than housed in them. ", gszThoreau, NULL,
   "Live each season as it passes; breathe the air, drink the drink, taste the fruit, and resign yourself to the influences of each. ", gszThoreau, NULL,
   "Our life is frittered away by detail. Simplify, simplify. ", gszThoreau, NULL,
   "A man is rich in proportion to the number of things he can afford to let alone. ", gszThoreau, NULL,
   "As for the pyramids, there is nothing to wonder at in them so much as the fact "
      "that so many men could be found degraded enough to spend their lives constructing "
      "a tomb for some ambitious booby, whom it would have been wiser and manlier "
      "to have drowned in the Nile, and then given his body to the dogs. ", gszThoreau, NULL,
   "The reverse side also has a reverse side. ", gszJapaneseProverb, NULL,
   "Speak the truth, but leave immediately after. ", gszSlovenianProverb, NULL,
   "Who begins too much accomplishes little. ", gszGermanProverb, NULL,
   "Proportion your expenses to what you have, not what you expect. ", gszEnglishProverb, NULL,
   "Delay is preferable to error. ", gszJefferson, NULL,
   "Whatever you do will be insignificant, but it is very important that you do it. ", gszGandhi, NULL,
   "Do not be too timid and squeamish about your actions. All life is an experiment. ", gszEmerson, NULL,
   "A man builds a fine house; and now he has a master, and a task for life; he is to furnish, watch, show it, and keep it in repair, the rest of his days. ", gszEmerson, NULL,
   "We are prisoners of ideas. ", gszEmerson, NULL,
   "The measure of a master is his success in bringing all men around to his opinion twenty years later. ", gszEmerson, NULL,
   "Necessity, who is the mother of invention. ", gszPlato, NULL,
   "A stitch in time saves nine.", gszAnonymous, NULL,
   "Be it ever so humble, there's no place like home.", gszHowardPayne, NULL,
   "...rest at length have come. All the day's long toil is past, And each heart is whispering, Home, Home at last.", gszThomasHood, NULL,
   "If a house be divided against itself, that house cannot stand.", gszNewTestament, NULL,
   "For a man's house is his castle, et domus sua cuique tutissimum refugium.", gszSirEdwardCoke, NULL,
   "I remember, I remember the house there I was born, the little window where the sun came peeing in at morn.", gszThomasHood, NULL,
   "I built my soul a lordly pleasure-house, Wherein at ease for aye to dwell.", gszAlfredTennyson, NULL,
   "When a building is about to fall down, all the mice desert it.", gszPlinyTheElder, NULL,
   "Architecture is frozen music.", gszAnonymous, NULL,
   "He is happiest, be he king or peasant, who finds peace in his home.", gszJohannVonGoethe, NULL,
   "Always design a thing by considering it in its next larger context - a chair in a room, "
      "a room in a house, a house in an environment, and environment in a city plan.", gszElielSaarinen, NULL,
   "Reality is wrong. Dreams are for real.", gszTupacShakur, NULL,
   "Don't dwell on reality; it will only keep you from greatness.", gszRandallMcBrideJr, NULL,
   "Imagination is the only weapon in the war against reality.", gszJulesDeGautier, NULL,
   "In architecture as in all other operative arts, the end must direct the operation. The "
      "end is to build well. Wll building has three conditions: Commodity, Firmness and Delight.", gszHenryWatson, NULL,
   "If you want to make an apple pie from scratch, you must first create the universe.", gszCarlSagan, NULL,
   "The computer programmer is a creator of universes for which he alone is responsible. "
      "Universes of virtually unlimited complexity can be created in the form of computer programs.", gszJosephWeizenbaum, NULL,
   "ARCHITECTURE, n: The art of how to waste space.", gszPhilipJohnson, NULL,
   "Noble life demands a noble architecture for noble uses of noble men. "
      "Lack of culture means what it has always meant: ignoble civilization and therefore imminent downfall.", gszFrankLloydWright, NULL,
   "Form follows function.", gszLouisHenriSullivan, NULL,
   "Live out of your imagination, not your history.", gszStephenCovey, NULL,
   "Imagination is more important than knowledge. Knowledge is limited. Imagination encircles the world.", gszEinstein, NULL,
   "Some men see things as they are and ask, \"Why?\" I dream thinks that never where "
      "and ask \"Why not?\"", gszRobertKennedy, NULL,
   "Imagination grows by exercise, and contrary to common belief, is more powerful in the mature than in the young.", gszSomersetMaugham, NULL,
   "Consistency is the last resort of the unimaginative.", gszOscarWilde, NULL,
   "I am imagination. I can see what the eyes cannot see. I can hear what the ears cannot hear. "
      "I can feel what the heart cannot feel.", gszPeterNivoZarlenga, NULL,
   "A doctor can bury is mistakes but an architect can only advise his client to plant vines.", gszFrankLloydWright, NULL,
   "If it keeps up, man will atrophy all his limbs but the push-button finger.", gszFrankLloydWright, NULL,
   "No matter under what circumstances you leave it, home does not cease to be home. "
      "No matter how you lived there - well or poorly.", gszJosephBrodsky, NULL,
   "A home without books is a body without soul.", gszMarcusTulliusCicero, NULL,
   "Home is where the toys are.", gszMikeRozak, NULL,
   "The road to a friend's house is never long.", gszDanishProverb, NULL,
   "He that lives in a glass house must not throw stones.", gszEnglishProverb, NULL,
   "If your house is on fire, warm yourself by it.", gszSpanishProverb, NULL,
   "His house was perfect, whether you liked food, or sleep, or work, or story-telling, or singing, "
      "or just sitting and thinking, best, or a pleasant mixture of them all.", gszJRRTolkien, NULL,
   "We must learn to tailor our concepts to fit reality, instead of trying to stuff reality into our concepts.", gszVictorDaniels, NULL,
   "Chaos is the score upon which reality is written.", gszHenryMiller, NULL,
   "If you won't make your dreams a reality, reality will take your dreams away.", gszEricPio, NULL,
   "The world of reality has its limits; the world of imagination is boundless.", gszJeanJacquesRousseau, NULL,
   "But my dear man, reality is only a Rorschach ink-blot, you know.", gszAlanWatts, NULL,
   "A truly great book should be read in youth, again in maturity and once more in old age, "
      "as a fine building should be seen by morning light, at noon and by moonlight.", gszRobertsonDavies, NULL,
   "What you spend years building may be destroyed overnight. Build anyway.", gszMotherTeresa, NULL,
   "A computer terminal is not some clunky old television with a typewriter in front of it. "
      "It is an interface where the mind and body can connect with the universe and move "
      "bits of it about.", gszDouglasAdams, NULL,
   "All architecture is great architecture after sunset; perhaps architecture is really a "
      "nocturnal art, like the art of fireworks.", gszGilbertChesterton, NULL,
   "In short, the building becomes a theatrical demonstration of its functional ideal. In this "
      "romanicisim, High-Tech architecture is, of course, no different in spirit - if totally different "
      "in form - from all the romantic architecture of the past.", gszDanCruickshank, NULL,
   "The job of buildings is to improve human relations; architecture must ease them, not make them worse.", gszRalphErskine, NULL,
   "A common mistake that people make when trying to design something completely foolproof "
      "is to underestimate the ingenuit of complete fools.", gszDouglasAdams, NULL,
   "Give me the luxuries of life and I will willingly do without the necessities.", gszFrankLloydWright, NULL,
   "Tip the world over on its side and everything loose will land in Los Angeles.", gszFrankLloydWright, NULL,
   "Many wealthy people are little more than janitors of their possessions.", gszFrankLloydWright, NULL,
   "The trouble in corporate America is that too many people with too much power live in a box (their home), "
      "then travel the same road every day to another box (their office).", gszFaithPopcorn, NULL,
   "A home is not a mere transient shelter: its essence lies in the personalities of the people who live in it.", gszHLMencken, NULL,
   "Home life as we understand it is no more natural to us than a cage is natural to a cockatoo.", gszGeorgeBernardShaw, NULL,
   "I believe in looking reality straight in the eye and denying it.", gszGarrisonKeillor, NULL,
   "Reality is something you rise above.", gszLizaMinnelli, NULL,
   "\"Reality\" is the only world in the English language that should always be used in quotes.", gszAnonymous, NULL,
   "\"Virtual Reality\" is a name being slapped on almost anything these days, especially if it's lame.", gszMarkHamilton, NULL,
   "Everything you've learned in school as \"obvious\" becomes less and less obvious as you begin to "
      "study the universe. For example, there are no solids in the universe. There's not even a suggestion of a sold. "
      "There are no absolute continuums. There are no surfaces. There are no straight lines.", gszRBuckminsterFuller, NULL,
   "The engineer's first problem in any design situation is to discover what the problem really is.", gszAnonymous, NULL,
   "If you have built castles in the air, your work need not be lost; that is where they should "
      "be. Now put the foundations under them.", gszThoreau, NULL,
   "What is the use of a house if you haven't got a tolerable planet to put it on?", gszThoreau, NULL,
   "Every generation laughs at the old fasions, but follows religiously the new.", gszThoreau, NULL,
   "Our houses are such unwieldy property that we are often imprisoned rather than housed in them.", gszThoreau, NULL
   };

PWSTR DeepThoughtGenerate (void)
{
   WCHAR szTemp[2048];  // large
   SYSTEMTIME st;
   DWORD dwRand;

   memset (&st, 0, sizeof(st));
   GetSystemTime (&st);
   srand ((DWORD)st.wDay + st.wHour + st.wMinute + st.wMonth + st.wSecond + st.wYear +
      GetTickCount());
   rand();  // just to make a  bit more random
   dwRand = rand();
   DWORD dwIndex;

#define NUMELEM      3
   dwIndex = sizeof(gpaszDeepThought)/sizeof(PSTR);
   dwIndex = (DWORD) dwRand % (dwIndex/NUMELEM);

   PSTR pszLink;
   pszLink = gpaszDeepThought[dwIndex*NUMELEM+2];

   // convert it
   MemZero (&gMemTemp);
   MemCat (&gMemTemp, L"<big>");
   if (pszLink) {
      MemCat (&gMemTemp, L"<a href=\"");
      //MemCat (&gMemTemp, L"<a color=#c0c0ff href=\"");
      MultiByteToWideChar (CP_ACP, 0, pszLink, -1, szTemp, sizeof(szTemp)/2);
      MemCatSanitize (&gMemTemp, szTemp);
      MemCat (&gMemTemp, L"\">");
   }
   MultiByteToWideChar (CP_ACP, 0, gpaszDeepThought[dwIndex*NUMELEM], -1, szTemp, sizeof(szTemp)/2);
   MemCatSanitize (&gMemTemp, szTemp);
   if (pszLink)
      MemCat (&gMemTemp, L"</a>");
   MemCat (&gMemTemp, L"</big><br/><align align=right><font color=#404000><italic>- ");
   //MemCat (&gMemTemp, L"</big><br/><align align=right><font color=#c0c080><italic>- ");
   MultiByteToWideChar (CP_ACP, 0, gpaszDeepThought[dwIndex*NUMELEM+1], -1, szTemp, sizeof(szTemp)/2);
   MemCatSanitize (&gMemTemp, szTemp);
   MemCat (&gMemTemp, L"</italic></font></align>");

   return (PWSTR) gMemTemp.p;
}



// FUTURERELEASE - Include not only quotes, but links to interesting web sites dealing
// with houses and VR
