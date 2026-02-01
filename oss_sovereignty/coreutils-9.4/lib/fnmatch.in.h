 
#if @HAVE_FNMATCH_H@ && !@REPLACE_FNMATCH@
# @INCLUDE_NEXT@ @NEXT_FNMATCH_H@
#endif

#ifndef _@GUARD_PREFIX@_FNMATCH_H
#define _@GUARD_PREFIX@_FNMATCH_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 

 

 

#if !@HAVE_FNMATCH_H@ || @REPLACE_FNMATCH@

 
#undef  FNM_PATHNAME
#undef  FNM_NOESCAPE
#undef  FNM_PERIOD

 
#define FNM_PATHNAME    (1 << 0)  
#define FNM_NOESCAPE    (1 << 1)  
#define FNM_PERIOD      (1 << 2)  

#if !defined _POSIX_C_SOURCE || _POSIX_C_SOURCE < 2 || defined _GNU_SOURCE
# define FNM_FILE_NAME   FNM_PATHNAME    
# define FNM_LEADING_DIR (1 << 3)        
# define FNM_CASEFOLD    (1 << 4)        
# define FNM_EXTMATCH    (1 << 5)        
#endif

 
#define FNM_NOMATCH     1

 
#ifdef _XOPEN_SOURCE
# define FNM_NOSYS      (-1)
#endif

#endif


#if @GNULIB_FNMATCH@
 
# if @REPLACE_FNMATCH@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define fnmatch rpl_fnmatch
#  endif
#  define GNULIB_defined_fnmatch_function 1
_GL_FUNCDECL_RPL (fnmatch, int,
                  (const char *pattern, const char *name, int flags)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (fnmatch, int,
                  (const char *pattern, const char *name, int flags));
# else
#  if !@HAVE_FNMATCH@
#   define GNULIB_defined_fnmatch_function 1
_GL_FUNCDECL_SYS (fnmatch, int,
                  (const char *pattern, const char *name, int flags)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (fnmatch, int,
                  (const char *pattern, const char *name, int flags));
# endif
# if !GNULIB_FNMATCH_GNU && __GLIBC__ >= 2
_GL_CXXALIASWARN (fnmatch);
# endif
#elif defined GNULIB_POSIXCHECK
# undef fnmatch
# if HAVE_RAW_DECL_FNMATCH
_GL_WARN_ON_USE (fnmatch,
                 "fnmatch does not portably work - "
                 "use gnulib module fnmatch for portability or gnulib module fnmatch-gnu for a glibc compatible implementation");
# endif
#endif


#endif  
#endif  
