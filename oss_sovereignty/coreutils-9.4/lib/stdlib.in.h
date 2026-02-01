 

#@INCLUDE_NEXT@ @NEXT_STDLIB_H@

#else
 

#ifndef _@GUARD_PREFIX@_STDLIB_H

 
#@INCLUDE_NEXT@ @NEXT_STDLIB_H@

#ifndef _@GUARD_PREFIX@_STDLIB_H
#define _@GUARD_PREFIX@_STDLIB_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <stddef.h>

 
#if @GNULIB_SYSTEM_POSIX@ && !defined WEXITSTATUS
# include <sys/wait.h>
#endif

 
#if (@GNULIB_GETLOADAVG@ || defined GNULIB_POSIXCHECK) && @HAVE_SYS_LOADAVG_H@
 
# include <sys/time.h>
# include <sys/loadavg.h>
#endif

 
#if defined _WIN32 && !defined __CYGWIN__
# include <io.h>
#endif

#if @GNULIB_RANDOM_R@

 
# if @HAVE_RANDOM_H@
#  include <random.h>
# endif

# include <stdint.h>

# if !@HAVE_STRUCT_RANDOM_DATA@
 
#  if !GNULIB_defined_struct_random_data
struct random_data
{
  int32_t *fptr;                 
  int32_t *rptr;                 
  int32_t *state;                
  int rand_type;                 
  int rand_deg;                  
  int rand_sep;                  
  int32_t *end_ptr;              
};
#   define GNULIB_defined_struct_random_data 1
#  endif
# endif
#endif

#if (@GNULIB_MKSTEMP@ || @GNULIB_MKSTEMPS@ || @GNULIB_MKOSTEMP@ || @GNULIB_MKOSTEMPS@ || @GNULIB_GETSUBOPT@ || defined GNULIB_POSIXCHECK) && ! defined __GLIBC__ && !(defined _WIN32 && ! defined __CYGWIN__)
 
 
 
 
 
# include <unistd.h>
#endif

 
#ifndef _GL_ATTRIBUTE_DEALLOC
# if __GNUC__ >= 11
#  define _GL_ATTRIBUTE_DEALLOC(f, i) __attribute__ ((__malloc__ (f, i)))
# else
#  define _GL_ATTRIBUTE_DEALLOC(f, i)
# endif
#endif

 
 
#ifndef _GL_ATTRIBUTE_DEALLOC_FREE
# define _GL_ATTRIBUTE_DEALLOC_FREE _GL_ATTRIBUTE_DEALLOC (free, 1)
#endif

 
 
#ifndef _GL_ATTRIBUTE_MALLOC
# if __GNUC__ >= 3 || defined __clang__
#  define _GL_ATTRIBUTE_MALLOC __attribute__ ((__malloc__))
# else
#  define _GL_ATTRIBUTE_MALLOC
# endif
#endif

 
#ifndef _GL_ATTRIBUTE_PURE
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96) || defined __clang__
#  define _GL_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define _GL_ATTRIBUTE_PURE  
# endif
#endif

 

 

 

 


 
#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif
 
#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#elif EXIT_FAILURE != 1
# undef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif


#if @GNULIB__EXIT@
 
# if @REPLACE__EXIT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef _Exit
#   define _Exit rpl__Exit
#  endif
_GL_FUNCDECL_RPL (_Exit, _Noreturn void, (int status));
_GL_CXXALIAS_RPL (_Exit, void, (int status));
# else
#  if !@HAVE__EXIT@
_GL_FUNCDECL_SYS (_Exit, _Noreturn void, (int status));
#  endif
_GL_CXXALIAS_SYS (_Exit, void, (int status));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (_Exit);
# endif
#elif defined GNULIB_POSIXCHECK
# undef _Exit
# if HAVE_RAW_DECL__EXIT
_GL_WARN_ON_USE (_Exit, "_Exit is unportable - "
                 "use gnulib module _Exit for portability");
# endif
#endif


#if @GNULIB_FREE_POSIX@
# if @REPLACE_FREE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef free
#   define free rpl_free
#  endif
#  if defined __cplusplus && (__GLIBC__ + (__GLIBC_MINOR__ >= 14) > 2)
_GL_FUNCDECL_RPL (free, void, (void *ptr) throw ());
#  else
_GL_FUNCDECL_RPL (free, void, (void *ptr));
#  endif
_GL_CXXALIAS_RPL (free, void, (void *ptr));
# else
_GL_CXXALIAS_SYS (free, void, (void *ptr));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (free);
# endif
#elif defined GNULIB_POSIXCHECK
# undef free
 
_GL_WARN_ON_USE (free, "free is not future POSIX compliant everywhere - "
                 "use gnulib module free for portability");
#endif


 
#if @GNULIB_ALIGNED_ALLOC@
# if @REPLACE_ALIGNED_ALLOC@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef aligned_alloc
#   define aligned_alloc rpl_aligned_alloc
#  endif
_GL_FUNCDECL_RPL (aligned_alloc, void *,
                  (size_t alignment, size_t size)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
_GL_CXXALIAS_RPL (aligned_alloc, void *, (size_t alignment, size_t size));
# else
#  if @HAVE_ALIGNED_ALLOC@
#   if __GNUC__ >= 11
 
_GL_FUNCDECL_SYS (aligned_alloc, void *,
                  (size_t alignment, size_t size)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
#   endif
_GL_CXXALIAS_SYS (aligned_alloc, void *, (size_t alignment, size_t size));
#  endif
# endif
# if (__GLIBC__ >= 2) && @HAVE_ALIGNED_ALLOC@
_GL_CXXALIASWARN (aligned_alloc);
# endif
#else
# if @GNULIB_FREE_POSIX@ && __GNUC__ >= 11 && !defined aligned_alloc
 
_GL_FUNCDECL_SYS (aligned_alloc, void *,
                  (size_t alignment, size_t size)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
# endif
# if defined GNULIB_POSIXCHECK
#  undef aligned_alloc
#  if HAVE_RAW_DECL_ALIGNED_ALLOC
_GL_WARN_ON_USE (aligned_alloc, "aligned_alloc is not portable - "
                 "use gnulib module aligned_alloc for portability");
#  endif
# endif
#endif

#if @GNULIB_ATOLL@
 
# if !@HAVE_ATOLL@
_GL_FUNCDECL_SYS (atoll, long long, (const char *string)
                                    _GL_ATTRIBUTE_PURE
                                    _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (atoll, long long, (const char *string));
_GL_CXXALIASWARN (atoll);
#elif defined GNULIB_POSIXCHECK
# undef atoll
# if HAVE_RAW_DECL_ATOLL
_GL_WARN_ON_USE (atoll, "atoll is unportable - "
                 "use gnulib module atoll for portability");
# endif
#endif

#if @GNULIB_CALLOC_POSIX@
# if (@GNULIB_CALLOC_POSIX@ && @REPLACE_CALLOC_FOR_CALLOC_POSIX@) \
     || (@GNULIB_CALLOC_GNU@ && @REPLACE_CALLOC_FOR_CALLOC_GNU@)
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef calloc
#   define calloc rpl_calloc
#  endif
_GL_FUNCDECL_RPL (calloc, void *,
                  (size_t nmemb, size_t size)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
_GL_CXXALIAS_RPL (calloc, void *, (size_t nmemb, size_t size));
# else
#  if __GNUC__ >= 11
 
_GL_FUNCDECL_SYS (calloc, void *,
                  (size_t nmemb, size_t size)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
#  endif
_GL_CXXALIAS_SYS (calloc, void *, (size_t nmemb, size_t size));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (calloc);
# endif
#else
# if @GNULIB_FREE_POSIX@ && __GNUC__ >= 11 && !defined calloc
 
_GL_FUNCDECL_SYS (calloc, void *,
                  (size_t nmemb, size_t size)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
# endif
# if defined GNULIB_POSIXCHECK
#  undef calloc
 
_GL_WARN_ON_USE (calloc, "calloc is not POSIX compliant everywhere - "
                 "use gnulib module calloc-posix for portability");
# endif
#endif

#if @GNULIB_CANONICALIZE_FILE_NAME@
# if @REPLACE_CANONICALIZE_FILE_NAME@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define canonicalize_file_name rpl_canonicalize_file_name
#  endif
_GL_FUNCDECL_RPL (canonicalize_file_name, char *,
                  (const char *name)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
_GL_CXXALIAS_RPL (canonicalize_file_name, char *, (const char *name));
# else
#  if !@HAVE_CANONICALIZE_FILE_NAME@ || __GNUC__ >= 11
_GL_FUNCDECL_SYS (canonicalize_file_name, char *,
                  (const char *name)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
#  endif
_GL_CXXALIAS_SYS (canonicalize_file_name, char *, (const char *name));
# endif
# ifndef GNULIB_defined_canonicalize_file_name
#  define GNULIB_defined_canonicalize_file_name \
     (!@HAVE_CANONICALIZE_FILE_NAME@ || @REPLACE_CANONICALIZE_FILE_NAME@)
# endif
_GL_CXXALIASWARN (canonicalize_file_name);
#else
# if @GNULIB_FREE_POSIX@ && __GNUC__ >= 11 && !defined canonicalize_file_name
 
_GL_FUNCDECL_SYS (canonicalize_file_name, char *,
                  (const char *name)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
# endif
# if defined GNULIB_POSIXCHECK
#  undef canonicalize_file_name
#  if HAVE_RAW_DECL_CANONICALIZE_FILE_NAME
_GL_WARN_ON_USE (canonicalize_file_name,
                 "canonicalize_file_name is unportable - "
                 "use gnulib module canonicalize-lgpl for portability");
#  endif
# endif
#endif

#if @GNULIB_MDA_ECVT@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef ecvt
#   define ecvt _ecvt
#  endif
_GL_CXXALIAS_MDA (ecvt, char *,
                  (double number, int ndigits, int *decptp, int *signp));
# else
#  if @HAVE_DECL_ECVT@
_GL_CXXALIAS_SYS (ecvt, char *,
                  (double number, int ndigits, int *decptp, int *signp));
#  endif
# endif
# if (defined _WIN32 && !defined __CYGWIN__) || @HAVE_DECL_ECVT@
_GL_CXXALIASWARN (ecvt);
# endif
#endif

#if @GNULIB_MDA_FCVT@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fcvt
#   define fcvt _fcvt
#  endif
_GL_CXXALIAS_MDA (fcvt, char *,
                  (double number, int ndigits, int *decptp, int *signp));
# else
#  if @HAVE_DECL_FCVT@
_GL_CXXALIAS_SYS (fcvt, char *,
                  (double number, int ndigits, int *decptp, int *signp));
#  endif
# endif
# if (defined _WIN32 && !defined __CYGWIN__) || @HAVE_DECL_FCVT@
_GL_CXXALIASWARN (fcvt);
# endif
#endif

#if @GNULIB_MDA_GCVT@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef gcvt
#   define gcvt _gcvt
#  endif
_GL_CXXALIAS_MDA (gcvt, char *, (double number, int ndigits, char *buf));
# else
#  if @HAVE_DECL_GCVT@
_GL_CXXALIAS_SYS (gcvt, char *, (double number, int ndigits, char *buf));
#  endif
# endif
# if (defined _WIN32 && !defined __CYGWIN__) || @HAVE_DECL_GCVT@
_GL_CXXALIASWARN (gcvt);
# endif
#endif

#if @GNULIB_GETLOADAVG@
 
# if @REPLACE_GETLOADAVG@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getloadavg
#   define getloadavg rpl_getloadavg
#  endif
_GL_FUNCDECL_RPL (getloadavg, int, (double loadavg[], int nelem)
                                   _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (getloadavg, int, (double loadavg[], int nelem));
# else
#  if !@HAVE_DECL_GETLOADAVG@
_GL_FUNCDECL_SYS (getloadavg, int, (double loadavg[], int nelem)
                                   _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (getloadavg, int, (double loadavg[], int nelem));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (getloadavg);
# endif
#elif defined GNULIB_POSIXCHECK
# undef getloadavg
# if HAVE_RAW_DECL_GETLOADAVG
_GL_WARN_ON_USE (getloadavg, "getloadavg is not portable - "
                 "use gnulib module getloadavg for portability");
# endif
#endif

#if @GNULIB_GETPROGNAME@
 
# if @REPLACE_GETPROGNAME@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef getprogname
#   define getprogname rpl_getprogname
#  endif
#  if @HAVE_DECL_PROGRAM_INVOCATION_NAME@
_GL_FUNCDECL_RPL (getprogname, const char *, (void) _GL_ATTRIBUTE_PURE);
#  else
_GL_FUNCDECL_RPL (getprogname, const char *, (void));
#  endif
_GL_CXXALIAS_RPL (getprogname, const char *, (void));
# else
#  if !@HAVE_GETPROGNAME@
#   if @HAVE_DECL_PROGRAM_INVOCATION_NAME@
_GL_FUNCDECL_SYS (getprogname, const char *, (void) _GL_ATTRIBUTE_PURE);
#   else
_GL_FUNCDECL_SYS (getprogname, const char *, (void));
#   endif
#  endif
_GL_CXXALIAS_SYS (getprogname, const char *, (void));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (getprogname);
# endif
#elif defined GNULIB_POSIXCHECK
# undef getprogname
# if HAVE_RAW_DECL_GETPROGNAME
_GL_WARN_ON_USE (getprogname, "getprogname is unportable - "
                 "use gnulib module getprogname for portability");
# endif
#endif

#if @GNULIB_GETSUBOPT@
 
# if !@HAVE_GRANTPT@
_GL_FUNCDECL_SYS (grantpt, int, (int fd));
# endif
_GL_CXXALIAS_SYS (grantpt, int, (int fd));
_GL_CXXALIASWARN (grantpt);
#elif defined GNULIB_POSIXCHECK
# undef grantpt
# if HAVE_RAW_DECL_GRANTPT
_GL_WARN_ON_USE (grantpt, "grantpt is not portable - "
                 "use gnulib module grantpt for portability");
# endif
#endif

 
#if @GNULIB_MALLOC_POSIX@
# if (@GNULIB_MALLOC_POSIX@ && @REPLACE_MALLOC_FOR_MALLOC_POSIX@) \
     || (@GNULIB_MALLOC_GNU@ && @REPLACE_MALLOC_FOR_MALLOC_GNU@)
#  if !((defined __cplusplus && defined GNULIB_NAMESPACE) \
        || _GL_USE_STDLIB_ALLOC)
#   undef malloc
#   define malloc rpl_malloc
#  endif
_GL_FUNCDECL_RPL (malloc, void *,
                  (size_t size)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
_GL_CXXALIAS_RPL (malloc, void *, (size_t size));
# else
#  if __GNUC__ >= 11
 
_GL_FUNCDECL_SYS (malloc, void *,
                  (size_t size)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
#  endif
_GL_CXXALIAS_SYS (malloc, void *, (size_t size));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (malloc);
# endif
#else
# if @GNULIB_FREE_POSIX@ && __GNUC__ >= 11 && !defined malloc
 
_GL_FUNCDECL_SYS (malloc, void *,
                  (size_t size)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
# endif
# if defined GNULIB_POSIXCHECK && !_GL_USE_STDLIB_ALLOC
#  undef malloc
 
_GL_WARN_ON_USE (malloc, "malloc is not POSIX compliant everywhere - "
                 "use gnulib module malloc-posix for portability");
# endif
#endif

 
#if @REPLACE_MB_CUR_MAX@
# if !GNULIB_defined_MB_CUR_MAX
static inline
int gl_MB_CUR_MAX (void)
{
   
  return MB_CUR_MAX + (MB_CUR_MAX == 3);
}
#  undef MB_CUR_MAX
#  define MB_CUR_MAX gl_MB_CUR_MAX ()
#  define GNULIB_defined_MB_CUR_MAX 1
# endif
#endif

 
#if @GNULIB_MBSTOWCS@
# if @REPLACE_MBSTOWCS@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mbstowcs
#   define mbstowcs rpl_mbstowcs
#  endif
_GL_FUNCDECL_RPL (mbstowcs, size_t,
                  (wchar_t *restrict dest, const char *restrict src,
                   size_t len)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (mbstowcs, size_t,
                  (wchar_t *restrict dest, const char *restrict src,
                   size_t len));
# else
_GL_CXXALIAS_SYS (mbstowcs, size_t,
                  (wchar_t *restrict dest, const char *restrict src,
                   size_t len));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mbstowcs);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mbstowcs
# if HAVE_RAW_DECL_MBSTOWCS
_GL_WARN_ON_USE (mbstowcs, "mbstowcs is unportable - "
                 "use gnulib module mbstowcs for portability");
# endif
#endif

 
#if @GNULIB_MBTOWC@
# if @REPLACE_MBTOWC@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mbtowc
#   define mbtowc rpl_mbtowc
#  endif
_GL_FUNCDECL_RPL (mbtowc, int,
                  (wchar_t *restrict pwc, const char *restrict s, size_t n));
_GL_CXXALIAS_RPL (mbtowc, int,
                  (wchar_t *restrict pwc, const char *restrict s, size_t n));
# else
#  if !@HAVE_MBTOWC@
_GL_FUNCDECL_SYS (mbtowc, int,
                  (wchar_t *restrict pwc, const char *restrict s, size_t n));
#  endif
_GL_CXXALIAS_SYS (mbtowc, int,
                  (wchar_t *restrict pwc, const char *restrict s, size_t n));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mbtowc);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mbtowc
# if HAVE_RAW_DECL_MBTOWC
_GL_WARN_ON_USE (mbtowc, "mbtowc is not portable - "
                 "use gnulib module mbtowc for portability");
# endif
#endif

#if @GNULIB_MKDTEMP@
 
# if !@HAVE_MKDTEMP@
_GL_FUNCDECL_SYS (mkdtemp, char *, (char *  ) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (mkdtemp, char *, (char *  ));
_GL_CXXALIASWARN (mkdtemp);
#elif defined GNULIB_POSIXCHECK
# undef mkdtemp
# if HAVE_RAW_DECL_MKDTEMP
_GL_WARN_ON_USE (mkdtemp, "mkdtemp is unportable - "
                 "use gnulib module mkdtemp for portability");
# endif
#endif

#if @GNULIB_MKOSTEMP@
 
# if @REPLACE_MKOSTEMP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mkostemp
#   define mkostemp rpl_mkostemp
#  endif
_GL_FUNCDECL_RPL (mkostemp, int, (char *  , int  )
                                 _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (mkostemp, int, (char *  , int  ));
# else
#  if !@HAVE_MKOSTEMP@
_GL_FUNCDECL_SYS (mkostemp, int, (char *  , int  )
                                 _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (mkostemp, int, (char *  , int  ));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mkostemp);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mkostemp
# if HAVE_RAW_DECL_MKOSTEMP
_GL_WARN_ON_USE (mkostemp, "mkostemp is unportable - "
                 "use gnulib module mkostemp for portability");
# endif
#endif

#if @GNULIB_MKOSTEMPS@
 
# if @REPLACE_MKOSTEMPS@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mkostemps
#   define mkostemps rpl_mkostemps
#  endif
_GL_FUNCDECL_RPL (mkostemps, int,
                  (char *  , int  , int  )
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (mkostemps, int,
                  (char *  , int  , int  ));
# else
#  if !@HAVE_MKOSTEMPS@
_GL_FUNCDECL_SYS (mkostemps, int,
                  (char *  , int  , int  )
                  _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (mkostemps, int,
                  (char *  , int  , int  ));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mkostemps);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mkostemps
# if HAVE_RAW_DECL_MKOSTEMPS
_GL_WARN_ON_USE (mkostemps, "mkostemps is unportable - "
                 "use gnulib module mkostemps for portability");
# endif
#endif

#if @GNULIB_MKSTEMP@
 
# if @REPLACE_MKSTEMP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define mkstemp rpl_mkstemp
#  endif
_GL_FUNCDECL_RPL (mkstemp, int, (char *  ) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (mkstemp, int, (char *  ));
# else
#  if ! @HAVE_MKSTEMP@
_GL_FUNCDECL_SYS (mkstemp, int, (char *  ) _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (mkstemp, int, (char *  ));
# endif
_GL_CXXALIASWARN (mkstemp);
#elif defined GNULIB_POSIXCHECK
# undef mkstemp
# if HAVE_RAW_DECL_MKSTEMP
_GL_WARN_ON_USE (mkstemp, "mkstemp is unportable - "
                 "use gnulib module mkstemp for portability");
# endif
#endif

#if @GNULIB_MKSTEMPS@
 
# if !@HAVE_MKSTEMPS@
_GL_FUNCDECL_SYS (mkstemps, int, (char *  , int  )
                                 _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (mkstemps, int, (char *  , int  ));
_GL_CXXALIASWARN (mkstemps);
#elif defined GNULIB_POSIXCHECK
# undef mkstemps
# if HAVE_RAW_DECL_MKSTEMPS
_GL_WARN_ON_USE (mkstemps, "mkstemps is unportable - "
                 "use gnulib module mkstemps for portability");
# endif
#endif

#if @GNULIB_MDA_MKTEMP@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mktemp
#   define mktemp _mktemp
#  endif
_GL_CXXALIAS_MDA (mktemp, char *, (char *  ));
# else
_GL_CXXALIAS_SYS (mktemp, char *, (char *  ));
# endif
_GL_CXXALIASWARN (mktemp);
#endif

 
#if @GNULIB_POSIX_MEMALIGN@
# if @REPLACE_POSIX_MEMALIGN@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef posix_memalign
#   define posix_memalign rpl_posix_memalign
#  endif
_GL_FUNCDECL_RPL (posix_memalign, int,
                  (void **memptr, size_t alignment, size_t size)
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (posix_memalign, int,
                  (void **memptr, size_t alignment, size_t size));
# else
#  if @HAVE_POSIX_MEMALIGN@
_GL_CXXALIAS_SYS (posix_memalign, int,
                  (void **memptr, size_t alignment, size_t size));
#  endif
# endif
# if __GLIBC__ >= 2 && @HAVE_POSIX_MEMALIGN@
_GL_CXXALIASWARN (posix_memalign);
# endif
#elif defined GNULIB_POSIXCHECK
# undef posix_memalign
# if HAVE_RAW_DECL_POSIX_MEMALIGN
_GL_WARN_ON_USE (posix_memalign, "posix_memalign is not portable - "
                 "use gnulib module posix_memalign for portability");
# endif
#endif

#if @GNULIB_POSIX_OPENPT@
 
# if @REPLACE_POSIX_OPENPT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef posix_openpt
#   define posix_openpt rpl_posix_openpt
#  endif
_GL_FUNCDECL_RPL (posix_openpt, int, (int flags));
_GL_CXXALIAS_RPL (posix_openpt, int, (int flags));
# else
#  if !@HAVE_POSIX_OPENPT@
_GL_FUNCDECL_SYS (posix_openpt, int, (int flags));
#  endif
_GL_CXXALIAS_SYS (posix_openpt, int, (int flags));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (posix_openpt);
# endif
#elif defined GNULIB_POSIXCHECK
# undef posix_openpt
# if HAVE_RAW_DECL_POSIX_OPENPT
_GL_WARN_ON_USE (posix_openpt, "posix_openpt is not portable - "
                 "use gnulib module posix_openpt for portability");
# endif
#endif

#if @GNULIB_PTSNAME@
 
# if @REPLACE_PTSNAME@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef ptsname
#   define ptsname rpl_ptsname
#  endif
_GL_FUNCDECL_RPL (ptsname, char *, (int fd));
_GL_CXXALIAS_RPL (ptsname, char *, (int fd));
# else
#  if !@HAVE_PTSNAME@
_GL_FUNCDECL_SYS (ptsname, char *, (int fd));
#  endif
_GL_CXXALIAS_SYS (ptsname, char *, (int fd));
# endif
_GL_CXXALIASWARN (ptsname);
#elif defined GNULIB_POSIXCHECK
# undef ptsname
# if HAVE_RAW_DECL_PTSNAME
_GL_WARN_ON_USE (ptsname, "ptsname is not portable - "
                 "use gnulib module ptsname for portability");
# endif
#endif

#if @GNULIB_PTSNAME_R@
 
# if @REPLACE_PTSNAME_R@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef ptsname_r
#   define ptsname_r rpl_ptsname_r
#  endif
_GL_FUNCDECL_RPL (ptsname_r, int, (int fd, char *buf, size_t len));
_GL_CXXALIAS_RPL (ptsname_r, int, (int fd, char *buf, size_t len));
# else
#  if !@HAVE_PTSNAME_R@
_GL_FUNCDECL_SYS (ptsname_r, int, (int fd, char *buf, size_t len));
#  endif
_GL_CXXALIAS_SYS (ptsname_r, int, (int fd, char *buf, size_t len));
# endif
# ifndef GNULIB_defined_ptsname_r
#  define GNULIB_defined_ptsname_r (!@HAVE_PTSNAME_R@ || @REPLACE_PTSNAME_R@)
# endif
_GL_CXXALIASWARN (ptsname_r);
#elif defined GNULIB_POSIXCHECK
# undef ptsname_r
# if HAVE_RAW_DECL_PTSNAME_R
_GL_WARN_ON_USE (ptsname_r, "ptsname_r is not portable - "
                 "use gnulib module ptsname_r for portability");
# endif
#endif

#if @GNULIB_PUTENV@
# if @REPLACE_PUTENV@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef putenv
#   define putenv rpl_putenv
#  endif
_GL_FUNCDECL_RPL (putenv, int, (char *string) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (putenv, int, (char *string));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef putenv
#   define putenv _putenv
#  endif
_GL_CXXALIAS_MDA (putenv, int, (char *string));
# else
_GL_CXXALIAS_SYS (putenv, int, (char *string));
# endif
_GL_CXXALIASWARN (putenv);
#elif @GNULIB_MDA_PUTENV@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef putenv
#   define putenv _putenv
#  endif
 
_GL_CXXALIAS_MDA_CAST (putenv, int, (char *string));
# else
_GL_CXXALIAS_SYS (putenv, int, (char *string));
# endif
_GL_CXXALIASWARN (putenv);
#endif

#if @GNULIB_QSORT_R@
 
# ifdef __cplusplus
extern "C" {
# endif
# if !GNULIB_defined_qsort_r_fn_types
typedef int (*_gl_qsort_r_compar_fn) (void const *, void const *, void *);
#  define GNULIB_defined_qsort_r_fn_types 1
# endif
# ifdef __cplusplus
}
# endif
# if @REPLACE_QSORT_R@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef qsort_r
#   define qsort_r rpl_qsort_r
#  endif
_GL_FUNCDECL_RPL (qsort_r, void, (void *base, size_t nmemb, size_t size,
                                  _gl_qsort_r_compar_fn compare,
                                  void *arg) _GL_ARG_NONNULL ((1, 4)));
_GL_CXXALIAS_RPL (qsort_r, void, (void *base, size_t nmemb, size_t size,
                                  _gl_qsort_r_compar_fn compare,
                                  void *arg));
# else
#  if !@HAVE_QSORT_R@
_GL_FUNCDECL_SYS (qsort_r, void, (void *base, size_t nmemb, size_t size,
                                  _gl_qsort_r_compar_fn compare,
                                  void *arg) _GL_ARG_NONNULL ((1, 4)));
#  endif
_GL_CXXALIAS_SYS (qsort_r, void, (void *base, size_t nmemb, size_t size,
                                  _gl_qsort_r_compar_fn compare,
                                  void *arg));
# endif
_GL_CXXALIASWARN (qsort_r);
#elif defined GNULIB_POSIXCHECK
# undef qsort_r
# if HAVE_RAW_DECL_QSORT_R
_GL_WARN_ON_USE (qsort_r, "qsort_r is not portable - "
                 "use gnulib module qsort_r for portability");
# endif
#endif


#if @GNULIB_RANDOM_R@
# if !@HAVE_RANDOM_R@
#  ifndef RAND_MAX
#   define RAND_MAX 2147483647
#  endif
# endif
#endif


#if @GNULIB_RANDOM@
# if @REPLACE_RANDOM@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef random
#   define random rpl_random
#  endif
_GL_FUNCDECL_RPL (random, long, (void));
_GL_CXXALIAS_RPL (random, long, (void));
# else
#  if !@HAVE_RANDOM@
_GL_FUNCDECL_SYS (random, long, (void));
#  endif
 
_GL_CXXALIAS_SYS_CAST (random, long, (void));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (random);
# endif
#elif defined GNULIB_POSIXCHECK
# undef random
# if HAVE_RAW_DECL_RANDOM
_GL_WARN_ON_USE (random, "random is unportable - "
                 "use gnulib module random for portability");
# endif
#endif

#if @GNULIB_RANDOM@
# if @REPLACE_RANDOM@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef srandom
#   define srandom rpl_srandom
#  endif
_GL_FUNCDECL_RPL (srandom, void, (unsigned int seed));
_GL_CXXALIAS_RPL (srandom, void, (unsigned int seed));
# else
#  if !@HAVE_RANDOM@
_GL_FUNCDECL_SYS (srandom, void, (unsigned int seed));
#  endif
 
_GL_CXXALIAS_SYS_CAST (srandom, void, (unsigned int seed));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (srandom);
# endif
#elif defined GNULIB_POSIXCHECK
# undef srandom
# if HAVE_RAW_DECL_SRANDOM
_GL_WARN_ON_USE (srandom, "srandom is unportable - "
                 "use gnulib module random for portability");
# endif
#endif

#if @GNULIB_RANDOM@
# if @REPLACE_INITSTATE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef initstate
#   define initstate rpl_initstate
#  endif
_GL_FUNCDECL_RPL (initstate, char *,
                  (unsigned int seed, char *buf, size_t buf_size)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (initstate, char *,
                  (unsigned int seed, char *buf, size_t buf_size));
# else
#  if !@HAVE_INITSTATE@ || !@HAVE_DECL_INITSTATE@
_GL_FUNCDECL_SYS (initstate, char *,
                  (unsigned int seed, char *buf, size_t buf_size)
                  _GL_ARG_NONNULL ((2)));
#  endif
 
_GL_CXXALIAS_SYS_CAST (initstate, char *,
                       (unsigned int seed, char *buf, size_t buf_size));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (initstate);
# endif
#elif defined GNULIB_POSIXCHECK
# undef initstate
# if HAVE_RAW_DECL_INITSTATE
_GL_WARN_ON_USE (initstate, "initstate is unportable - "
                 "use gnulib module random for portability");
# endif
#endif

#if @GNULIB_RANDOM@
# if @REPLACE_SETSTATE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef setstate
#   define setstate rpl_setstate
#  endif
_GL_FUNCDECL_RPL (setstate, char *, (char *arg_state) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (setstate, char *, (char *arg_state));
# else
#  if !@HAVE_SETSTATE@ || !@HAVE_DECL_SETSTATE@
_GL_FUNCDECL_SYS (setstate, char *, (char *arg_state) _GL_ARG_NONNULL ((1)));
#  endif
 
_GL_CXXALIAS_SYS_CAST (setstate, char *, (char *arg_state));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (setstate);
# endif
#elif defined GNULIB_POSIXCHECK
# undef setstate
# if HAVE_RAW_DECL_SETSTATE
_GL_WARN_ON_USE (setstate, "setstate is unportable - "
                 "use gnulib module random for portability");
# endif
#endif


#if @GNULIB_RANDOM_R@
# if @REPLACE_RANDOM_R@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef random_r
#   define random_r rpl_random_r
#  endif
_GL_FUNCDECL_RPL (random_r, int, (struct random_data *buf, int32_t *result)
                                 _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (random_r, int, (struct random_data *buf, int32_t *result));
# else
#  if !@HAVE_RANDOM_R@
_GL_FUNCDECL_SYS (random_r, int, (struct random_data *buf, int32_t *result)
                                 _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (random_r, int, (struct random_data *buf, int32_t *result));
# endif
_GL_CXXALIASWARN (random_r);
#elif defined GNULIB_POSIXCHECK
# undef random_r
# if HAVE_RAW_DECL_RANDOM_R
_GL_WARN_ON_USE (random_r, "random_r is unportable - "
                 "use gnulib module random_r for portability");
# endif
#endif

#if @GNULIB_RANDOM_R@
# if @REPLACE_RANDOM_R@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef srandom_r
#   define srandom_r rpl_srandom_r
#  endif
_GL_FUNCDECL_RPL (srandom_r, int,
                  (unsigned int seed, struct random_data *rand_state)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (srandom_r, int,
                  (unsigned int seed, struct random_data *rand_state));
# else
#  if !@HAVE_RANDOM_R@
_GL_FUNCDECL_SYS (srandom_r, int,
                  (unsigned int seed, struct random_data *rand_state)
                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (srandom_r, int,
                  (unsigned int seed, struct random_data *rand_state));
# endif
_GL_CXXALIASWARN (srandom_r);
#elif defined GNULIB_POSIXCHECK
# undef srandom_r
# if HAVE_RAW_DECL_SRANDOM_R
_GL_WARN_ON_USE (srandom_r, "srandom_r is unportable - "
                 "use gnulib module random_r for portability");
# endif
#endif

#if @GNULIB_RANDOM_R@
# if @REPLACE_RANDOM_R@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef initstate_r
#   define initstate_r rpl_initstate_r
#  endif
_GL_FUNCDECL_RPL (initstate_r, int,
                  (unsigned int seed, char *buf, size_t buf_size,
                   struct random_data *rand_state)
                  _GL_ARG_NONNULL ((2, 4)));
_GL_CXXALIAS_RPL (initstate_r, int,
                  (unsigned int seed, char *buf, size_t buf_size,
                   struct random_data *rand_state));
# else
#  if !@HAVE_RANDOM_R@
_GL_FUNCDECL_SYS (initstate_r, int,
                  (unsigned int seed, char *buf, size_t buf_size,
                   struct random_data *rand_state)
                  _GL_ARG_NONNULL ((2, 4)));
#  endif
 
_GL_CXXALIAS_SYS_CAST (initstate_r, int,
                       (unsigned int seed, char *buf, size_t buf_size,
                        struct random_data *rand_state));
# endif
_GL_CXXALIASWARN (initstate_r);
#elif defined GNULIB_POSIXCHECK
# undef initstate_r
# if HAVE_RAW_DECL_INITSTATE_R
_GL_WARN_ON_USE (initstate_r, "initstate_r is unportable - "
                 "use gnulib module random_r for portability");
# endif
#endif

#if @GNULIB_RANDOM_R@
# if @REPLACE_RANDOM_R@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef setstate_r
#   define setstate_r rpl_setstate_r
#  endif
_GL_FUNCDECL_RPL (setstate_r, int,
                  (char *arg_state, struct random_data *rand_state)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (setstate_r, int,
                  (char *arg_state, struct random_data *rand_state));
# else
#  if !@HAVE_RANDOM_R@
_GL_FUNCDECL_SYS (setstate_r, int,
                  (char *arg_state, struct random_data *rand_state)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
 
_GL_CXXALIAS_SYS_CAST (setstate_r, int,
                       (char *arg_state, struct random_data *rand_state));
# endif
_GL_CXXALIASWARN (setstate_r);
#elif defined GNULIB_POSIXCHECK
# undef setstate_r
# if HAVE_RAW_DECL_SETSTATE_R
_GL_WARN_ON_USE (setstate_r, "setstate_r is unportable - "
                 "use gnulib module random_r for portability");
# endif
#endif


#if @GNULIB_REALLOC_POSIX@
# if (@GNULIB_REALLOC_POSIX@ && @REPLACE_REALLOC_FOR_REALLOC_POSIX@) \
     || (@GNULIB_REALLOC_GNU@ && @REPLACE_REALLOC_FOR_REALLOC_GNU@)
#  if !((defined __cplusplus && defined GNULIB_NAMESPACE) \
        || _GL_USE_STDLIB_ALLOC)
#   undef realloc
#   define realloc rpl_realloc
#  endif
_GL_FUNCDECL_RPL (realloc, void *, (void *ptr, size_t size)
                                   _GL_ATTRIBUTE_DEALLOC_FREE);
_GL_CXXALIAS_RPL (realloc, void *, (void *ptr, size_t size));
# else
#  if __GNUC__ >= 11
 
_GL_FUNCDECL_SYS (realloc, void *, (void *ptr, size_t size)
                                   _GL_ATTRIBUTE_DEALLOC_FREE);
#  endif
_GL_CXXALIAS_SYS (realloc, void *, (void *ptr, size_t size));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (realloc);
# endif
#else
# if @GNULIB_FREE_POSIX@ && __GNUC__ >= 11 && !defined realloc
 
_GL_FUNCDECL_SYS (realloc, void *, (void *ptr, size_t size)
                                   _GL_ATTRIBUTE_DEALLOC_FREE);
# endif
# if defined GNULIB_POSIXCHECK && !_GL_USE_STDLIB_ALLOC
#  undef realloc
 
_GL_WARN_ON_USE (realloc, "realloc is not POSIX compliant everywhere - "
                 "use gnulib module realloc-posix for portability");
# endif
#endif


#if @GNULIB_REALLOCARRAY@
# if @REPLACE_REALLOCARRAY@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef reallocarray
#   define reallocarray rpl_reallocarray
#  endif
_GL_FUNCDECL_RPL (reallocarray, void *,
                  (void *ptr, size_t nmemb, size_t size));
_GL_CXXALIAS_RPL (reallocarray, void *,
                  (void *ptr, size_t nmemb, size_t size));
# else
#  if ! @HAVE_REALLOCARRAY@
_GL_FUNCDECL_SYS (reallocarray, void *,
                  (void *ptr, size_t nmemb, size_t size));
#  endif
_GL_CXXALIAS_SYS (reallocarray, void *,
                  (void *ptr, size_t nmemb, size_t size));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (reallocarray);
# endif
#elif defined GNULIB_POSIXCHECK
# undef reallocarray
# if HAVE_RAW_DECL_REALLOCARRAY
_GL_WARN_ON_USE (reallocarray, "reallocarray is not portable - "
                 "use gnulib module reallocarray for portability");
# endif
#endif

#if @GNULIB_REALPATH@
# if @REPLACE_REALPATH@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define realpath rpl_realpath
#  endif
_GL_FUNCDECL_RPL (realpath, char *,
                  (const char *restrict name, char *restrict resolved)
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (realpath, char *,
                  (const char *restrict name, char *restrict resolved));
# else
#  if !@HAVE_REALPATH@
_GL_FUNCDECL_SYS (realpath, char *,
                  (const char *restrict name, char *restrict resolved)
                  _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (realpath, char *,
                  (const char *restrict name, char *restrict resolved));
# endif
_GL_CXXALIASWARN (realpath);
#elif defined GNULIB_POSIXCHECK
# undef realpath
# if HAVE_RAW_DECL_REALPATH
_GL_WARN_ON_USE (realpath, "realpath is unportable - use gnulib module "
                 "canonicalize or canonicalize-lgpl for portability");
# endif
#endif

#if @GNULIB_RPMATCH@
 
# if !@HAVE_RPMATCH@
_GL_FUNCDECL_SYS (rpmatch, int, (const char *response) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (rpmatch, int, (const char *response));
_GL_CXXALIASWARN (rpmatch);
#elif defined GNULIB_POSIXCHECK
# undef rpmatch
# if HAVE_RAW_DECL_RPMATCH
_GL_WARN_ON_USE (rpmatch, "rpmatch is unportable - "
                 "use gnulib module rpmatch for portability");
# endif
#endif

#if @GNULIB_SECURE_GETENV@
 
# if !@HAVE_SECURE_GETENV@
_GL_FUNCDECL_SYS (secure_getenv, char *,
                  (char const *name) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (secure_getenv, char *, (char const *name));
_GL_CXXALIASWARN (secure_getenv);
#elif defined GNULIB_POSIXCHECK
# undef secure_getenv
# if HAVE_RAW_DECL_SECURE_GETENV
_GL_WARN_ON_USE (secure_getenv, "secure_getenv is unportable - "
                 "use gnulib module secure_getenv for portability");
# endif
#endif

#if @GNULIB_SETENV@
 
# if @REPLACE_SETENV@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef setenv
#   define setenv rpl_setenv
#  endif
_GL_FUNCDECL_RPL (setenv, int,
                  (const char *name, const char *value, int replace)
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (setenv, int,
                  (const char *name, const char *value, int replace));
# else
#  if !@HAVE_DECL_SETENV@
_GL_FUNCDECL_SYS (setenv, int,
                  (const char *name, const char *value, int replace)
                  _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (setenv, int,
                  (const char *name, const char *value, int replace));
# endif
# if !(@REPLACE_SETENV@ && !@HAVE_DECL_SETENV@)
_GL_CXXALIASWARN (setenv);
# endif
#elif defined GNULIB_POSIXCHECK
# undef setenv
# if HAVE_RAW_DECL_SETENV
_GL_WARN_ON_USE (setenv, "setenv is unportable - "
                 "use gnulib module setenv for portability");
# endif
#endif

#if @GNULIB_STRTOD@
  
# if @REPLACE_STRTOD@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define strtod rpl_strtod
#  endif
#  define GNULIB_defined_strtod_function 1
_GL_FUNCDECL_RPL (strtod, double,
                  (const char *restrict str, char **restrict endp)
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (strtod, double,
                  (const char *restrict str, char **restrict endp));
# else
#  if !@HAVE_STRTOD@
_GL_FUNCDECL_SYS (strtod, double,
                  (const char *restrict str, char **restrict endp)
                  _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (strtod, double,
                  (const char *restrict str, char **restrict endp));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (strtod);
# endif
#elif defined GNULIB_POSIXCHECK
# undef strtod
# if HAVE_RAW_DECL_STRTOD
_GL_WARN_ON_USE (strtod, "strtod is unportable - "
                 "use gnulib module strtod for portability");
# endif
#endif

#if @GNULIB_STRTOLD@
  
# if @REPLACE_STRTOLD@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define strtold rpl_strtold
#  endif
#  define GNULIB_defined_strtold_function 1
_GL_FUNCDECL_RPL (strtold, long double,
                  (const char *restrict str, char **restrict endp)
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (strtold, long double,
                  (const char *restrict str, char **restrict endp));
# else
#  if !@HAVE_STRTOLD@
_GL_FUNCDECL_SYS (strtold, long double,
                  (const char *restrict str, char **restrict endp)
                  _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (strtold, long double,
                  (const char *restrict str, char **restrict endp));
# endif
_GL_CXXALIASWARN (strtold);
#elif defined GNULIB_POSIXCHECK
# undef strtold
# if HAVE_RAW_DECL_STRTOLD
_GL_WARN_ON_USE (strtold, "strtold is unportable - "
                 "use gnulib module strtold for portability");
# endif
#endif

#if @GNULIB_STRTOL@
 
# if @REPLACE_STRTOL@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define strtol rpl_strtol
#  endif
#  define GNULIB_defined_strtol_function 1
_GL_FUNCDECL_RPL (strtol, long,
                  (const char *restrict string, char **restrict endptr,
                   int base)
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (strtol, long,
                  (const char *restrict string, char **restrict endptr,
                   int base));
# else
#  if !@HAVE_STRTOL@
_GL_FUNCDECL_SYS (strtol, long,
                  (const char *restrict string, char **restrict endptr,
                   int base)
                  _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (strtol, long,
                  (const char *restrict string, char **restrict endptr,
                   int base));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (strtol);
# endif
#elif defined GNULIB_POSIXCHECK
# undef strtol
# if HAVE_RAW_DECL_STRTOL
_GL_WARN_ON_USE (strtol, "strtol is unportable - "
                 "use gnulib module strtol for portability");
# endif
#endif

#if @GNULIB_STRTOLL@
 
# if @REPLACE_STRTOLL@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define strtoll rpl_strtoll
#  endif
#  define GNULIB_defined_strtoll_function 1
_GL_FUNCDECL_RPL (strtoll, long long,
                  (const char *restrict string, char **restrict endptr,
                   int base)
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (strtoll, long long,
                  (const char *restrict string, char **restrict endptr,
                   int base));
# else
#  if !@HAVE_STRTOLL@
_GL_FUNCDECL_SYS (strtoll, long long,
                  (const char *restrict string, char **restrict endptr,
                   int base)
                  _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (strtoll, long long,
                  (const char *restrict string, char **restrict endptr,
                   int base));
# endif
_GL_CXXALIASWARN (strtoll);
#elif defined GNULIB_POSIXCHECK
# undef strtoll
# if HAVE_RAW_DECL_STRTOLL
_GL_WARN_ON_USE (strtoll, "strtoll is unportable - "
                 "use gnulib module strtoll for portability");
# endif
#endif

#if @GNULIB_STRTOUL@
 
# if @REPLACE_STRTOUL@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define strtoul rpl_strtoul
#  endif
#  define GNULIB_defined_strtoul_function 1
_GL_FUNCDECL_RPL (strtoul, unsigned long,
                  (const char *restrict string, char **restrict endptr,
                   int base)
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (strtoul, unsigned long,
                  (const char *restrict string, char **restrict endptr,
                   int base));
# else
#  if !@HAVE_STRTOUL@
_GL_FUNCDECL_SYS (strtoul, unsigned long,
                  (const char *restrict string, char **restrict endptr,
                   int base)
                  _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (strtoul, unsigned long,
                  (const char *restrict string, char **restrict endptr,
                   int base));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (strtoul);
# endif
#elif defined GNULIB_POSIXCHECK
# undef strtoul
# if HAVE_RAW_DECL_STRTOUL
_GL_WARN_ON_USE (strtoul, "strtoul is unportable - "
                 "use gnulib module strtoul for portability");
# endif
#endif

#if @GNULIB_STRTOULL@
 
# if @REPLACE_STRTOULL@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define strtoull rpl_strtoull
#  endif
#  define GNULIB_defined_strtoull_function 1
_GL_FUNCDECL_RPL (strtoull, unsigned long long,
                  (const char *restrict string, char **restrict endptr,
                   int base)
                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (strtoull, unsigned long long,
                  (const char *restrict string, char **restrict endptr,
                   int base));
# else
#  if !@HAVE_STRTOULL@
_GL_FUNCDECL_SYS (strtoull, unsigned long long,
                  (const char *restrict string, char **restrict endptr,
                   int base)
                  _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (strtoull, unsigned long long,
                  (const char *restrict string, char **restrict endptr,
                   int base));
# endif
_GL_CXXALIASWARN (strtoull);
#elif defined GNULIB_POSIXCHECK
# undef strtoull
# if HAVE_RAW_DECL_STRTOULL
_GL_WARN_ON_USE (strtoull, "strtoull is unportable - "
                 "use gnulib module strtoull for portability");
# endif
#endif

#if @GNULIB_UNLOCKPT@
 
# if !@HAVE_UNLOCKPT@
_GL_FUNCDECL_SYS (unlockpt, int, (int fd));
# endif
_GL_CXXALIAS_SYS (unlockpt, int, (int fd));
_GL_CXXALIASWARN (unlockpt);
#elif defined GNULIB_POSIXCHECK
# undef unlockpt
# if HAVE_RAW_DECL_UNLOCKPT
_GL_WARN_ON_USE (unlockpt, "unlockpt is not portable - "
                 "use gnulib module unlockpt for portability");
# endif
#endif

#if @GNULIB_UNSETENV@
 
# if @REPLACE_UNSETENV@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef unsetenv
#   define unsetenv rpl_unsetenv
#  endif
_GL_FUNCDECL_RPL (unsetenv, int, (const char *name) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (unsetenv, int, (const char *name));
# else
#  if !@HAVE_DECL_UNSETENV@
_GL_FUNCDECL_SYS (unsetenv, int, (const char *name) _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (unsetenv, int, (const char *name));
# endif
# if !(@REPLACE_UNSETENV@ && !@HAVE_DECL_UNSETENV@)
_GL_CXXALIASWARN (unsetenv);
# endif
#elif defined GNULIB_POSIXCHECK
# undef unsetenv
# if HAVE_RAW_DECL_UNSETENV
_GL_WARN_ON_USE (unsetenv, "unsetenv is unportable - "
                 "use gnulib module unsetenv for portability");
# endif
#endif

 
#if @GNULIB_WCTOMB@
# if @REPLACE_WCTOMB@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wctomb
#   define wctomb rpl_wctomb
#  endif
_GL_FUNCDECL_RPL (wctomb, int, (char *s, wchar_t wc));
_GL_CXXALIAS_RPL (wctomb, int, (char *s, wchar_t wc));
# else
_GL_CXXALIAS_SYS (wctomb, int, (char *s, wchar_t wc));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wctomb);
# endif
#endif


#endif  
#endif  
#endif
