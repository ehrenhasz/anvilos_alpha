 
 

#ifndef _SH_MALLOC_H
#define _SH_MALLOC_H

#ifndef PARAMS
#  if defined (__STDC__) || defined (__GNUC__) || defined (__cplusplus)
#    define PARAMS(protos) protos
#  else
#    define PARAMS(protos) ()
#  endif
#endif

 
#ifndef PTR_T

#if defined (__STDC__)
#  define PTR_T void *
#else
#  define PTR_T char *
#endif

#endif  


extern PTR_T sh_malloc PARAMS((size_t, const char *, int));
extern PTR_T sh_realloc PARAMS((PTR_T, size_t, const char *, int));
extern void sh_free PARAMS((PTR_T, const char *, int));

extern PTR_T sh_memalign PARAMS((size_t, size_t, const char *, int));

extern PTR_T sh_calloc PARAMS((size_t, size_t, const char *, int));
extern void sh_cfree PARAMS((PTR_T, const char *, int));

extern PTR_T sh_valloc PARAMS((size_t, const char *, int));

 
extern int malloc_set_trace PARAMS((int));
extern void malloc_set_tracefp ();	 
extern void malloc_set_tracefn PARAMS((char *, char *));

 
extern void mregister_dump_table PARAMS((void));
extern void mregister_table_init PARAMS((void));
extern int malloc_set_register PARAMS((int));

 
extern void print_malloc_stats PARAMS((char *));
extern void fprint_malloc_stats ();	 
extern void trace_malloc_stats PARAMS((char *, char *));

#endif
