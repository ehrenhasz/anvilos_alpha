 

 

#if !defined (_XMALLOC_H_)
#define _XMALLOC_H_

#if defined (READLINE_LIBRARY)
#  include "rlstdc.h"
#else
#  include <readline/rlstdc.h>
#endif

#ifndef PTR_T

#ifdef __STDC__
#  define PTR_T	void *
#else
#  define PTR_T	char *
#endif

#endif  

extern PTR_T xmalloc (size_t);
extern PTR_T xrealloc (void *, size_t);
extern void xfree (void *);

#endif  
