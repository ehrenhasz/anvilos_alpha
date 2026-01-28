

#if !defined(XMACROS_H)
#define XMACROS_H


#if defined(__hpux) || defined(VMS)
# ifndef SIZE_MAX
#  define SIZE_MAX ((size_t)(-1))
# endif
#endif

#define XDL_MIN(a, b) ((a) < (b) ? (a): (b))
#define XDL_MAX(a, b) ((a) > (b) ? (a): (b))
#define XDL_ABS(v) ((v) >= 0 ? (v): -(v))
#define XDL_ISDIGIT(c) ((c) >= '0' && (c) <= '9')
#define XDL_ISSPACE(c) (isspace((unsigned char)(c)))
#define XDL_ADDBITS(v,b)	((v) + ((v) >> (b)))
#define XDL_MASKBITS(b)		((1UL << (b)) - 1)
#define XDL_HASHLONG(v,b)	(XDL_ADDBITS((unsigned long)(v), b) & XDL_MASKBITS(b))
#define XDL_LE32_PUT(p, v) \
do { \
	unsigned char *__p = (unsigned char *) (p); \
	*__p++ = (unsigned char) (v); \
	*__p++ = (unsigned char) ((v) >> 8); \
	*__p++ = (unsigned char) ((v) >> 16); \
	*__p = (unsigned char) ((v) >> 24); \
} while (0)
#define XDL_LE32_GET(p, v) \
do { \
	unsigned char const *__p = (unsigned char const *) (p); \
	(v) = (unsigned long) __p[0] | ((unsigned long) __p[1]) << 8 | \
		((unsigned long) __p[2]) << 16 | ((unsigned long) __p[3]) << 24; \
} while (0)


#define XDL_ALLOC_ARRAY(p, nr)				\
	((p) = SIZE_MAX / sizeof(*(p)) >= (size_t)(nr)	\
		? xdl_malloc((nr) * sizeof(*(p)))	\
		: NULL)


#define XDL_CALLOC_ARRAY(p, nr)	((p) = xdl_calloc(nr, sizeof(*(p))))


#define XDL_ALLOC_GROW(p, nr, alloc)	\
	(-!((nr) <= (alloc) ||		\
	    ((p) = xdl_alloc_grow_helper((p), (nr), &(alloc), sizeof(*(p))))))

#endif 
