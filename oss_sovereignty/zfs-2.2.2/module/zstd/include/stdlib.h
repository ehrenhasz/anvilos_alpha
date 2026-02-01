 

 

#ifndef	_ZSTD_STDLIB_H
#define	_ZSTD_STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#undef	GCC_VERSION

 
#define	calloc(n, sz)	NULL
#define	malloc(sz)	NULL
#define	free(ptr)

#ifdef __cplusplus
}
#endif

#endif  
