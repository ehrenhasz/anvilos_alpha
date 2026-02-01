 

 

#if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
#endif
@PRAGMA_COLUMNS@

#if (((defined __need_mbstate_t || defined __need_wint_t)               \
      && !defined __MINGW32__)                                          \
     || (defined __hpux                                                 \
         && ((defined _INTTYPES_INCLUDED                                \
              && !defined _GL_FINISHED_INCLUDING_SYSTEM_INTTYPES_H)     \
             || defined _GL_JUST_INCLUDE_SYSTEM_WCHAR_H))               \
     || (defined __MINGW32__ && defined __STRING_H_SOURCED__)           \
     || defined _GL_ALREADY_INCLUDING_WCHAR_H)
 

#@INCLUDE_NEXT@ @NEXT_WCHAR_H@

#else
 

#ifndef _@GUARD_PREFIX@_WCHAR_H

#define _GL_ALREADY_INCLUDING_WCHAR_H

#if @HAVE_FEATURES_H@
# include <features.h>  
#endif

 
#if !(defined __GLIBC__ && !defined __UCLIBC__)
# include <stddef.h>
#endif

 
 
#if @HAVE_WCHAR_H@
# @INCLUDE_NEXT@ @NEXT_WCHAR_H@
#endif

#undef _GL_ALREADY_INCLUDING_WCHAR_H

#ifndef _@GUARD_PREFIX@_WCHAR_H
#define _@GUARD_PREFIX@_WCHAR_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#ifndef _GL_ATTRIBUTE_DEALLOC
# if __GNUC__ >= 11
#  define _GL_ATTRIBUTE_DEALLOC(f, i) __attribute__ ((__malloc__ (f, i)))
# else
#  define _GL_ATTRIBUTE_DEALLOC(f, i)
# endif
#endif

 
 
#ifndef _GL_ATTRIBUTE_DEALLOC_FREE
# if defined __cplusplus && defined __GNUC__ && !defined __clang__
 
 
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

 

 

 


 
#if !@HAVE_WINT_T@ && !defined wint_t
# define wint_t int
# ifndef WEOF
#  define WEOF -1
# endif
#else
 
# if @GNULIBHEADERS_OVERRIDE_WINT_T@
#  if !GNULIB_defined_wint_t
#   if @HAVE_CRTDEFS_H@
#    include <crtdefs.h>
#   else
#    include <stddef.h>
#   endif
typedef unsigned int rpl_wint_t;
#   undef wint_t
#   define wint_t rpl_wint_t
#   define GNULIB_defined_wint_t 1
#  endif
# endif
# ifndef WEOF
#  define WEOF ((wint_t) -1)
# endif
#endif


 
#if !(((defined _WIN32 && !defined __CYGWIN__) || @HAVE_MBSINIT@) && @HAVE_MBRTOWC@) || @REPLACE_MBSTATE_T@
# if !GNULIB_defined_mbstate_t
#  if !(defined _AIX || defined _MSC_VER)
typedef int rpl_mbstate_t;
#   undef mbstate_t
#   define mbstate_t rpl_mbstate_t
#  endif
#  define GNULIB_defined_mbstate_t 1
# endif
#endif

 
#if @GNULIB_FREE_POSIX@
# if (@REPLACE_FREE@ && !defined free \
      && !(defined __cplusplus && defined GNULIB_NAMESPACE))
 
#  if defined __cplusplus && (__GLIBC__ + (__GLIBC_MINOR__ >= 14) > 2)
_GL_EXTERN_C void rpl_free (void *) throw ();
#  else
_GL_EXTERN_C void rpl_free (void *);
#  endif
#  undef _GL_ATTRIBUTE_DEALLOC_FREE
#  define _GL_ATTRIBUTE_DEALLOC_FREE _GL_ATTRIBUTE_DEALLOC (rpl_free, 1)
# else
#  if defined _MSC_VER && !defined free
_GL_EXTERN_C
#   if defined _DLL
     __declspec (dllimport)
#   endif
     void __cdecl free (void *);
#  else
#   if defined __cplusplus && (__GLIBC__ + (__GLIBC_MINOR__ >= 14) > 2)
_GL_EXTERN_C void free (void *) throw ();
#   else
_GL_EXTERN_C void free (void *);
#   endif
#  endif
# endif
#else
# if defined _MSC_VER && !defined free
_GL_EXTERN_C
#   if defined _DLL
     __declspec (dllimport)
#   endif
     void __cdecl free (void *);
# else
#  if defined __cplusplus && (__GLIBC__ + (__GLIBC_MINOR__ >= 14) > 2)
_GL_EXTERN_C void free (void *) throw ();
#  else
_GL_EXTERN_C void free (void *);
#  endif
# endif
#endif


#if @GNULIB_MBSZERO@
 
# include <string.h>
#endif


 
#if @GNULIB_BTOWC@
# if @REPLACE_BTOWC@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef btowc
#   define btowc rpl_btowc
#  endif
_GL_FUNCDECL_RPL (btowc, wint_t, (int c) _GL_ATTRIBUTE_PURE);
_GL_CXXALIAS_RPL (btowc, wint_t, (int c));
# else
#  if !@HAVE_BTOWC@
_GL_FUNCDECL_SYS (btowc, wint_t, (int c) _GL_ATTRIBUTE_PURE);
#  endif
 
_GL_CXXALIAS_SYS_CAST (btowc, wint_t, (int c));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (btowc);
# endif
#elif defined GNULIB_POSIXCHECK
# undef btowc
# if HAVE_RAW_DECL_BTOWC
_GL_WARN_ON_USE (btowc, "btowc is unportable - "
                 "use gnulib module btowc for portability");
# endif
#endif


 
#if @GNULIB_WCTOB@
# if @REPLACE_WCTOB@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wctob
#   define wctob rpl_wctob
#  endif
_GL_FUNCDECL_RPL (wctob, int, (wint_t wc) _GL_ATTRIBUTE_PURE);
_GL_CXXALIAS_RPL (wctob, int, (wint_t wc));
# else
#  if !defined wctob && !@HAVE_DECL_WCTOB@
 
_GL_FUNCDECL_SYS (wctob, int, (wint_t wc) _GL_ATTRIBUTE_PURE);
#  endif
_GL_CXXALIAS_SYS (wctob, int, (wint_t wc));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wctob);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wctob
# if HAVE_RAW_DECL_WCTOB
_GL_WARN_ON_USE (wctob, "wctob is unportable - "
                 "use gnulib module wctob for portability");
# endif
#endif


 
#if @GNULIB_MBSINIT@
# if @REPLACE_MBSINIT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mbsinit
#   define mbsinit rpl_mbsinit
#  endif
_GL_FUNCDECL_RPL (mbsinit, int, (const mbstate_t *ps));
_GL_CXXALIAS_RPL (mbsinit, int, (const mbstate_t *ps));
# else
#  if !@HAVE_MBSINIT@
_GL_FUNCDECL_SYS (mbsinit, int, (const mbstate_t *ps));
#  endif
_GL_CXXALIAS_SYS (mbsinit, int, (const mbstate_t *ps));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mbsinit);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mbsinit
# if HAVE_RAW_DECL_MBSINIT
_GL_WARN_ON_USE (mbsinit, "mbsinit is unportable - "
                 "use gnulib module mbsinit for portability");
# endif
#endif


 
#if @GNULIB_MBSZERO@
 
 
# if GNULIB_defined_mbstate_t                              
 
#  define _GL_MBSTATE_INIT_SIZE 1
 
# elif __GLIBC__ + (__GLIBC_MINOR__ >= 2) > 2              
 
#  define _GL_MBSTATE_INIT_SIZE 4  
#  define _GL_MBSTATE_ZERO_SIZE   sizeof (mbstate_t)
# elif defined MUSL_LIBC                                   
 
#  define _GL_MBSTATE_INIT_SIZE 4  
#  define _GL_MBSTATE_ZERO_SIZE 4
# elif defined __APPLE__ && defined __MACH__               
 
 
#  define _GL_MBSTATE_INIT_SIZE 12
#  define _GL_MBSTATE_ZERO_SIZE 12
# elif defined __FreeBSD__                                 
 
 
#  define _GL_MBSTATE_INIT_SIZE 12
#  define _GL_MBSTATE_ZERO_SIZE 12
# elif defined __NetBSD__                                  
 
 
 
#  define _GL_MBSTATE_ZERO_SIZE 28
# elif defined __OpenBSD__                                 
 
 
#  define _GL_MBSTATE_INIT_SIZE 12
#  define _GL_MBSTATE_ZERO_SIZE 12
# elif defined __minix                                     
 
 
 
#  define _GL_MBSTATE_ZERO_SIZE 4
# elif defined __sun                                       
 
 
 
# elif defined __CYGWIN__                                  
 
#  define _GL_MBSTATE_INIT_SIZE 4  
#  define _GL_MBSTATE_ZERO_SIZE 8
# elif defined _WIN32 && !defined __CYGWIN__               
 
# elif defined __ANDROID__                                 
 
#  define _GL_MBSTATE_INIT_SIZE 4
#  define _GL_MBSTATE_ZERO_SIZE 4
# endif
 
# ifndef _GL_MBSTATE_INIT_SIZE
#  define _GL_MBSTATE_INIT_SIZE sizeof (mbstate_t)
# endif
# ifndef _GL_MBSTATE_ZERO_SIZE
#  define _GL_MBSTATE_ZERO_SIZE sizeof (mbstate_t)
# endif
_GL_BEGIN_C_LINKAGE
# if defined IN_MBSZERO
_GL_EXTERN_INLINE
# else
_GL_INLINE
# endif
_GL_ARG_NONNULL ((1)) void
mbszero (mbstate_t *ps)
{
  memset (ps, 0, _GL_MBSTATE_ZERO_SIZE);
}
_GL_END_C_LINKAGE
_GL_CXXALIAS_SYS (mbszero, void, (mbstate_t *ps));
_GL_CXXALIASWARN (mbszero);
#endif


 
#if @GNULIB_MBRTOWC@
# if @REPLACE_MBRTOWC@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mbrtowc
#   define mbrtowc rpl_mbrtowc
#  endif
_GL_FUNCDECL_RPL (mbrtowc, size_t,
                  (wchar_t *restrict pwc, const char *restrict s, size_t n,
                   mbstate_t *restrict ps));
_GL_CXXALIAS_RPL (mbrtowc, size_t,
                  (wchar_t *restrict pwc, const char *restrict s, size_t n,
                   mbstate_t *restrict ps));
# else
#  if !@HAVE_MBRTOWC@
_GL_FUNCDECL_SYS (mbrtowc, size_t,
                  (wchar_t *restrict pwc, const char *restrict s, size_t n,
                   mbstate_t *restrict ps));
#  endif
_GL_CXXALIAS_SYS (mbrtowc, size_t,
                  (wchar_t *restrict pwc, const char *restrict s, size_t n,
                   mbstate_t *restrict ps));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mbrtowc);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mbrtowc
# if HAVE_RAW_DECL_MBRTOWC
_GL_WARN_ON_USE (mbrtowc, "mbrtowc is unportable - "
                 "use gnulib module mbrtowc for portability");
# endif
#endif


 
#if @GNULIB_MBRLEN@
# if @REPLACE_MBRLEN@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mbrlen
#   define mbrlen rpl_mbrlen
#  endif
_GL_FUNCDECL_RPL (mbrlen, size_t,
                  (const char *restrict s, size_t n, mbstate_t *restrict ps));
_GL_CXXALIAS_RPL (mbrlen, size_t,
                  (const char *restrict s, size_t n, mbstate_t *restrict ps));
# else
#  if !@HAVE_MBRLEN@
_GL_FUNCDECL_SYS (mbrlen, size_t,
                  (const char *restrict s, size_t n, mbstate_t *restrict ps));
#  endif
_GL_CXXALIAS_SYS (mbrlen, size_t,
                  (const char *restrict s, size_t n, mbstate_t *restrict ps));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mbrlen);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mbrlen
# if HAVE_RAW_DECL_MBRLEN
_GL_WARN_ON_USE (mbrlen, "mbrlen is unportable - "
                 "use gnulib module mbrlen for portability");
# endif
#endif


 
#if @GNULIB_MBSRTOWCS@
# if @REPLACE_MBSRTOWCS@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mbsrtowcs
#   define mbsrtowcs rpl_mbsrtowcs
#  endif
_GL_FUNCDECL_RPL (mbsrtowcs, size_t,
                  (wchar_t *restrict dest,
                   const char **restrict srcp, size_t len,
                   mbstate_t *restrict ps)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (mbsrtowcs, size_t,
                  (wchar_t *restrict dest,
                   const char **restrict srcp, size_t len,
                   mbstate_t *restrict ps));
# else
#  if !@HAVE_MBSRTOWCS@
_GL_FUNCDECL_SYS (mbsrtowcs, size_t,
                  (wchar_t *restrict dest,
                   const char **restrict srcp, size_t len,
                   mbstate_t *restrict ps)
                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (mbsrtowcs, size_t,
                  (wchar_t *restrict dest,
                   const char **restrict srcp, size_t len,
                   mbstate_t *restrict ps));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mbsrtowcs);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mbsrtowcs
# if HAVE_RAW_DECL_MBSRTOWCS
_GL_WARN_ON_USE (mbsrtowcs, "mbsrtowcs is unportable - "
                 "use gnulib module mbsrtowcs for portability");
# endif
#endif


 
#if @GNULIB_MBSNRTOWCS@
# if @REPLACE_MBSNRTOWCS@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mbsnrtowcs
#   define mbsnrtowcs rpl_mbsnrtowcs
#  endif
_GL_FUNCDECL_RPL (mbsnrtowcs, size_t,
                  (wchar_t *restrict dest,
                   const char **restrict srcp, size_t srclen, size_t len,
                   mbstate_t *restrict ps)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (mbsnrtowcs, size_t,
                  (wchar_t *restrict dest,
                   const char **restrict srcp, size_t srclen, size_t len,
                   mbstate_t *restrict ps));
# else
#  if !@HAVE_MBSNRTOWCS@
_GL_FUNCDECL_SYS (mbsnrtowcs, size_t,
                  (wchar_t *restrict dest,
                   const char **restrict srcp, size_t srclen, size_t len,
                   mbstate_t *restrict ps)
                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (mbsnrtowcs, size_t,
                  (wchar_t *restrict dest,
                   const char **restrict srcp, size_t srclen, size_t len,
                   mbstate_t *restrict ps));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mbsnrtowcs);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mbsnrtowcs
# if HAVE_RAW_DECL_MBSNRTOWCS
_GL_WARN_ON_USE (mbsnrtowcs, "mbsnrtowcs is unportable - "
                 "use gnulib module mbsnrtowcs for portability");
# endif
#endif


 
#if @GNULIB_WCRTOMB@
# if @REPLACE_WCRTOMB@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wcrtomb
#   define wcrtomb rpl_wcrtomb
#  endif
_GL_FUNCDECL_RPL (wcrtomb, size_t,
                  (char *restrict s, wchar_t wc, mbstate_t *restrict ps));
_GL_CXXALIAS_RPL (wcrtomb, size_t,
                  (char *restrict s, wchar_t wc, mbstate_t *restrict ps));
# else
#  if !@HAVE_WCRTOMB@
_GL_FUNCDECL_SYS (wcrtomb, size_t,
                  (char *restrict s, wchar_t wc, mbstate_t *restrict ps));
#  endif
_GL_CXXALIAS_SYS (wcrtomb, size_t,
                  (char *restrict s, wchar_t wc, mbstate_t *restrict ps));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcrtomb);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcrtomb
# if HAVE_RAW_DECL_WCRTOMB
_GL_WARN_ON_USE (wcrtomb, "wcrtomb is unportable - "
                 "use gnulib module wcrtomb for portability");
# endif
#endif


 
#if @GNULIB_WCSRTOMBS@
# if @REPLACE_WCSRTOMBS@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wcsrtombs
#   define wcsrtombs rpl_wcsrtombs
#  endif
_GL_FUNCDECL_RPL (wcsrtombs, size_t,
                  (char *restrict dest, const wchar_t **restrict srcp,
                   size_t len,
                   mbstate_t *restrict ps)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (wcsrtombs, size_t,
                  (char *restrict dest, const wchar_t **restrict srcp,
                   size_t len,
                   mbstate_t *restrict ps));
# else
#  if !@HAVE_WCSRTOMBS@
_GL_FUNCDECL_SYS (wcsrtombs, size_t,
                  (char *restrict dest, const wchar_t **restrict srcp,
                   size_t len,
                   mbstate_t *restrict ps)
                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (wcsrtombs, size_t,
                  (char *restrict dest, const wchar_t **restrict srcp,
                   size_t len,
                   mbstate_t *restrict ps));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcsrtombs);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcsrtombs
# if HAVE_RAW_DECL_WCSRTOMBS
_GL_WARN_ON_USE (wcsrtombs, "wcsrtombs is unportable - "
                 "use gnulib module wcsrtombs for portability");
# endif
#endif


 
#if @GNULIB_WCSNRTOMBS@
# if @REPLACE_WCSNRTOMBS@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wcsnrtombs
#   define wcsnrtombs rpl_wcsnrtombs
#  endif
_GL_FUNCDECL_RPL (wcsnrtombs, size_t,
                  (char *restrict dest,
                   const wchar_t **restrict srcp, size_t srclen,
                   size_t len,
                   mbstate_t *restrict ps)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (wcsnrtombs, size_t,
                  (char *restrict dest,
                   const wchar_t **restrict srcp, size_t srclen,
                   size_t len,
                   mbstate_t *restrict ps));
# else
#  if !@HAVE_WCSNRTOMBS@ || (defined __cplusplus && defined __sun)
_GL_FUNCDECL_SYS (wcsnrtombs, size_t,
                  (char *restrict dest,
                   const wchar_t **restrict srcp, size_t srclen,
                   size_t len,
                   mbstate_t *restrict ps)
                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (wcsnrtombs, size_t,
                  (char *restrict dest,
                   const wchar_t **restrict srcp, size_t srclen,
                   size_t len,
                   mbstate_t *restrict ps));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcsnrtombs);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcsnrtombs
# if HAVE_RAW_DECL_WCSNRTOMBS
_GL_WARN_ON_USE (wcsnrtombs, "wcsnrtombs is unportable - "
                 "use gnulib module wcsnrtombs for portability");
# endif
#endif


 
#if @GNULIB_WCWIDTH@
# if @REPLACE_WCWIDTH@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wcwidth
#   define wcwidth rpl_wcwidth
#  endif
_GL_FUNCDECL_RPL (wcwidth, int, (wchar_t) _GL_ATTRIBUTE_PURE);
_GL_CXXALIAS_RPL (wcwidth, int, (wchar_t));
# else
#  if !@HAVE_DECL_WCWIDTH@
 
_GL_FUNCDECL_SYS (wcwidth, int, (wchar_t) _GL_ATTRIBUTE_PURE);
#  endif
_GL_CXXALIAS_SYS (wcwidth, int, (wchar_t));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcwidth);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcwidth
# if HAVE_RAW_DECL_WCWIDTH
_GL_WARN_ON_USE (wcwidth, "wcwidth is unportable - "
                 "use gnulib module wcwidth for portability");
# endif
#endif


 
#if @GNULIB_WMEMCHR@
# if !@HAVE_WMEMCHR@
_GL_FUNCDECL_SYS (wmemchr, wchar_t *, (const wchar_t *s, wchar_t c, size_t n)
                                      _GL_ATTRIBUTE_PURE);
# endif
   
_GL_CXXALIAS_SYS_CAST2 (wmemchr,
                        wchar_t *, (const wchar_t *, wchar_t, size_t),
                        const wchar_t *, (const wchar_t *, wchar_t, size_t));
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
_GL_CXXALIASWARN1 (wmemchr, wchar_t *, (wchar_t *s, wchar_t c, size_t n));
_GL_CXXALIASWARN1 (wmemchr, const wchar_t *,
                   (const wchar_t *s, wchar_t c, size_t n));
# elif __GLIBC__ >= 2
_GL_CXXALIASWARN (wmemchr);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wmemchr
# if HAVE_RAW_DECL_WMEMCHR
_GL_WARN_ON_USE (wmemchr, "wmemchr is unportable - "
                 "use gnulib module wmemchr for portability");
# endif
#endif


 
#if @GNULIB_WMEMCMP@
# if @REPLACE_WMEMCMP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wmemcmp
#   define wmemcmp rpl_wmemcmp
#  endif
_GL_FUNCDECL_RPL (wmemcmp, int,
                  (const wchar_t *s1, const wchar_t *s2, size_t n)
                  _GL_ATTRIBUTE_PURE);
_GL_CXXALIAS_RPL (wmemcmp, int,
                  (const wchar_t *s1, const wchar_t *s2, size_t n));
# else
#  if !@HAVE_WMEMCMP@
_GL_FUNCDECL_SYS (wmemcmp, int,
                  (const wchar_t *s1, const wchar_t *s2, size_t n)
                  _GL_ATTRIBUTE_PURE);
#  endif
_GL_CXXALIAS_SYS (wmemcmp, int,
                  (const wchar_t *s1, const wchar_t *s2, size_t n));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wmemcmp);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wmemcmp
# if HAVE_RAW_DECL_WMEMCMP
_GL_WARN_ON_USE (wmemcmp, "wmemcmp is unportable - "
                 "use gnulib module wmemcmp for portability");
# endif
#endif


 
#if @GNULIB_WMEMCPY@
# if !@HAVE_WMEMCPY@
_GL_FUNCDECL_SYS (wmemcpy, wchar_t *,
                  (wchar_t *restrict dest,
                   const wchar_t *restrict src, size_t n));
# endif
_GL_CXXALIAS_SYS (wmemcpy, wchar_t *,
                  (wchar_t *restrict dest,
                   const wchar_t *restrict src, size_t n));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wmemcpy);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wmemcpy
# if HAVE_RAW_DECL_WMEMCPY
_GL_WARN_ON_USE (wmemcpy, "wmemcpy is unportable - "
                 "use gnulib module wmemcpy for portability");
# endif
#endif


 
#if @GNULIB_WMEMMOVE@
# if !@HAVE_WMEMMOVE@
_GL_FUNCDECL_SYS (wmemmove, wchar_t *,
                  (wchar_t *dest, const wchar_t *src, size_t n));
# endif
_GL_CXXALIAS_SYS (wmemmove, wchar_t *,
                  (wchar_t *dest, const wchar_t *src, size_t n));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wmemmove);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wmemmove
# if HAVE_RAW_DECL_WMEMMOVE
_GL_WARN_ON_USE (wmemmove, "wmemmove is unportable - "
                 "use gnulib module wmemmove for portability");
# endif
#endif


 
#if @GNULIB_WMEMPCPY@
# if @REPLACE_WMEMPCPY@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wmempcpy
#   define wmempcpy rpl_wmempcpy
#  endif
_GL_FUNCDECL_RPL (wmempcpy, wchar_t *,
                  (wchar_t *restrict dest,
                   const wchar_t *restrict src, size_t n));
_GL_CXXALIAS_RPL (wmempcpy, wchar_t *,
                  (wchar_t *restrict dest,
                   const wchar_t *restrict src, size_t n));
# else
#  if !@HAVE_WMEMPCPY@
_GL_FUNCDECL_SYS (wmempcpy, wchar_t *,
                  (wchar_t *restrict dest,
                   const wchar_t *restrict src, size_t n));
#  endif
_GL_CXXALIAS_SYS (wmempcpy, wchar_t *,
                  (wchar_t *restrict dest,
                   const wchar_t *restrict src, size_t n));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wmempcpy);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wmempcpy
# if HAVE_RAW_DECL_WMEMPCPY
_GL_WARN_ON_USE (wmempcpy, "wmempcpy is unportable - "
                 "use gnulib module wmempcpy for portability");
# endif
#endif


 
#if @GNULIB_WMEMSET@
# if !@HAVE_WMEMSET@
_GL_FUNCDECL_SYS (wmemset, wchar_t *, (wchar_t *s, wchar_t c, size_t n));
# endif
_GL_CXXALIAS_SYS (wmemset, wchar_t *, (wchar_t *s, wchar_t c, size_t n));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wmemset);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wmemset
# if HAVE_RAW_DECL_WMEMSET
_GL_WARN_ON_USE (wmemset, "wmemset is unportable - "
                 "use gnulib module wmemset for portability");
# endif
#endif


 
#if @GNULIB_WCSLEN@
# if !@HAVE_WCSLEN@
_GL_FUNCDECL_SYS (wcslen, size_t, (const wchar_t *s) _GL_ATTRIBUTE_PURE);
# endif
_GL_CXXALIAS_SYS (wcslen, size_t, (const wchar_t *s));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcslen);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcslen
# if HAVE_RAW_DECL_WCSLEN
_GL_WARN_ON_USE (wcslen, "wcslen is unportable - "
                 "use gnulib module wcslen for portability");
# endif
#endif


 
#if @GNULIB_WCSNLEN@
 
# if !@HAVE_WCSNLEN@ || (defined __sun && defined __cplusplus)
_GL_FUNCDECL_SYS (wcsnlen, size_t, (const wchar_t *s, size_t maxlen)
                                   _GL_ATTRIBUTE_PURE);
# endif
_GL_CXXALIAS_SYS (wcsnlen, size_t, (const wchar_t *s, size_t maxlen));
_GL_CXXALIASWARN (wcsnlen);
#elif defined GNULIB_POSIXCHECK
# undef wcsnlen
# if HAVE_RAW_DECL_WCSNLEN
_GL_WARN_ON_USE (wcsnlen, "wcsnlen is unportable - "
                 "use gnulib module wcsnlen for portability");
# endif
#endif


 
#if @GNULIB_WCSCPY@
# if !@HAVE_WCSCPY@
_GL_FUNCDECL_SYS (wcscpy, wchar_t *,
                  (wchar_t *restrict dest, const wchar_t *restrict src));
# endif
_GL_CXXALIAS_SYS (wcscpy, wchar_t *,
                  (wchar_t *restrict dest, const wchar_t *restrict src));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcscpy);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcscpy
# if HAVE_RAW_DECL_WCSCPY
_GL_WARN_ON_USE (wcscpy, "wcscpy is unportable - "
                 "use gnulib module wcscpy for portability");
# endif
#endif


 
#if @GNULIB_WCPCPY@
 
# if !@HAVE_WCPCPY@ || (defined __sun && defined __cplusplus)
_GL_FUNCDECL_SYS (wcpcpy, wchar_t *,
                  (wchar_t *restrict dest, const wchar_t *restrict src));
# endif
_GL_CXXALIAS_SYS (wcpcpy, wchar_t *,
                  (wchar_t *restrict dest, const wchar_t *restrict src));
_GL_CXXALIASWARN (wcpcpy);
#elif defined GNULIB_POSIXCHECK
# undef wcpcpy
# if HAVE_RAW_DECL_WCPCPY
_GL_WARN_ON_USE (wcpcpy, "wcpcpy is unportable - "
                 "use gnulib module wcpcpy for portability");
# endif
#endif


 
#if @GNULIB_WCSNCPY@
# if !@HAVE_WCSNCPY@
_GL_FUNCDECL_SYS (wcsncpy, wchar_t *,
                  (wchar_t *restrict dest,
                   const wchar_t *restrict src, size_t n));
# endif
_GL_CXXALIAS_SYS (wcsncpy, wchar_t *,
                  (wchar_t *restrict dest,
                   const wchar_t *restrict src, size_t n));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcsncpy);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcsncpy
# if HAVE_RAW_DECL_WCSNCPY
_GL_WARN_ON_USE (wcsncpy, "wcsncpy is unportable - "
                 "use gnulib module wcsncpy for portability");
# endif
#endif


 
#if @GNULIB_WCPNCPY@
 
# if !@HAVE_WCPNCPY@ || (defined __sun && defined __cplusplus)
_GL_FUNCDECL_SYS (wcpncpy, wchar_t *,
                  (wchar_t *restrict dest,
                   const wchar_t *restrict src, size_t n));
# endif
_GL_CXXALIAS_SYS (wcpncpy, wchar_t *,
                  (wchar_t *restrict dest,
                   const wchar_t *restrict src, size_t n));
_GL_CXXALIASWARN (wcpncpy);
#elif defined GNULIB_POSIXCHECK
# undef wcpncpy
# if HAVE_RAW_DECL_WCPNCPY
_GL_WARN_ON_USE (wcpncpy, "wcpncpy is unportable - "
                 "use gnulib module wcpncpy for portability");
# endif
#endif


 
#if @GNULIB_WCSCAT@
# if !@HAVE_WCSCAT@
_GL_FUNCDECL_SYS (wcscat, wchar_t *,
                  (wchar_t *restrict dest, const wchar_t *restrict src));
# endif
_GL_CXXALIAS_SYS (wcscat, wchar_t *,
                  (wchar_t *restrict dest, const wchar_t *restrict src));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcscat);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcscat
# if HAVE_RAW_DECL_WCSCAT
_GL_WARN_ON_USE (wcscat, "wcscat is unportable - "
                 "use gnulib module wcscat for portability");
# endif
#endif


 
#if @GNULIB_WCSNCAT@
# if !@HAVE_WCSNCAT@
_GL_FUNCDECL_SYS (wcsncat, wchar_t *,
                  (wchar_t *restrict dest, const wchar_t *restrict src,
                   size_t n));
# endif
_GL_CXXALIAS_SYS (wcsncat, wchar_t *,
                  (wchar_t *restrict dest, const wchar_t *restrict src,
                   size_t n));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcsncat);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcsncat
# if HAVE_RAW_DECL_WCSNCAT
_GL_WARN_ON_USE (wcsncat, "wcsncat is unportable - "
                 "use gnulib module wcsncat for portability");
# endif
#endif


 
#if @GNULIB_WCSCMP@
# if @REPLACE_WCSCMP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wcscmp
#   define wcscmp rpl_wcscmp
#  endif
_GL_FUNCDECL_RPL (wcscmp, int, (const wchar_t *s1, const wchar_t *s2)
                               _GL_ATTRIBUTE_PURE);
_GL_CXXALIAS_RPL (wcscmp, int, (const wchar_t *s1, const wchar_t *s2));
# else
#  if !@HAVE_WCSCMP@
_GL_FUNCDECL_SYS (wcscmp, int, (const wchar_t *s1, const wchar_t *s2)
                               _GL_ATTRIBUTE_PURE);
#  endif
_GL_CXXALIAS_SYS (wcscmp, int, (const wchar_t *s1, const wchar_t *s2));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcscmp);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcscmp
# if HAVE_RAW_DECL_WCSCMP
_GL_WARN_ON_USE (wcscmp, "wcscmp is unportable - "
                 "use gnulib module wcscmp for portability");
# endif
#endif


 
#if @GNULIB_WCSNCMP@
# if @REPLACE_WCSNCMP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wcsncmp
#   define wcsncmp rpl_wcsncmp
#  endif
_GL_FUNCDECL_RPL (wcsncmp, int,
                  (const wchar_t *s1, const wchar_t *s2, size_t n)
                  _GL_ATTRIBUTE_PURE);
_GL_CXXALIAS_RPL (wcsncmp, int,
                  (const wchar_t *s1, const wchar_t *s2, size_t n));
# else
#  if !@HAVE_WCSNCMP@
_GL_FUNCDECL_SYS (wcsncmp, int,
                  (const wchar_t *s1, const wchar_t *s2, size_t n)
                  _GL_ATTRIBUTE_PURE);
#  endif
_GL_CXXALIAS_SYS (wcsncmp, int,
                  (const wchar_t *s1, const wchar_t *s2, size_t n));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcsncmp);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcsncmp
# if HAVE_RAW_DECL_WCSNCMP
_GL_WARN_ON_USE (wcsncmp, "wcsncmp is unportable - "
                 "use gnulib module wcsncmp for portability");
# endif
#endif


 
#if @GNULIB_WCSCASECMP@
 
# if !@HAVE_WCSCASECMP@ || (defined __sun && defined __cplusplus)
_GL_FUNCDECL_SYS (wcscasecmp, int, (const wchar_t *s1, const wchar_t *s2)
                                   _GL_ATTRIBUTE_PURE);
# endif
_GL_CXXALIAS_SYS (wcscasecmp, int, (const wchar_t *s1, const wchar_t *s2));
_GL_CXXALIASWARN (wcscasecmp);
#elif defined GNULIB_POSIXCHECK
# undef wcscasecmp
# if HAVE_RAW_DECL_WCSCASECMP
_GL_WARN_ON_USE (wcscasecmp, "wcscasecmp is unportable - "
                 "use gnulib module wcscasecmp for portability");
# endif
#endif


 
#if @GNULIB_WCSNCASECMP@
 
# if !@HAVE_WCSNCASECMP@ || (defined __sun && defined __cplusplus)
_GL_FUNCDECL_SYS (wcsncasecmp, int,
                  (const wchar_t *s1, const wchar_t *s2, size_t n)
                  _GL_ATTRIBUTE_PURE);
# endif
_GL_CXXALIAS_SYS (wcsncasecmp, int,
                  (const wchar_t *s1, const wchar_t *s2, size_t n));
_GL_CXXALIASWARN (wcsncasecmp);
#elif defined GNULIB_POSIXCHECK
# undef wcsncasecmp
# if HAVE_RAW_DECL_WCSNCASECMP
_GL_WARN_ON_USE (wcsncasecmp, "wcsncasecmp is unportable - "
                 "use gnulib module wcsncasecmp for portability");
# endif
#endif


 
#if @GNULIB_WCSCOLL@
# if !@HAVE_WCSCOLL@
_GL_FUNCDECL_SYS (wcscoll, int, (const wchar_t *s1, const wchar_t *s2));
# endif
_GL_CXXALIAS_SYS (wcscoll, int, (const wchar_t *s1, const wchar_t *s2));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcscoll);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcscoll
# if HAVE_RAW_DECL_WCSCOLL
_GL_WARN_ON_USE (wcscoll, "wcscoll is unportable - "
                 "use gnulib module wcscoll for portability");
# endif
#endif


 
#if @GNULIB_WCSXFRM@
# if !@HAVE_WCSXFRM@
_GL_FUNCDECL_SYS (wcsxfrm, size_t,
                  (wchar_t *restrict s1, const wchar_t *restrict s2, size_t n));
# endif
_GL_CXXALIAS_SYS (wcsxfrm, size_t,
                  (wchar_t *restrict s1, const wchar_t *restrict s2, size_t n));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcsxfrm);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcsxfrm
# if HAVE_RAW_DECL_WCSXFRM
_GL_WARN_ON_USE (wcsxfrm, "wcsxfrm is unportable - "
                 "use gnulib module wcsxfrm for portability");
# endif
#endif


 
#if @GNULIB_WCSDUP@
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wcsdup
#   define wcsdup _wcsdup
#  endif
_GL_CXXALIAS_MDA (wcsdup, wchar_t *, (const wchar_t *s));
# else
 
#  if !@HAVE_WCSDUP@ || (defined __sun && defined __cplusplus) || __GNUC__ >= 11
_GL_FUNCDECL_SYS (wcsdup, wchar_t *,
                  (const wchar_t *s)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
#  endif
_GL_CXXALIAS_SYS (wcsdup, wchar_t *, (const wchar_t *s));
# endif
_GL_CXXALIASWARN (wcsdup);
#else
# if __GNUC__ >= 11 && !defined wcsdup
 
_GL_FUNCDECL_SYS (wcsdup, wchar_t *,
                  (const wchar_t *s)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
# endif
# if defined GNULIB_POSIXCHECK
#  undef wcsdup
#  if HAVE_RAW_DECL_WCSDUP
_GL_WARN_ON_USE (wcsdup, "wcsdup is unportable - "
                 "use gnulib module wcsdup for portability");
#  endif
# elif @GNULIB_MDA_WCSDUP@
 
#  if defined _WIN32 && !defined __CYGWIN__
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    undef wcsdup
#    define wcsdup _wcsdup
#   endif
_GL_CXXALIAS_MDA (wcsdup, wchar_t *, (const wchar_t *s));
#  else
_GL_FUNCDECL_SYS (wcsdup, wchar_t *,
                  (const wchar_t *s)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
#   if @HAVE_DECL_WCSDUP@
_GL_CXXALIAS_SYS (wcsdup, wchar_t *, (const wchar_t *s));
#   endif
#  endif
#  if (defined _WIN32 && !defined __CYGWIN__) || @HAVE_DECL_WCSDUP@
_GL_CXXALIASWARN (wcsdup);
#  endif
# endif
#endif


 
#if @GNULIB_WCSCHR@
# if !@HAVE_WCSCHR@
_GL_FUNCDECL_SYS (wcschr, wchar_t *, (const wchar_t *wcs, wchar_t wc)
                                     _GL_ATTRIBUTE_PURE);
# endif
   
_GL_CXXALIAS_SYS_CAST2 (wcschr,
                        wchar_t *, (const wchar_t *, wchar_t),
                        const wchar_t *, (const wchar_t *, wchar_t));
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
_GL_CXXALIASWARN1 (wcschr, wchar_t *, (wchar_t *wcs, wchar_t wc));
_GL_CXXALIASWARN1 (wcschr, const wchar_t *, (const wchar_t *wcs, wchar_t wc));
# elif __GLIBC__ >= 2
_GL_CXXALIASWARN (wcschr);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcschr
# if HAVE_RAW_DECL_WCSCHR
_GL_WARN_ON_USE (wcschr, "wcschr is unportable - "
                 "use gnulib module wcschr for portability");
# endif
#endif


 
#if @GNULIB_WCSRCHR@
# if !@HAVE_WCSRCHR@
_GL_FUNCDECL_SYS (wcsrchr, wchar_t *, (const wchar_t *wcs, wchar_t wc)
                                      _GL_ATTRIBUTE_PURE);
# endif
   
_GL_CXXALIAS_SYS_CAST2 (wcsrchr,
                        wchar_t *, (const wchar_t *, wchar_t),
                        const wchar_t *, (const wchar_t *, wchar_t));
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
_GL_CXXALIASWARN1 (wcsrchr, wchar_t *, (wchar_t *wcs, wchar_t wc));
_GL_CXXALIASWARN1 (wcsrchr, const wchar_t *, (const wchar_t *wcs, wchar_t wc));
# elif __GLIBC__ >= 2
_GL_CXXALIASWARN (wcsrchr);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcsrchr
# if HAVE_RAW_DECL_WCSRCHR
_GL_WARN_ON_USE (wcsrchr, "wcsrchr is unportable - "
                 "use gnulib module wcsrchr for portability");
# endif
#endif


 
#if @GNULIB_WCSCSPN@
# if !@HAVE_WCSCSPN@
_GL_FUNCDECL_SYS (wcscspn, size_t, (const wchar_t *wcs, const wchar_t *reject)
                                   _GL_ATTRIBUTE_PURE);
# endif
_GL_CXXALIAS_SYS (wcscspn, size_t, (const wchar_t *wcs, const wchar_t *reject));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcscspn);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcscspn
# if HAVE_RAW_DECL_WCSCSPN
_GL_WARN_ON_USE (wcscspn, "wcscspn is unportable - "
                 "use gnulib module wcscspn for portability");
# endif
#endif


 
#if @GNULIB_WCSSPN@
# if !@HAVE_WCSSPN@
_GL_FUNCDECL_SYS (wcsspn, size_t, (const wchar_t *wcs, const wchar_t *accept)
                                  _GL_ATTRIBUTE_PURE);
# endif
_GL_CXXALIAS_SYS (wcsspn, size_t, (const wchar_t *wcs, const wchar_t *accept));
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcsspn);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcsspn
# if HAVE_RAW_DECL_WCSSPN
_GL_WARN_ON_USE (wcsspn, "wcsspn is unportable - "
                 "use gnulib module wcsspn for portability");
# endif
#endif


 
#if @GNULIB_WCSPBRK@
# if !@HAVE_WCSPBRK@
_GL_FUNCDECL_SYS (wcspbrk, wchar_t *,
                  (const wchar_t *wcs, const wchar_t *accept)
                  _GL_ATTRIBUTE_PURE);
# endif
   
_GL_CXXALIAS_SYS_CAST2 (wcspbrk,
                        wchar_t *, (const wchar_t *, const wchar_t *),
                        const wchar_t *, (const wchar_t *, const wchar_t *));
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
_GL_CXXALIASWARN1 (wcspbrk, wchar_t *,
                   (wchar_t *wcs, const wchar_t *accept));
_GL_CXXALIASWARN1 (wcspbrk, const wchar_t *,
                   (const wchar_t *wcs, const wchar_t *accept));
# elif __GLIBC__ >= 2
_GL_CXXALIASWARN (wcspbrk);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcspbrk
# if HAVE_RAW_DECL_WCSPBRK
_GL_WARN_ON_USE (wcspbrk, "wcspbrk is unportable - "
                 "use gnulib module wcspbrk for portability");
# endif
#endif


 
#if @GNULIB_WCSSTR@
# if @REPLACE_WCSSTR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wcsstr
#   define wcsstr rpl_wcsstr
#  endif
_GL_FUNCDECL_RPL (wcsstr, wchar_t *,
                  (const wchar_t *restrict haystack,
                   const wchar_t *restrict needle)
                  _GL_ATTRIBUTE_PURE);
_GL_CXXALIAS_RPL (wcsstr, wchar_t *,
                  (const wchar_t *restrict haystack,
                   const wchar_t *restrict needle));
# else
#  if !@HAVE_WCSSTR@
_GL_FUNCDECL_SYS (wcsstr, wchar_t *,
                  (const wchar_t *restrict haystack,
                   const wchar_t *restrict needle)
                  _GL_ATTRIBUTE_PURE);
#  endif
   
_GL_CXXALIAS_SYS_CAST2 (wcsstr,
                        wchar_t *,
                        (const wchar_t *restrict, const wchar_t *restrict),
                        const wchar_t *,
                        (const wchar_t *restrict, const wchar_t *restrict));
# endif
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
_GL_CXXALIASWARN1 (wcsstr, wchar_t *,
                   (wchar_t *restrict haystack,
                    const wchar_t *restrict needle));
_GL_CXXALIASWARN1 (wcsstr, const wchar_t *,
                   (const wchar_t *restrict haystack,
                    const wchar_t *restrict needle));
# elif __GLIBC__ >= 2
_GL_CXXALIASWARN (wcsstr);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcsstr
# if HAVE_RAW_DECL_WCSSTR
_GL_WARN_ON_USE (wcsstr, "wcsstr is unportable - "
                 "use gnulib module wcsstr for portability");
# endif
#endif


 
#if @GNULIB_WCSTOK@
# if @REPLACE_WCSTOK@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wcstok
#   define wcstok rpl_wcstok
#  endif
_GL_FUNCDECL_RPL (wcstok, wchar_t *,
                  (wchar_t *restrict wcs, const wchar_t *restrict delim,
                   wchar_t **restrict ptr));
_GL_CXXALIAS_RPL (wcstok, wchar_t *,
                  (wchar_t *restrict wcs, const wchar_t *restrict delim,
                   wchar_t **restrict ptr));
# else
#  if !@HAVE_WCSTOK@
_GL_FUNCDECL_SYS (wcstok, wchar_t *,
                  (wchar_t *restrict wcs, const wchar_t *restrict delim,
                   wchar_t **restrict ptr));
#  endif
_GL_CXXALIAS_SYS (wcstok, wchar_t *,
                  (wchar_t *restrict wcs, const wchar_t *restrict delim,
                   wchar_t **restrict ptr));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcstok);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcstok
# if HAVE_RAW_DECL_WCSTOK
_GL_WARN_ON_USE (wcstok, "wcstok is unportable - "
                 "use gnulib module wcstok for portability");
# endif
#endif


 
#if @GNULIB_WCSWIDTH@
# if @REPLACE_WCSWIDTH@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef wcswidth
#   define wcswidth rpl_wcswidth
#  endif
_GL_FUNCDECL_RPL (wcswidth, int, (const wchar_t *s, size_t n)
                                 _GL_ATTRIBUTE_PURE);
_GL_CXXALIAS_RPL (wcswidth, int, (const wchar_t *s, size_t n));
# else
#  if !@HAVE_WCSWIDTH@
_GL_FUNCDECL_SYS (wcswidth, int, (const wchar_t *s, size_t n)
                                 _GL_ATTRIBUTE_PURE);
#  endif
_GL_CXXALIAS_SYS (wcswidth, int, (const wchar_t *s, size_t n));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (wcswidth);
# endif
#elif defined GNULIB_POSIXCHECK
# undef wcswidth
# if HAVE_RAW_DECL_WCSWIDTH
_GL_WARN_ON_USE (wcswidth, "wcswidth is unportable - "
                 "use gnulib module wcswidth for portability");
# endif
#endif


 
#endif  
#endif
