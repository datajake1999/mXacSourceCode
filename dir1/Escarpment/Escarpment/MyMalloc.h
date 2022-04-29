/* mymalloc.h - debugging*/

#ifndef _MYMALLOC_H
#define _MYMALLOC_H

#if 0 // replaced by ESCMALLOC
#ifdef _DEBUG

void *MyMalloc (unsigned int iSize);
void MyFree (void *pMem);
void *MyRealloc (void *pMem, unsigned int iSize);

#define MYMALLOC(x)     MyMalloc(x)
#define MYREALLOC(x,y)  MyRealloc(x,y)
#define MYFREE(x)       MyFree(x)

#else // release

#define MYMALLOC(x)     EscMalloc(x)
#define MYREALLOC(x,y)  EscRealloc(x,y)
#define Zone(x)      EscFree(x)

#endif
#endif // 0

#endif
