 

 

#if !defined (_OCACHE_H_)
#define	_OCACHE_H_ 1

#ifndef PTR_T

#if defined (__STDC__)
#  define PTR_T void *
#else
#  define PTR_T char *
#endif

#endif  

#define OC_MEMSET(memp, xch, nbytes)					\
do {									\
  if ((nbytes) <= 32) {							\
    register char * mzp = (char *)(memp);				\
    unsigned long mctmp = (nbytes);					\
    register long mcn;							\
    if (mctmp < 8) mcn = 0; else { mcn = (mctmp-1)/8; mctmp &= 7; }	\
    switch (mctmp) {							\
      case 0: for(;;) { *mzp++ = xch;					\
      case 7:	   *mzp++ = xch;					\
      case 6:	   *mzp++ = xch;					\
      case 5:	   *mzp++ = xch;					\
      case 4:	   *mzp++ = xch;					\
      case 3:	   *mzp++ = xch;					\
      case 2:	   *mzp++ = xch;					\
      case 1:	   *mzp++ = xch; if(mcn <= 0) break; mcn--; }		\
    }									\
  } else								\
    memset ((memp), (xch), (nbytes));					\
} while(0)

typedef struct objcache {
	PTR_T	data;
	int	cs;		 
	int	nc;		 
} sh_obj_cache_t;

 
#define ocache_create(c, otype, n) \
	do { \
		(c).data = xmalloc((n) * sizeof (otype *)); \
		(c).cs = (n); \
		(c).nc = 0; \
	} while (0)

 
#define ocache_destroy(c) \
	do { \
		if ((c).data) \
			xfree ((c).data); \
		(c).data = 0; \
		(c).cs = (c).nc = 0; \
	} while (0)

 
#define ocache_flush(c, otype) \
	do { \
		while ((c).nc > 0) \
			xfree (((otype **)((c).data))[--(c).nc]); \
	} while (0)

 
#define ocache_alloc(c, otype, r) \
	do { \
		if ((c).nc > 0) { \
			(r) = (otype *)((otype **)((c).data))[--(c).nc]; \
		} else \
			(r) = (otype *)xmalloc (sizeof (otype)); \
	} while (0)

 
#define ocache_free(c, otype, r) \
	do { \
		if ((c).nc < (c).cs) { \
			OC_MEMSET ((r), 0xdf, sizeof(otype)); \
			((otype **)((c).data))[(c).nc++] = (r); \
		} else \
			xfree (r); \
	} while (0)

 

#endif  
