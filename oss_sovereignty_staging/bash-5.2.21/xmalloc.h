 

 

#if !defined (_XMALLOC_H_)
#define _XMALLOC_H_

#include "stdc.h"
#include "bashansi.h"

 
#ifndef PTR_T

#if defined (__STDC__)
#  define PTR_T	void *
#else
#  define PTR_T char *
#endif

#endif  

 
extern PTR_T xmalloc PARAMS((size_t));
extern PTR_T xrealloc PARAMS((void *, size_t));
extern void xfree PARAMS((void *));

#if defined(USING_BASH_MALLOC) && !defined (DISABLE_MALLOC_WRAPPERS)
extern PTR_T sh_xmalloc PARAMS((size_t, const char *, int));
extern PTR_T sh_xrealloc PARAMS((void *, size_t, const char *, int));
extern void sh_xfree PARAMS((void *, const char *, int));

#define xmalloc(x)	sh_xmalloc((x), __FILE__, __LINE__)
#define xrealloc(x, n)	sh_xrealloc((x), (n), __FILE__, __LINE__)
#define xfree(x)	sh_xfree((x), __FILE__, __LINE__)

#ifdef free
#undef free
#endif
#define free(x)		sh_xfree((x), __FILE__, __LINE__)

extern PTR_T sh_malloc PARAMS((size_t, const char *, int));

#ifdef malloc
#undef malloc
#endif
#define malloc(x)	sh_malloc((x), __FILE__, __LINE__)

#endif	 

#endif	 
