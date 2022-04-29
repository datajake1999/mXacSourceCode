/* mymalloc.h - debugging*/

#ifndef _MYMALLOC_H
#define _MYMALLOC_H

#ifdef _DEBUG

void *MyMalloc (unsigned int iSize);
void MyFree (void *pMem);
void *MyRealloc (void *pMem, unsigned int iSize);

#define MYMALLOC(x)     MyMalloc(x)
#define MYREALLOC(x,y)  MyRealloc(x,y)
#define MYFREE(x)      MyFree(x)

#else // release

#define MYMALLOC(x)     malloc(x)
#define MYREALLOC(x,y)  realloc(x,y)
#define MYFREE(x)      free(x)

#endif

#endif
