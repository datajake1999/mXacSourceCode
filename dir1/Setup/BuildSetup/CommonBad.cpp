/***************************************************************************
Common.cpp - routines common among different SiliconPage exes

begun 11/17/99 by Mike Rozak
Copyright 1999 Mike Rozak. All rights reserved
*/

#include <windows.h>
#include <objbase.h>
#include <commctrl.h>
#include <stdio.h>
#include "z:\setup\buildsetup\Common.h"

#define  MXAC     MAKEFOURCC ('m', 'X', 'a', 'c')

typedef struct {
   DWORD       dwMagic; // magic number, MXAC
   DWORD       dwEncrypt;  // encryption key
} ENCHEADER, *PENCHEADER;

/* globals */
LANGIDTOSTRING gaLIDToString[] = {
   {LANG_ALBANIAN, "Albanian"},
   {LANG_ARABIC, "Arabic"},
   {0x21, "Bahasa"},
   {LANG_BULGARIAN, "Bulgarian"},
   {LANG_CATALAN, "Catalan"},
   {LANG_CHINESE, "Chinese"},
   {LANG_CZECH, "Czech"},
   {LANG_DANISH, "Danish"},
   {LANG_DUTCH, "Dutch"},
   {LANG_ENGLISH, "English"},
   {LANG_FINNISH, "Finnish"},
   {LANG_FRENCH, "French"},
   {LANG_GERMAN, "German"},
   {LANG_GREEK, "Greek"},
   {LANG_HEBREW, "Hebrew"},
   {LANG_HUNGARIAN, "Hungarian"},
   {LANG_ICELANDIC, "Icelandic"},
   {LANG_ITALIAN, "Italian"},
   {LANG_JAPANESE, "Japanese"},
   {LANG_KOREAN, "Korean"},
   {LANG_NORWEGIAN, "Norwegian"},
   {LANG_POLISH, "Polish"},
   {LANG_PORTUGUESE, "Poruguese"},
   {0x17, "Rhaeto Roman"},
   {LANG_ROMANIAN, "Romanian"},
   {LANG_RUSSIAN, "Russian"},
   {0x1a, "Serbo Croatian"},
   {LANG_SLOVAK, "Slovak"},
   {LANG_SPANISH, "Spanish"},
   {LANG_SWEDISH, "Swedish"},
   {LANG_THAI, "Thai"},
   {LANG_TURKISH, "Turkish"},
   {LANG_URDU, "Urdu"}
};


SUBIDTOSTRING gaSubIDToString[] = {
   {1, 0, "Art, Architecture, and Photography"},
   {1, 1, "Architecture"},
   {1, 2, "Art History"},
   {1, 3, "Dance"},
   {1, 4, "Decorative Arts and Design"},
   {1, 5, "Museums and Collections"},
   {1, 6, "Other Media"},
   {1, 7, "Painting"},
   {1, 8, "Photography"},
   {1, 9, "Sculpture"},
   {1, 10, "Theater"},

   {2, 0, "Biography"},
   {2, 1, "Arts and Literature"},
   {2, 2, "Children's and Young Adult"},
   {2, 3, "Entertainers"},
   {2, 4, "Historical"},
   {2, 5, "Leaders"},
   {2, 6, "Professionals and Academics"},
   {2, 7, "Reference and Collections"},
   {2, 8, "Sports and Outdoors"},

   {3, 0, "Business and Investing"},
   {3, 1, "Accounting"},
   {3, 2, "Biographies"},
   {3, 3, "Careers"},
   {3, 4, "Consulting"},
   {3, 5, "E-Commerce"},
   {3, 6, "Economics"},
   {3, 7, "Industries and Professions"},
   {3, 8, "Management and Leadership"},
   {3, 9, "Management and Professional"},
   {3, 10, "Marketing and Sales"},
   {3, 11, "Personal Finance and Investing"},
   {3, 12, "Primers"},
   {3, 13, "Reference"},
   {3, 14, "Small Business and Entrepreneurship"},
   {3, 15, "Time Management and Organization"},

   {4, 0, "Computers"},
   {4, 1, "Artificial Intelligence"},
   {4, 2, "Certification"},
   {4, 3, "Computer Science"},
   {4, 4, "Cyberculture"},
   {4, 5, "Graphics"},
   {4, 6, "Internet"},
   {4, 7, "Networking and OS"},
   {4, 8, "New to Computing"},
   {4, 9, "Programming"},

   {5, 0, "Cooking"},
   {5, 1, "Africa"},
   {5, 2, "Americas"},
   {5, 3, "Asia"},
   {5, 4, "Australia and Oceania"},
   {5, 5, "Baking"},
   {5, 6, "Canning and Preserving"},
   {5, 7, "Cooking by Ingredient"},
   {5, 8, "Drinks and Beverages"},
   {5, 9, "Entertaining"},
   {5, 10, "Europe"},
   {5, 11, "Gastronomy"},
   {5, 12, "Low-Fat"},
   {5, 13, "Meals"},
   {5, 14, "Outdoor Cooking"},
   {5, 15, "Professional Cooking"},
   {5, 16, "Quick and Easy"},
   {5, 17, "Reference"},
   {5, 18, "Regional and International"},
   {5, 19, "Special Diet"},
   {5, 20, "Special Occasions"},
   {5, 21, "Vegetables and Vegetarian"},

   {6, 0, "Entertainment"},
   {6, 1, "Comics"},
   {6, 2, "Games"},
   {6, 3, "Humor"},
   {6, 4, "Movies"},
   {6, 5, "Music"},
   {6, 6, "Pop Culture"},
   {6, 7, "Radio"},
   {6, 8, "Television"},

   {7, 0, "Fiction and Literature"},
   {7, 1, "Americas"},
   {7, 2, "Asia"},
   {7, 3, "Australia and Oceania"},
   {7, 4, "Classics"},
   {7, 5, "Collection"},
   {7, 6, "Drama"},
   {7, 7, "Essays"},
   {7, 8, "Europe"},
   {7, 9, "Genre Fiction"},
   {7, 10, "History and Criticism"},
   {7, 11, "Poetry"},
   {7, 12, "Short Stories"},

   {8, 0, "History"},
   {8, 1, "Acient"},
   {8, 2, "Africa"},
   {8, 3, "Americas"},
   {8, 4, "Asia"},
   {8, 5, "Australia and Ocieania"},
   {8, 6, "Biographies"},
   {8, 7, "Europe"},
   {8, 8, "Gay and Lesbian"},
   {8, 9, "Historical Study"},
   {8, 11, "Middle East"},
   {8, 12, "Military"},
   {8, 13, "World"},

   {9, 0, "Home and Garden"},
   {9, 1, "Antiques and Collectibles"},
   {9, 2, "Crafts and Hobbies"},
   {9, 3, "Entertaining"},
   {9, 4, "Expert Advice"},
   {9, 5, "Gardening and Horticulture"},
   {9, 6, "Home Design"},
   {9, 7, "How-To and Home Improvements"},
   {9, 8, "Interior Decoration"},
   {9, 9, "Pets"},
   {9, 10, "Weddings"},

   {10, 0, "Horror and Suspense"},
   {10, 1, "Anthologies"},
   {10, 2, "Dark Fantasy"},
   {10, 3, "Erotic"},
   {10, 4, "Ghosts"},
   {10, 5, "Graphic Novels"},
   {10, 6, "Horror"},
   {10, 7, "Occult"},
   {10, 8, "Reference"},
   {10, 9, "Suspense"},
   {10, 10, "Thrillers"},
   {10, 11, "Vampires"},
   {10, 12, "Writing"},

   {11, 0, "Kids"},
   {11, 1, "Animals"},
   {11, 2, "Anthologies"},
   {11, 3, "Arts and Music"},
   {11, 4, "Classics"},
   {11, 5, "Computers"},
   {11, 6, "Educational"},
   {11, 7, "History and Historical Fiction"},
   {11, 8, "Literature"},
   {11, 9, "People and Places"},
   {11, 10, "Popular Characters"},
   {11, 11, "Reference and Nonfiction"},
   {11, 12, "Religions"},
   {11, 13, "Science and Nature"},
   {11, 14, "Sports and Activities"},

   {12, 0, "Mind, Body, and Spirit"},
   {12, 1, "Alternative Medicine"},
   {12, 2, "Beauty and Fashion"},
   {12, 3, "Cancel"},
   {12, 4, "Death and Grief"},
   {12, 5, "Diet and Nutrition"},
   {12, 6, "Fitness"},
   {12, 7, "Health-Related Professions"},
   {12, 8, "Medicine"},
   {12, 9, "Men's Health"},
   {12, 10, "Mental Health"},
   {12, 11, "Natural Medicine"},
   {12, 12, "Nursing"},
   {12, 13, "Psychology and Counseling"},
   {12, 14, "Recovery"},
   {12, 15, "Reference"},
   {12, 16, "Self-improvement"},
   {12, 17, "Sex"},
   {12, 18, "Women's Health"},

   {13, 0, "Mystery and Thrillers"},
   {13, 1, "Action-Adventure"},
   {13, 2, "Legal Thrillers"},
   {13, 3, "Mystery"},
   {13, 4, "Police Procedural"},
   {13, 5, "Spy Thrillers"},
   {13, 6, "Writing"},

   {14, 0, "Nonfiction"},
   {14, 1, "Automotive"},
   {14, 2, "Crime and Criminals"},
   {14, 3, "Current Events"},
   {14, 4, "Economics"},
   {14, 5, "Education"},
   {14, 6, "Gay and Lesbian"},
   {14, 7, "Government"},
   {14, 8, "Holidays"},
   {14, 9, "Law"},
   {14, 10, "Media"},
   {14, 11, "Philosophy"},
   {14, 12, "Politics"},
   {14, 13, "Social Sciences"},
   {14, 14, "True Accounts"},
   {14, 15, "Urban Planning and Development"},
   {14, 16, "Women's Studies"},

   {15, 0, "Parenting and Family"},
   {15, 1, "Adoption"},
   {15, 2, "Aging Parents"},
   {15, 3, "Education"},
   {15, 4, "Family Activities"},
   {15, 5, "Family Health"},
   {15, 6, "Family Relationships"},
   {15, 7, "Fertility"},
   {15, 8, "Infants and Toddlers"},
   {15, 9, "Literature Guides"},
   {15, 10, "Parenting"},
   {15, 11, "Pregnancy and Childbirth"},
   {15, 12, "Reference"},
   {15, 13, "Special Needs"},

   {16, 0, "Professional and Technical"},
   {16, 1, "Engineering"},
   {16, 2, "Law"},
   {16, 3, "Medical"},
   {16, 4, "Writer's Workshop"},

   {17, 0, "Reference"},
   {17, 1, "Almanacs and Annuals"},
   {17, 2, "Atlases and Maps"},
   {17, 3, "Business Skills"},
   {17, 4, "Calendars"},
   {17, 5, "Careers"},
   {17, 6, "Catalogs and Directories"},
   {17, 7, "Consumer Guides"},
   {17, 8, "Dictionaries and Thesauri"},
   {17, 9, "Education"},
   {17, 10, "Encylopedias"},
   {17, 11, "Etiquette"},
   {17, 12, "Foreign Languages"},
   {17, 13, "Fun Facts"},
   {17, 14, "Genealogy"},
   {17, 15, "Law"},
   {17, 16, "Publishing and Books"},
   {17, 17, "Quotations"},
   {17, 18, "Study Guides"},
   {17, 19, "Test Prep. Central"},
   {17, 20, "Transportation"},
   {17, 21, "Words and Language"},
   {17, 22, "Writing"},

   {18, 0, "Religion"},
   {18, 1, "Bibles"},
   {18, 2, "Buddhism"},
   {18, 3, "Catholicism"},
   {18, 4, "Charismatic"},
   {18, 5, "Children's Books"},
   {18, 6, "Church History"},
   {18, 8, "Clergy"},
   {18, 9, "Comparative Religion"},
   {18, 10, "Earth-Based Religions"},
   {18, 11, "Eastern Religions"},
   {18, 12, "Education"},
   {18, 13, "Evangelism"},
   {18, 14, "Fiction, Literature, and Poetry"},
   {18, 15, "Hinduism"},
   {18, 16, "History"},
   {18, 17, "Holidays"},
   {18, 18, "Inspiration and Memoir"},
   {18, 19, "Islam"},
   {18, 20, "Jesus"},
   {18, 21, "Judaism"},
   {18, 22, "Lifestyle"},
   {18, 23, "Mythology"},
   {18, 24, "New Age"},
   {18, 25, "Occult"},
   {18, 26, "Orthodoxy"},
   {18, 27, "Other Eastern Religions"},
   {18, 28, "Philosophy"},
   {18, 29, "Protestantism"},
   {18, 30, "Spirituality"},
   {18, 31, "Theology"},
   {18, 32, "Worship and Devotion"},

   {19, 0, "Romance"},
   {19, 1, "Anthologies"},
   {19, 2, "Contemporary"},
   {19, 3, "Fantasy"},
   {19, 4, "Historical"},
   {19, 5, "Multicultural"},
   {19, 6, "Religious"},
   {19, 7, "Romantic Suspense"},
   {19, 8, "Writing"},

   {20, 0, "Science and Nature"},
   {20, 1, "Agricultural"},
   {20, 2, "Anthropology"},
   {20, 3, "Archaeology"},
   {20, 4, "Astronomy and Cosmology"},
   {20, 5, "Behavioral Sciences"},
   {20, 6, "Biological Sciences"},
   {20, 7, "Chemistry"},
   {20, 8, "Earth Sciences"},
   {20, 9, "Education"},
   {20, 10, "Engineering"},
   {20, 11, "Evolution"},
   {20, 12, "Experiments, Instruments, and Measurement"},
   {20, 13, "History and Philosophy"},
   {20, 14, "Mathematics"},
   {20, 15, "Medicine"},
   {20, 16, "Military Science"},
   {20, 17, "Nature and Ecology"},
   {20, 18, "Physics"},
   {20, 19, "Reference"},
   {20, 20, "Technology"},
   {20, 21, "Weather"},

   {21, 0, "Science Fiction and Fantasy"},
   {21, 1, "Epic"},
   {21, 2, "Fantasy"},
   {21, 3, "Gaming"},
   {21, 4, "Science Fiction"},
   {21, 5, "Space Opera"},

   {22, 0, "Sports and Outdoors"},
   {22, 1, "Adventure"},
   {22, 2, "Baseball"},
   {22, 3, "Basketball"},
   {22, 4, "Children's Sports"},
   {22, 5, "Coaching"},
   {22, 6, "Football (American)"},
   {22, 7, "Golf"},
   {22, 8, "Hiking and Camping"},
   {22, 9, "Hockey"},
   {22, 10, "Hunting and Fishing"},
   {22, 11, "Individual Sports"},
   {22, 12, "Miscellaneous"},
   {22, 13, "Mountaineering"},
   {22, 14, "Other Team Sports"},
   {22, 15, "Outdoor Exploration"},
   {22, 16, "Racket Sports"},
   {22, 17, "Soccer"},
   {22, 18, "Training"},
   {22, 19, "Water Sports"},
   {22, 20, "Winter Sports"},

   {23, 0, "Teens"},
   {23, 1, "Biography"},
   {23, 2, "Family and Friends"},
   {23, 3, "Health, Mind, and Body"},
   {23, 4, "History and Historical Fiction"},
   {23, 5, "Horror"},
   {23, 6, "Literature and Fiction"},
   {23, 7, "Love and Relationships"},
   {23, 8, "Mysteries"},
   {23, 9, "Reference"},
   {23, 10, "Religion and Spirituality"},
   {23, 11, "School and Sports"},
   {23, 12, "Science Fiction and Fantasy"},
   {23, 13, "Social Issues"},

   {24, 0, "Travel"},
   {24, 1, "Africa"},
   {24, 2, "Americas"},
   {24, 3, "Asia"},
   {24, 4, "Australia and Oceania"},
   {24, 5, "Europe"},
   {24, 6, "Middle East"},
   {24, 7, "Polar Regions"},
   {24, 8, "Specialty Travel"}
};


/**********************************************************************************
LangIndex - Given an index into the list of languages return string.
*/
LANGIDTOSTRING *LangIndex (DWORD i)
{
   if (i >=sizeof(gaLIDToString) / sizeof(LANGIDTOSTRING))
      return NULL;

   return gaLIDToString + i;
}

/**********************************************************************************
LangNum - Returns the number of languages available
*/
DWORD LangNum (void)
{
   return sizeof(gaLIDToString) / sizeof(LANGIDTOSTRING);
}

/**********************************************************************************
LangIndexFromID - Given language ID, return string
*/
DWORD LangIndexFromID (LANGID lid)
{
   DWORD i;
   for (i = 0; i < sizeof(gaLIDToString) / sizeof(LANGIDTOSTRING); i++)
      if (gaLIDToString[i].lid == lid)
         return i;

   return (DWORD)-1;
}

/**********************************************************************************
LangFromID - Given language ID, return string
*/
char *LangFromID (LANGID lid)
{
   DWORD i;
   i= LangIndexFromID (lid);
   if (i != (DWORD)-1)
      return gaLIDToString[i].psz;

   return NULL;
}

/**********************************************************************************
LangToID - Given a language string convert to a lang ID
*/
LANGID LangToID (char *psz)
{
   DWORD i;
   for (i = 0; i < sizeof(gaLIDToString) / sizeof(LANGIDTOSTRING); i++)
      if (!stricmp(gaLIDToString[i].psz, psz))
         return gaLIDToString[i].lid;

   return 0;
}

/**********************************************************************************
SubIndex - Given an index return a subject

inputs
   DWORD i - index
returns
   SUBIDTOSTRING*
*/
SUBIDTOSTRING * SubIndex (DWORD i)
{
   if (i >= sizeof(gaSubIDToString) / sizeof(SUBIDTOSTRING))
      return NULL;

   return gaSubIDToString + i;
}


/**********************************************************************************
SubNum - Returns the number of subjects available
*/
DWORD SubNum (void)
{
   return sizeof(gaSubIDToString) / sizeof(SUBIDTOSTRING);
}

/********************************************************************************
SubFromID - Subject from ID.

inputs
   WORD     wPrimary - primary
   WORD     wSecondary - secondary
returns
   char *
*/
char *SubFromID (WORD wPrimary, WORD wSecondary)
{
   DWORD i;
   for (i = 0; i < sizeof(gaSubIDToString) / sizeof(SUBIDTOSTRING); i++)
      if ((gaSubIDToString[i].wMajor == wPrimary) && (gaSubIDToString[i].wMinor == wSecondary))
         return gaSubIDToString[i].psz;

   return NULL;

}


/********************************************************************************
SubToPrimaryID - Convert subject string that's a primary subject into an ID.

inputs
   char *psz
returns
   WORD - primary subject ID
*/
WORD SubToPrimaryID (char *psz)
{
   DWORD i;
   for (i = 0; i < sizeof(gaSubIDToString) / sizeof(SUBIDTOSTRING); i++)
      if ((gaSubIDToString[i].wMinor == 0) && !stricmp(psz, gaSubIDToString[i].psz))
         return gaSubIDToString[i].wMajor;

   return 0;

}


/********************************************************************************
SubToSecondaryID - Convert subject string that's a secondary subject into an ID.

inputs
   WORD  wPrimary - Primary ID number
   char *psz
returns
   WORD - primary subject ID
*/
WORD SubToSecondaryID (WORD wPrimary, char *psz)
{
   DWORD i;
   for (i = 0; i < sizeof(gaSubIDToString) / sizeof(SUBIDTOSTRING); i++)
      if ((gaSubIDToString[i].wMajor == wPrimary) && !stricmp(psz, gaSubIDToString[i].psz))
         return gaSubIDToString[i].wMinor;

   return 0;

}

/**********************************************************************************
MySRand, MyRand - Personal random functions.
*/
static DWORD   gdwRandSeed;

void MySRand (DWORD dwVal)
{
   gdwRandSeed = dwVal;
}

void MySRand (void)
{
   FILETIME ft;
   GetSystemTimeAsFileTime (&ft);

   gdwRandSeed = GetTickCount() + ft.dwHighDateTime + ft.dwLowDateTime;
}

DWORD MyRand (void)
{
   gdwRandSeed = (gdwRandSeed ^ 0x34a892b2) * (gdwRandSeed ^ 0x39b87c8a) +
      (gdwRandSeed ^ 0x1893b78a);

   return gdwRandSeed;
}



/**********************************************************************************
HashString - Hash an E-mail (or other string) to a DWORD number. Use this as the
registration key.

inputs
   char  *psz
returns
   DWORD
*/
DWORD HashString (char *psz)
{
   DWORD dwSum;

   DWORD i;
   dwSum = 324233;
   for (i = 0; psz[i]; i++) {
      MySRand ((DWORD) CharLower((char*) MAKELONG(psz[i], 0)));
      MyRand ();
      dwSum += (DWORD) MyRand();
   }

   return dwSum;
}


/*****************************************************************************
Constructor & destructor */
CEncrypt::CEncrypt (void)
{
   // this space intentionally left blank
}

CEncrypt::~CEncrypt (void)
{
   // this space intentionally left blank
}

/****************************************************************************
Init - Initializes the encrypt object using the given key.
*/
void CEncrypt::Init (DWORD dwKey)
{
   DWORD i;

   MySRand (dwKey);
   for (i = 0; i < ENCRYPTSIZE; i++)
      m_adwEncrypt[i] = MyRand();
}


/****************************************************************************
GetValue - Given a DWORD index into a data stream, this returns the encryption
value that's xored with the data to encrypt it.

inputs
   DWORD    dwPosn - position (in DWORDs)
returns
   DWORD    dwValue - Value to xor
*/
DWORD CEncrypt::GetValue (DWORD dwPosn)
{
   DWORD dwLoc;
   dwLoc = (dwPosn * dwPosn) % ENCRYPTSIZE;
   return m_adwEncrypt[dwLoc];
}


/****************************************************************************
EncryptBuffer - Encrypts an entire buffer. Not that the values are in bytes,
   so encyrption can be on odd DWORD alignments

inputs
   PBYTE    *pData - Data to encrypt (in place)
   DWORD    dwSize - size (in bytes) of the data
   DWORD    dwStartPosn - start of the encryption position, used in GetValue().
               This is automatically converted to DWORDs, and odd alignments are
               dealt with
returns
   none
*/

void  CEncrypt::EncryptBuffer (PBYTE pData, DWORD dwSize, DWORD dwStartPosn)
{
   DWORD dwTemp, dwVal, dwValInv;

   // if the start position is not DWORD align then get it aligned.
   if (dwStartPosn % sizeof(DWORD)) {
      dwTemp = 0;
      dwVal = dwStartPosn % sizeof(DWORD);
      dwValInv = sizeof(DWORD) - dwVal;
      memcpy ((PBYTE) (&dwTemp) + dwVal, pData, min(dwValInv, dwSize));
      
      // encrypt
      dwTemp ^= GetValue ((dwStartPosn - dwVal) / sizeof(DWORD));

      // write back
      memcpy (pData, (PBYTE) (&dwTemp) + dwVal, min(dwValInv, dwSize));

      // adjust the offsets
      pData += min(dwValInv, dwSize);
      dwSize -= min(dwValInv, dwSize);
      dwStartPosn += dwValInv;
   }

   // loop through data
   for (; dwSize >= sizeof(DWORD); dwSize -= sizeof(DWORD), pData += sizeof(DWORD), dwStartPosn += sizeof(DWORD)) {
      DWORD *pdw;
      pdw = (DWORD*) pData;
      *pdw = *pdw ^ GetValue (dwStartPosn / sizeof(DWORD));
   }

   // if there's remaining then special case
   if (dwSize) {
      dwTemp = 0;
      memcpy (&dwTemp, pData, dwSize);
      dwTemp ^= GetValue (dwStartPosn / sizeof(DWORD));
      memcpy (pData, &dwTemp, dwSize);
   }

   // done

}



/*****************************************************************************
Constructor & destructor */
CChunk::CChunk (void)
{
   m_pData = NULL;
   m_paSubChunks = 0;
   ClearOut();
}

CChunk::~CChunk (void)
{
   ClearOut();

}

void CChunk::ClearOut (void)
{
   if (m_pData)
      free (m_pData);
   m_pData = NULL;

   if (m_paSubChunks) {
      DWORD i;
      for (i = 0; i < m_dwSubChunks; i++)
         delete m_paSubChunks[i];
      free (m_paSubChunks);
   }
   m_paSubChunks = NULL;

   m_dwID = 0;
   m_dwFlags = CHUNKFLAG_COMPRESSED;   // default to compressed for all chunks
   m_dwDataSize = NULL;
   m_dwEncoding = 0;
   m_dwSubChunks = 0;
   m_pChunkParent = NULL;
   m_fFileDirty = FALSE;
}


/***************************************************************************************
FileDirty - Sets the file dirty flag of the top-most parent. This is called by any
function that changes the data.
*/
void     CChunk::FileDirty (void)
{
   CChunk   *p;
   for (p = this; p->m_pChunkParent; p = p->m_pChunkParent);

   p->m_fFileDirty = TRUE;
}


/***************************************************************************************
SubChunkGet - Gets a chunk for the subchunk.

inputs
   DWORD    dwIndex - chunk index, from 0 .. ?
returns
   CChunk* - Pointer to chunk. Do not delete this. NULL if beyond end of chunks
*/
CChunk*  CChunk::SubChunkGet (DWORD dwIndex)
{
   if (!m_paSubChunks || (dwIndex >= m_dwSubChunks))
      return NULL;

   return m_paSubChunks[dwIndex];
}

/*************************************************************************************
SubChunkDelete - Deletes a subchunk and all its chunks

inputs
   DWORD    dwIndex - chunk index, from 0 .. ?
returns
   none
*/
VOID     CChunk::SubChunkDelete (DWORD dwIndex)
{
   if (!m_paSubChunks || (dwIndex >= m_dwSubChunks))
      return;

   delete m_paSubChunks[dwIndex];

   memmove (m_paSubChunks + dwIndex, m_paSubChunks + (dwIndex+1), (m_dwSubChunks - dwIndex - 1) * sizeof(CChunk*));
   m_dwSubChunks--;

   FileDirty ();
}


/*************************************************************************************
SubChunkInsert - Inserts a new (blank) chunk before the specified index number, and
then returns the chunk object.

inputs
   DWORD    dwIndex - chunk index
returns
   CChunk* - chunk pointer
*/

CChunk*  CChunk::SubChunkInsert (DWORD dwIndex)
{
   if (dwIndex > m_dwSubChunks)
      dwIndex = m_dwSubChunks;

   m_paSubChunks = (CChunk**) realloc (m_paSubChunks, (m_dwSubChunks+1) * sizeof(CChunk*));
   if (!m_paSubChunks)
      return NULL;

   memmove (m_paSubChunks + (dwIndex + 1), m_paSubChunks + dwIndex, m_dwSubChunks - dwIndex);
   m_dwSubChunks ++;

   m_paSubChunks[dwIndex] = new CChunk;
   if (m_paSubChunks[dwIndex])
      m_paSubChunks[dwIndex]->m_pChunkParent = this;

   FileDirty();

   return m_paSubChunks[dwIndex];
}

/*************************************************************************************
SubChunkAdd - Like Insert, but at the end

inputs
   DWORD    dwIndex - chunk index
returns
   CChunk* - chunk pointer
*/
CChunk*  CChunk::SubChunkAdd (void)
{
   return SubChunkInsert (m_dwSubChunks);
}

/*************************************************************************************
SubChunkFind - Searches sequentially through the subchunks to find one with the specified
   m_dwID

inputs
   DWORD    dwID - ID to look for
   DWORD    dwIndex - index to start at.
returns
   DWORD - Index found at, or -1 if cant find
*/
DWORD    CChunk::SubChunkFind (DWORD dwID, DWORD dwIndex)
{
   DWORD i;
   CChunk   *p;
   for (i = dwIndex; ; i++) {
      p = SubChunkGet (i);
      if (!p)
         return (DWORD) -1;
      if (p->m_dwID == dwID)
         return i;
   }

   return (DWORD) -1;
}

/*************************************************************************************
DataGet - Returns a pointer to the chunk's memory. Note that this pointer might change
if DataSet() is called. NOTE: do not change this memory because the system won't
know if it's been changed and won't recompress/encode the data.

inputs
   none
returns
   PVOID - memory. Might be NULL
*/
PVOID CChunk::DataGet (void)
{
   // IMPORTANT - if data was compressed or encoded the decompress & decode
   return m_pData;
}


/*************************************************************************************
DataSize - Returns the size of the data from DataGet.
*/
DWORD    CChunk::DataSize ()
{
   // IMPORTANT - if data was compressed or encoded the decompress & decode
   return m_dwDataSize;
}


/*************************************************************************************
DataSet - Writes new data into the chunks data.

inputs
   PVOID    pData - buffer to copy new data from
   DWORD    dwSize - number of bytes
*/
BOOL     CChunk::DataSet (PVOID pData, DWORD dwSize)
{
   if (m_pData)
      free (m_pData);
   m_pData = malloc (dwSize);
   if (!m_pData)
      return FALSE;

   memcpy (m_pData, pData, dwSize);
   m_dwDataSize = dwSize;

   FileDirty();

   return TRUE;
}


/*************************************************************************************
Write - Writes the chunk (and sub chunks) out to the file object.

inputs
   CEndFile *pFile - file object
returns
   BOOL - TRUE if OK, FALSE if error

TECHNICAL NOTE: Chunk Format...
   DWORD    dwSize - in bytes of the encoded/encrypted chunk data
   DWORD    dwID - chunk ID
   DWORD    dwCheckSum - Checksum of the chunk. So it the decrypt is pretty sure
               that the decryption worked
   DWORD    dwFlags - flags. Can be:
               CHUNKFLAG_COMPRESSED - If set then the chunk data is compressed
               CHUNKFLAG_ENCRYPTED - If set then the chunk data is encrypted.
   BYTE     abData[dwSize];
   DWORD    dwNumSubChunks - number of sub chunks
   contents of sub chunks, written out
*/
BOOL     CChunk::Write (CEncFile *pFile)
{
   // IMPORTANT - right now assume that not encoded or encrypted

   DWORD dw;
   PVOID pCompressed = NULL;
   DWORD dwCompressedSize;

   // compress if necessary
   if (m_dwFlags & CHUNKFLAG_COMPRESSED) {
      pCompressed = LZCompress (m_pData, m_dwDataSize, &dwCompressedSize);
      
      // if it's larger than the original then save uncompressed
      if (dwCompressedSize > m_dwDataSize) {
         free (pCompressed);
         pCompressed = NULL;
         m_dwFlags = m_dwFlags & (~CHUNKFLAG_COMPRESSED);
      }
   }

   // size
   dw = pCompressed ? dwCompressedSize : m_dwDataSize;
   if (!pFile->Write(&dw, sizeof(dw))) {
      if (pCompressed)
         free (pCompressed);
      return FALSE;
   }
   
   // ID
   dw = m_dwID;
   if (!pFile->Write(&dw, sizeof(dw))) {
      if (pCompressed)
         free (pCompressed);
      return FALSE;
   }
   
   // checksum
   // IMPORTANT - 0 for now
   dw = 0;
   if (!pFile->Write(&dw, sizeof(dw))) {
      if (pCompressed)
         free (pCompressed);
      return FALSE;
   }

   // flags
   dw = m_dwFlags;
   if (!pFile->Write(&dw, sizeof(dw))) {
      if (pCompressed)
         free (pCompressed);
      return FALSE;
   }

   // data
   if (!pFile->Write(pCompressed ? pCompressed : m_pData, pCompressed ? dwCompressedSize : m_dwDataSize)) {
      if (pCompressed)
         free (pCompressed);
      return FALSE;
   }
   if (pCompressed)
      free (pCompressed);

   // subchunks
   dw = m_dwSubChunks;
   if (!pFile->Write(&dw, sizeof(dw)))
      return FALSE;

   DWORD i;
   for (i = 0; i < m_dwSubChunks; i++)
      if (!m_paSubChunks[i]->Write (pFile))
         return FALSE;

   // all done
   return TRUE;
}

/*************************************************************************************
Read - Reads the chunk (and sub chunks) out to the file object. If any data was already
   in the chunk then it's deleted.

inputs
   CEndFile *pFile - file object
returns
   BOOL - TRUE if OK, FALSE if error
*/
BOOL     CChunk::Read (CEncFile *pFile)
{
   // IMPORTANT - right now assume that not encoded or encrypted

   DWORD dw;

   // clear out
   ClearOut ();

   // size
   if (!pFile->Read(&m_dwDataSize, sizeof(m_dwDataSize)))
      return FALSE;
   
   // ID
   if (!pFile->Read(&m_dwID, sizeof(m_dwID)))
      return FALSE;
   
   // checksum
   // IMPORTANT - ignoring for now
   if (!pFile->Read(&dw, sizeof(dw)))
      return FALSE;

   // flags
   if (!pFile->Read(&m_dwFlags, sizeof(m_dwFlags)))
      return FALSE;

   // data
   m_pData = malloc(m_dwDataSize);
   if (!m_pData)
      return FALSE;
   if (!pFile->Read(m_pData, m_dwDataSize))
      return FALSE;

   // if it's compressed then do something
   if (m_dwFlags & CHUNKFLAG_COMPRESSED) {
      PVOID pMem;
      DWORD dwSize;
      pMem = LZDecompress (m_pData, m_dwDataSize, &dwSize);
      if (m_pData)
         free (m_pData);
      m_pData = pMem;
      m_dwDataSize = dwSize;
   }

   // subchunks
   if (!pFile->Read(&m_dwSubChunks, sizeof(m_dwSubChunks)))
      return FALSE;
   m_paSubChunks = (CChunk**) malloc (m_dwSubChunks * sizeof(CChunk*));
   if (!m_paSubChunks)
      return FALSE;

   DWORD i;
   for (i = 0; i < m_dwSubChunks; i++) {
      m_paSubChunks[i] = new CChunk;
      if (!m_paSubChunks[i])
         return FALSE;
      if (!m_paSubChunks[i]->Read (pFile))
         return FALSE;
      m_paSubChunks[i]->m_pChunkParent = this;

   }

   // all done
   return TRUE;
}


/**********************************************************************************
CEncFile - functions
*/


/**********************************************************************************
Constructur & destructor */
CEncFile::CEncFile (void)
{
   m_f = NULL;
   m_pMem = NULL;
   m_dwMemSize = 0;
   m_dwMemCur = 0;
}

CEncFile::~CEncFile (void)
{
   Close ();
}

/**********************************************************************************
Open - opens a file for reading.

inputs
   char  *psz - file
returns
   BOOL - true if OK, FALSE if error
*/
BOOL CEncFile::Open (char *psz)
{

   if (m_f)
      return FALSE;  // already open

   m_f = fopen(psz, "rb");
   if (!m_f)
      return FALSE;

   // read in the header
   ENCHEADER   ec;
   if (1 != fread (&ec, sizeof(ec), 1, m_f)) {
      Close ();
      return FALSE;  // file not large enough
   }

   if (ec.dwMagic != MXAC) {
      Close ();
      return FALSE;  // wrong header
   }

   // encryption
   m_Encrypt.Init (ec.dwEncrypt);

   // that's it
   return TRUE;
}


/**********************************************************************************
Open - reads from memory instead of file

inputs
   void  *pMem - memopry
   DWORD dwSize - size
returns
   BOOL - true if OK, FALSE if error
*/
BOOL CEncFile::OpenMem (void *pMem, DWORD dwSize)
{
   if (m_f || m_pMem)
      return FALSE;  // already open

   m_pMem = pMem;
   m_dwMemSize = dwSize;
   m_dwMemCur = 0;

   // read in the header
   ENCHEADER   *pec;
   pec = (ENCHEADER*) pMem;
   m_dwMemCur = sizeof(ENCHEADER);

   if (pec->dwMagic != MXAC) {
      Close ();
      return FALSE;  // wrong header
   }

   // encryption
   m_Encrypt.Init (pec->dwEncrypt);

   // that's it
   return TRUE;
}


/**********************************************************************************
Create - Creates a file

inputs
   char  *psz - file
returns
   BOOL - true if ok, FALSE if error
*/
BOOL CEncFile::Create (char *psz)
{

   if (m_f)
      return FALSE;  // already open

   m_f = fopen(psz, "wb");
   if (!m_f)
      return FALSE;

   // write the header
   ENCHEADER   ec;
   ec.dwMagic = MXAC;
   MySRand();
   ec.dwEncrypt = MyRand();
   m_Encrypt.Init (ec.dwEncrypt);
   if (1 != fwrite (&ec, sizeof(ec), 1, m_f)) {
      Close ();
      return FALSE;  // file not large enough
   }

   // that's it
   return TRUE;
}

/******************************************************************************8
Close - closes the file
*/
BOOL CEncFile::Close (void)
{
   if (m_pMem)
      m_pMem = NULL;

   if (!m_f)
      return FALSE;

   fclose (m_f);
   m_f = NULL;

   return TRUE;
}

/******************************************************************************8
Write - Writes out the specfied data. If encryption is on it encrypts.

inputs
   PVOID pData - dat
   DWORD dwSize - size
returns
   BOOL - true if ok, FALSE if failed
*/
BOOL CEncFile::Write (PVOID pData, DWORD dwBytes)
{
   if (!m_f)
      return FALSE;

   // get the position
   DWORD dwPos;
   dwPos = ftell (m_f);

   // encrypt
   PVOID pTemp;
   pTemp = malloc(dwBytes);
   if (!pTemp)
      return FALSE;
   memcpy (pTemp, pData, dwBytes);
   m_Encrypt.EncryptBuffer ((LPBYTE) pTemp, dwBytes, dwPos);

   if (dwBytes != fwrite (pTemp, 1, dwBytes, m_f)) {
      free (pTemp);
      return FALSE;
   }

   free (pTemp);

   return TRUE;
}

/******************************************************************************8
Read - Reads in the specfied data. If encryption is on it decrypts.

inputs
   PVOID pData - dat
   DWORD dwSize - size
returns
   BOOL - true if ok, FALSE if failed
*/
BOOL CEncFile::Read (PVOID pData, DWORD dwBytes)
{
   if (!m_f && !m_pMem)
      return FALSE;

   // get the position
   DWORD dwPos;
   dwPos = m_pMem ? m_dwMemCur : ftell (m_f);

   if (m_f) {
      if (dwBytes != fread (pData, 1, dwBytes, m_f))
         return FALSE;
   }
   else {
      if (dwBytes + m_dwMemCur > m_dwMemSize)
         return FALSE;
      memcpy (pData, (PBYTE) m_pMem + m_dwMemCur, dwBytes);
      m_dwMemCur += dwBytes;
   }

   // encrypt
   m_Encrypt.EncryptBuffer ((LPBYTE) pData, dwBytes, dwPos);

   return TRUE;
}

/******************************************************************************8
Seek - Seeks to the position within the file. Right after the header is location 0

inputs
   DWORD    dwLoc - location
returns
   BOOL - TRUE if ok, FALSE if error
*/
BOOL CEncFile::Seek (DWORD dwLoc)
{
   if (m_pMem) {
      m_dwMemCur = dwLoc;
      if (m_dwMemCur > m_dwMemSize)
         m_dwMemCur = m_dwMemSize;
      return TRUE;
   }

   if (!m_f)
      return FALSE;

   return !fseek(m_f, dwLoc + sizeof(ENCHEADER), SEEK_SET);
}


/******************************************************************************8
Tell - Returns the location in the file (excluding the header structure)
*/
DWORD CEncFile::Tell (void)
{
   if (m_pMem)
      return m_dwMemCur - sizeof(ENCHEADER);

   if (!m_f)
      return FALSE;
   
   return (DWORD) ftell (m_f) - sizeof(ENCHEADER);
}


/****************************************************************************8
BitRead - Takes memory and reads N bits from the memory.

inputs
   PVOID    pMem - memory
   DWORD    dwStart - Bit number (0 = start of mem) where to read from
   DWORD    dwBits - Number of bits to read. Max 32.
returns
   DWORD - Bits read. Filling dwBits worth (from low-order bit) up
*/
DWORD BitRead (PVOID pMem, DWORD dwStart, DWORD dwBits)
{
   DWORD    *pdw = (DWORD*) pMem;
   DWORD    dwVal = 0;

   // jump forward by DWORDs
   pdw += (dwStart / 32);
   dwStart = dwStart % 32;

   // if it all fits in the same DWORD, do one thing
   if (dwStart + dwBits <= 32) {
      dwVal = pdw[0] >> dwStart;
   }
   else {
      // else, 2 dwords
      dwVal = (pdw[0] >> dwStart) | (pdw[1] << (32 - dwStart));
   }

   // mask it off
   if (dwBits < 32)
      dwVal = dwVal & ((1 << dwBits) - 1);

   return dwVal;
}


/****************************************************************************8
BitWrite - Appends N bits onto memory.

inputs
   PVOID    *ppMem - Pointer to a PVOID with the memory allocated with malloc.
                        Need this because the memory may need to be realloced,
                        in which case this value will change.
   DWORD    *pdwAlloc - Pointer to a DWORD containing the number of bytes
                        allocated in *ppMem. If data is realloced then this
                        will be increased.
   DWORD    dwStart - Location (in bits) to write to
   DWORD    dwBits - Number of bits to write (max 32)
   DWORD    dwVal - Value to write. The non-valid high bits must be 0.
returns
   none
*/
void BitWrite (PVOID *ppMem, DWORD *pdwAlloc, DWORD dwStart, DWORD dwBits, DWORD dwVal)
{
   // make sure the memory is large enough
   DWORD dwNeed;
   dwNeed = (dwStart + dwBits + 32) / 8;
   if (*pdwAlloc < dwNeed) {
      dwNeed += 256; // just to do a block at a time
      *ppMem = realloc(*ppMem, dwNeed);
      *pdwAlloc = dwNeed;
      if (!*ppMem)
         return;  // out of memory
   }

   // find the right place to add
   DWORD *pdw;
   pdw = (DWORD*) (*ppMem);
   pdw += (dwStart / 32);
   dwStart = dwStart % 32;

   DWORD dwMask;
   if (dwStart + dwBits <= 32) {
      // it all fits in this DWORD
      if (dwStart + dwBits == 32)
         dwMask = (DWORD) -1;
      else
         dwMask = ((1 << (dwStart + dwBits)) - 1);
      dwMask = dwMask & ~((1 << dwStart) - 1);

      pdw[0] = (pdw[0] & ~dwMask) | (dwVal << dwStart);
   }
   else {
      // it takes 2 DWORDS

      // the first mask is all high
      dwMask = (DWORD)-1 & ~((1 << dwStart) - 1);
      pdw[0] = (pdw[0] & ~dwMask) | (dwVal << dwStart);

      // the second mask is all low
      dwMask = (1 << (dwStart + dwBits - 32)) - 1;
      pdw[1] = (pdw[1] & ~dwMask) | (dwVal >> (32 - dwStart));
   }
}

/****************************************************************************8
LZDeconstructByteType - Desconstructs the byte table, converting tokens into bytes.

inputs
   PDWORD   pdwMap - an array of 8 DWORDS from the beginning of the LZ data.
   PBYTE    pbMap - Filled with a conversion from the token to the byte value.
               value = pbMap[token]
returns
   DWORD - number of different values actually seen.
*/
DWORD LZDeconstructByteTable (PDWORD pdwMap, PBYTE pbMap)
{
   memset (pbMap, 0, sizeof(pbMap));

   DWORD dwCount = 0;
   DWORD i, j, dw;
   for (j = 0; j < 8; j++) {
      dw = pdwMap[j];

      for (i = 0; i < 32; i++, dw >>= 1)
         if (dw & 0x01)
            pbMap[dwCount++] = (BYTE) (i + j * 32);
   }

   return dwCount;
}

/****************************************************************************8
LZConstructByteTable - Constructs a table indicating which bytes get mapped
   to which tokens. (For compression.) It does thsi by scanning through the document
   and learning how many of the 256 bytes are used. This leads to filling in a mapping
   table of byte -> token.

inputs
   PBYTE    pMem - memory to compress
   DWORD    dwSize - number of chars
   PBYTE    pbMap - an array of 256 bytes. Will be filled in with a mapping number
                     from 0..256, for what it gets mapped to. Unusued chars are set to 0.
   PDWORD   pdwMap - an array of 8 DWORDS. Filled with bit-values indicating if a particular
                     byte is used. pdwMap[1], bit 1 being on means char 33 is seen in pMem
returns
   DWORD - Number of different values actually seen.
*/
DWORD LZConstructByteTable (PBYTE pMem, DWORD dwSize, PBYTE pbMap, PDWORD pdwMap)
{
   memset (pbMap, 0, 256);
   memset (pdwMap, 0, 8 * sizeof(DWORD));

   // note which values appear
   DWORD i, j;
   for (i = 0; i < dwSize; i++, pMem++)
      pbMap[*pMem] = 1;

   // create the bit fields
   DWORD dwVal;
   DWORD dwCount;
   dwCount = 0;
   for (j = 0; j < 8; j++) {
      dwVal = 0;

      for (i = 31; i < 32; i--) {
         dwVal <<= 1;
         if (pbMap[i + j * 32])
            dwVal |= 1;
      }

      pdwMap[j] = dwVal;
   }

   // do conversion
   for (i = 0; i < 256; i++)
      if (pbMap[i])
         pbMap[i] = (BYTE) (dwCount++);

   // done
   return dwCount;
}

/****************************************************************************8
LZNumValuesToBits - Given the number of values seen in LZConstructByteTable,
   this returns the number of bits needed to represent them.

inputs
   DWORD dwSeen
returns
   DWORD - bits
*/
DWORD LZNumValuesToBits (DWORD dwSeen)
{
   DWORD dwBits;
   for (dwBits = 0; dwSeen; dwSeen >>= 1, dwBits++);

   return dwBits;
}

//#define  LZNEWBITS      6
#define  LZNEWBITS      6
#define  LZMAXNEW       ((1 << LZNEWBITS) - 1)

//#define  LZREPEATBITS   4     // number of bits to store repeat amount
#define  LZREPEATBITS   4     // number of bits to store repeat amount
#define  LZREPEATMIN    4     // don't bother doing a repeat unless it's this many chars
#define  LZREPEATMAX    ((1 << LZREPEATBITS) + LZREPEATMIN - 1)   // max number that can repeat

//#define  LZAGEBITS      12    // maximum age (# of chars past) that can be stored for repeats
#define  LZAGEBITS      15    // maximum age (# of chars past) that can be stored for repeats
#define  LZAGEMAX       ((1 << LZAGEBITS) - 1)

// NEW=6,REPEAT=4,AGE=12 => ffinstall=1004

// NEW=7,REPEAT=4,AGE=12 => 1008k
// NEW=5,REPEAT=4,AGE=12 => ffinstall=1004
// NEW=4,REPEAT=4,AGE=12 => ffinstall=1008

// NEW=6,REPEAT=5,AGE=12 => ffinstall=1004
// NEW=6,REPEAT=6,AGE=12 => ffinstall=1012
// NEW=6,REPEAT=3,AGE=12 => ffinstall=1016

// NEW=6,REPEAT=4,AGE=11 => ffinstall=1028
// NEW=6,REPEAT=4,AGE=13 => ffinstall=988 // 16k savings
// NEW=6,REPEAT=4,AGE=14 => ffinstall=968 // 36k savings
// NEW=6,REPEAT=4,AGE=15 => ffinstall=924 // 80k savings - very slow <--- use this
// NEW=6,REPEAT=5,AGE=14 => ffinstall=968 // same as repeat=4
// NEW=6,REPEAT=4,AGE=15, smaller flyfox.exe => ffinstall=900 // 104k savings - very slow <--- use this

/****************************************************************************8
LZDecompReadNewBytes - Read in compressed data. The data is "new" bytes, meaning
   that it hasnt' appeared in the datastream before.

inputs
   PVOID    pComp - compressed memory
   DWORD    *pdwCompPosn - position within the compressed memory, in bits.
               This will be incremented
   PBYTE    *ppDecomp - decompressed memory. may be changed by call to realloc
   DWORD    *pdwDecompSize - amount allocated for ppDecomp. May be changed
   DWORD    *pdwDecompPosn - Position writing to in the decomp locaton. May be changed
   BYTE     *pabMap - Map from toke to BYTE
   DWORD    dwBitsPerToken - Bits per token

returns
   BOOL - TRUE if OK. FALSE if EOF found
*/
BOOL LZDecompressReadNewBytes (PVOID pComp, DWORD *pdwCompPosn, PBYTE *ppDecomp,
                               DWORD *pdwDecompSize, DWORD *pdwDecompPosn,
                               BYTE *pabMap, DWORD dwBitsPerToken)
{
   // assume that the first bit, containing 1, has already been read.

   // read in the number of tokens following
   DWORD dwNum;
   dwNum = BitRead (pComp, *pdwCompPosn, LZNEWBITS);
   *pdwCompPosn = *pdwCompPosn + LZNEWBITS;

   if (!dwNum)
      return FALSE;  // no more

   // make sure the memory is large enough for new values
   if (*pdwDecompPosn + dwNum >= *pdwDecompSize) {
      *pdwDecompSize = *pdwDecompPosn + dwNum + 256;
      *ppDecomp = (PBYTE) realloc (*ppDecomp, *pdwDecompSize);
      if (!*ppDecomp)
         return FALSE;
   }

   // read the tokens
   DWORD i, dw;
   for (i = 0; i < dwNum; i++) {
      dw = BitRead (pComp, *pdwCompPosn, dwBitsPerToken);
      *pdwCompPosn = *pdwCompPosn + dwBitsPerToken;

      (*ppDecomp)[*pdwDecompPosn] = pabMap[dw];
      *pdwDecompPosn = *pdwDecompPosn + 1;
   }
   
   // done
   return TRUE;
}

/****************************************************************************8
LZDecompReadRepeat - Read in compressed data. The data is repeated data.

inputs
   PVOID    pComp - compressed memory
   DWORD    *pdwCompPosn - position within the compressed memory, in bits.
               This will be incremented
   PBYTE    *ppDecomp - decompressed memory. may be changed by call to realloc
   DWORD    *pdwDecompSize - amount allocated for ppDecomp. May be changed
   DWORD    *pdwDecompPosn - Position writing to in the decomp locaton. May be changed

returns
   BOOL - TRUE if OK. FALSE if EOF found
*/
BOOL LZDecompressReadRepeat (PVOID pComp, DWORD *pdwCompPosn, PBYTE *ppDecomp,
                               DWORD *pdwDecompSize, DWORD *pdwDecompPosn)
{
   // assume that the first bit, containing 0, has already been read.

   // read in the number of tokens following
   DWORD dwNum;
   dwNum = BitRead (pComp, *pdwCompPosn, LZREPEATBITS);
   *pdwCompPosn = *pdwCompPosn + LZREPEATBITS;
   dwNum += LZREPEATMIN;

   // read in the age
   DWORD dwAge;
   dwAge = BitRead (pComp, *pdwCompPosn, LZAGEBITS);
   *pdwCompPosn = *pdwCompPosn + LZAGEBITS;

   // make sure the memory is large enough for new values
   if (*pdwDecompPosn + dwNum >= *pdwDecompSize) {
      *pdwDecompSize = *pdwDecompPosn + dwNum + 256;
      *ppDecomp = (PBYTE) realloc (*ppDecomp, *pdwDecompSize);
      if (!*ppDecomp)
         return FALSE;
   }

   // copy
   memmove (*ppDecomp + *pdwDecompPosn, *ppDecomp + (*pdwDecompPosn - dwAge), dwNum);
   *pdwDecompPosn = *pdwDecompPosn + dwNum;

   // done
   return TRUE;
}

/****************************************************************************8
LZCompLookForRepeat - Look for a repeat up to LZAGEMAX bytes previous. If find a repeat
   then see how long it is. Remember if >= LZREPEATMIN. Then, continue, looking for
   the longest repeat

inputs
   PBYTE    pSrc - source to look through
   DWORD    dwSize - number of bytes of data in source
   DWORD    dwCur - current position. Storing from that byte ownward
   DWORD    *pdwLargestRepeat - Filled with the largest repeat
returns
   DWORD    - repeat age found. or 0 if none long enough found
*/
DWORD LZCompLookForRepeat (PBYTE pSrc, DWORD dwSize, DWORD dwCur, DWORD *pdwLargestRepeat)
{
   // remember the largest repeat
   DWORD dwLargestCount = LZREPEATMIN-1;
   DWORD dwLargestAge = 0  ;// not found

   // look
   DWORD i;
   int   iStart;
   for (i = LZREPEATMIN; i <= LZAGEMAX; i++) {
      // position within file
      iStart = (int) dwCur - i;
      if (iStart < 0)
         break;   // out of bounds

      // trivial reject
      if (pSrc[dwCur] != pSrc[iStart])
         continue;

      // longer test
      DWORD dwRepeat;
      int   iCur;
      for (dwRepeat = 1; dwRepeat < LZREPEATMAX; dwRepeat++) {
         // don't go past end
         iCur = iStart + (int) dwRepeat;
         if (iCur >= (int) dwCur)
            break;   // limit of size

         // see if matches
         if (pSrc[dwCur+dwRepeat] != pSrc[iCur])
            break;
      }

      // how far did the repeat get
      if (dwRepeat <= dwLargestCount)
         continue;   // not large enough to matter

      // else, new length
      dwLargestCount = dwRepeat;
      dwLargestAge = dwCur - (DWORD) iStart;
   }

   *pdwLargestRepeat = dwLargestCount;
   return dwLargestAge;
}

/****************************************************************************8
LZCompWriteRepeat - Write in compressed data. Note that the data is a marker
   that previous data that has appeared was repeated.

inputs
   DWORD    dwCount - Repeat count
   DWORD    dwAge - Age of the repeat
   PVOID    *ppMem - To write compressed data into. May be changed on relloc
   DWORD    *pdwMemSize - Size of memory in bytes. May be increased
   DWORD    *pdwCurBit - Current bit writing to. Will be increased.
returns
   none
*/
void LZCompWriteRepeat (DWORD dwCount, DWORD dwAge, PVOID *ppMem,
                          DWORD *pdwMemSize, DWORD *pdwCurBit)
{
   // write a 0 bit indicates a repeat
   // of previous data.
   BitWrite (ppMem, pdwMemSize, *pdwCurBit, 1, 0);
   *pdwCurBit = *pdwCurBit + 1;

   // write out the number of bytes data represented
   BitWrite (ppMem, pdwMemSize, *pdwCurBit, LZREPEATBITS, dwCount - LZREPEATMIN);
   *pdwCurBit = *pdwCurBit + LZREPEATBITS;

   // write the number of bytes age
   BitWrite (ppMem, pdwMemSize, *pdwCurBit, LZAGEBITS, dwAge);
   *pdwCurBit = *pdwCurBit + LZAGEBITS;

   // done
}



/****************************************************************************8
LZCompWriteNewBytes - Write in compressed data. Note that the data is "new"
   bytes, meaning they haven't appeared in the datastream before.

inputs
   PBYTE    pabWrte - to write
   DWORD    dwWriteSize - number of bytes. This is limited by LZMAXNEW.
            If this is 0 then EOF is written.
   PVOID    *ppMem - To write compressed data into. May be changed on relloc
   DWORD    *pdwMemSize - Size of memory in bytes. May be increased
   DWORD    *pdwCurBit - Current bit writing to. Will be increased.
   BYTE     *pabMap - Map from BYTE to token
   DWORD    dwBitsPerToken - Bits per token
returns
   none
*/
void LZCompWriteNewBytes (PBYTE pabWrite, DWORD dwWriteSize, PVOID *ppMem,
                          DWORD *pdwMemSize, DWORD *pdwCurBit,
                          BYTE *pabMap, DWORD dwBitsPerToken)
{
   // write a 1 bit indicating that it's new bytes. a 0 bit indicates a repeat
   // of previous data.
   BitWrite (ppMem, pdwMemSize, *pdwCurBit, 1, 1);
   *pdwCurBit = *pdwCurBit + 1;

   // write out the number of bytes data represented
   BitWrite (ppMem, pdwMemSize, *pdwCurBit, LZNEWBITS, dwWriteSize);
   *pdwCurBit = *pdwCurBit + LZNEWBITS;

   // write other data
   DWORD i;
   for (i = 0; i < dwWriteSize; i++) {
      BitWrite (ppMem, pdwMemSize, *pdwCurBit, dwBitsPerToken,
         pabMap[pabWrite[i]]);
      *pdwCurBit = *pdwCurBit + dwBitsPerToken;
   }

   // done
}

#define  COMPRESSCHUNK     4096        // use this for calculating # of bits

/****************************************************************************8
LZDecompress - Reads in compressed data and decompressed

inputs
   PVOID    pComp - compressed memory to decompress
   DWORD    dwCompSize - size in bytes of compressed memory
   DWORD    *pdwDecompSize - filled with the size of the decompressed memry
returns
   PVOID - filled with decompressed memory
*/
PVOID LZDecompress (PVOID pComp, DWORD dwCompSize, DWORD *pdwDecompSize)
{
   PVOID pNew;
   // if it's less than the minimum size can manage then just write out
   if (dwCompSize <= 8 * sizeof(DWORD)) {
      *pdwDecompSize = dwCompSize;
      pNew = malloc (dwCompSize);
      if (pNew)
         memcpy (pNew, pComp, dwCompSize);
      return pNew;
   }

   // else data to decompress
   DWORD dwCompCur;
   dwCompCur = 0;

   // load in the byte/token table
   DWORD i, dwBitsPerToken, dwCount;
   DWORD adwMap[8];
   BYTE  abMap[256];

   // memory
   PVOID pDecomp;
   DWORD dwDecompSize, dwDecompCur;
   dwDecompSize = dwCompSize * 2;
   dwDecompCur = 0;
   pDecomp = malloc (dwDecompSize);
   if (!pDecomp)
      return NULL;

   // go for it
   DWORD dw;
   while (TRUE) { // read in a chunk - about 2K

      // if not enough space for header then stop
      if (dwCompCur + 256 > dwCompSize * 8)
         break;

      // header of the chunk is 8*32 = 256 bits
      for (i = 0; i < 8; i++) {
         adwMap[i] = BitRead (pComp, dwCompCur, 32);
         dwCompCur += 32;
      }
      dwCount = LZDeconstructByteTable (adwMap, abMap);
      dwBitsPerToken = LZNumValuesToBits (dwCount);

      // and the data
      while (TRUE) {
         // read in a bit
         dw = BitRead (pComp, dwCompCur, 1);
         dwCompCur++;

         if (dw) {
            // its 1, which means new byte pattern
            if (!LZDecompressReadNewBytes (pComp, &dwCompCur, (PBYTE*) &pDecomp,
                                     &dwDecompSize, &dwDecompCur,
                                     abMap, dwBitsPerToken))
                                     break;  // no more data
         }
         else {
            // it's 0, wheich means repeat
            LZDecompressReadRepeat (pComp, &dwCompCur, (PBYTE*) &pDecomp,
                                     &dwDecompSize, &dwDecompCur);
         }

      }
   }

   // all done
   *pdwDecompSize = dwDecompCur;
   return pDecomp;

}

/****************************************************************************8
LZCompress - Compresses data.

inputs
   PVOID    pMem - to compress
   DWORD    dwSize - in bytes
   DWORD    *pdwCompressSize - Filled with the size (in bytes) of compressed data
returns
   PVOID - Memory that compressed to. Must be free()-ed.
*/
PVOID LZCompress (PVOID pMem, DWORD dwSize, DWORD *pdwCompressSize)
{
   PVOID pNew;
   // if it's less than the minimum size can manage then just write out
   if (dwSize <= 8 * sizeof(DWORD)) {
      *pdwCompressSize = dwSize;
      pNew = malloc (dwSize);
      if (pNew)
         memcpy (pNew, pMem, dwSize);
      return pNew;
   }

   // construct the byte table
   DWORD dwCount, dwBitsPerToken;
   BYTE  abMap[256];
   DWORD adwMapBits[8];

   // start out with blank memory
   DWORD dwNewSize;
   DWORD dwCurBit;
   dwNewSize = dwSize / 2; // as a rough guestimate
   pNew = malloc (dwNewSize);
   dwCurBit = 0;

   // BUGFIX - Split into smaller chunks to maximize advantage of byte table
   DWORD    dwChunkOffset;
   for (dwChunkOffset = 0; dwChunkOffset < dwSize; dwChunkOffset += COMPRESSCHUNK) {

      // sise of chunke
      DWORD dwSizeChunk = min(COMPRESSCHUNK, dwSize - dwChunkOffset);

      // calculate the bytes used in this chunk
      dwCount = LZConstructByteTable ((PBYTE) pMem + dwChunkOffset, dwSizeChunk, abMap, adwMapBits);
      dwBitsPerToken = LZNumValuesToBits (dwCount);

      // write it out
      DWORD i;
      for (i = 0; i < 8; i++) {
         BitWrite (&pNew, &dwNewSize, dwCurBit, 32, adwMapBits[i]);
         dwCurBit += 32;
      }

      // compress
      DWORD dwCurByte;
      for (dwCurByte = 0; dwCurByte < dwSizeChunk; ) {
         // see how far can go before find a repeat to use
         DWORD i;
         DWORD dwAge, dwRepeat;
         for (i = 0; i < LZMAXNEW; i++) {
            dwAge = 0;
            if (dwCurByte + i >= dwSizeChunk)
               break;   // can't go further

            dwAge = LZCompLookForRepeat ((PBYTE) pMem, dwChunkOffset + dwSizeChunk, dwChunkOffset + dwCurByte + i, &dwRepeat);
            if (dwAge)
               break;   // found
         }
      
         // write out the non-repeated data
         if (i) {
            LZCompWriteNewBytes ((PBYTE) pMem + dwChunkOffset + dwCurByte, i,
               &pNew, &dwNewSize, &dwCurBit,
               abMap, dwBitsPerToken);

            dwCurByte += i;
         }

         // if we have a repeat then write that out
         if (dwAge) {
            LZCompWriteRepeat (dwRepeat, dwAge,
               &pNew, &dwNewSize, &dwCurBit);

            dwCurByte += dwRepeat;
         }

      }


      // terminating
      LZCompWriteNewBytes ((PBYTE) pMem + dwChunkOffset + dwCurByte, 0,
         &pNew, &dwNewSize, &dwCurBit,
         abMap, dwBitsPerToken);
   }


   // done
   *pdwCompressSize = (dwCurBit + 7) / 8;
   return pNew;
}
