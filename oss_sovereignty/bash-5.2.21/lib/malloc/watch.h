 

 

#ifndef _MWATCH_H
#define _MWATCH_H

#include "imalloc.h"

#ifdef MALLOC_WATCH

 

#define W_ALLOC		0x01
#define W_FREE		0x02
#define W_REALLOC	0x04
#define W_RESIZED	0x08

extern int _malloc_nwatch;

extern void _malloc_ckwatch PARAMS((PTR_T, const char *, int, int, unsigned long));
                    
#endif  

#endif  
