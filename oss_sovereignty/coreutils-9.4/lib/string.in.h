 

#@INCLUDE_NEXT@ @NEXT_STRING_H@

#else
 

#ifndef _@GUARD_PREFIX@_STRING_H

#define _GL_ALREADY_INCLUDING_STRING_H

 
#@INCLUDE_NEXT@ @NEXT_STRING_H@

#undef _GL_ALREADY_INCLUDING_STRING_H

#ifndef _@GUARD_PREFIX@_STRING_H
#define _@GUARD_PREFIX@_STRING_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <stddef.h>

 
#if @GNULIB_MBSLEN@ && defined __MirBSD__
# include <wchar.h>
#endif

 
 
#if (@GNULIB_STRSIGNAL@ || defined GNULIB_POSIXCHECK) && defined __NetBSD__ \
    && ! defined __GLIBC__
# include <unistd.h>
#endif

 
 
#if ((@GNULIB_FFSL@ || @GNULIB_FFSLL@ || defined GNULIB_POSIXCHECK) \
     && (defined _AIX || defined __ANDROID__)) \
    && ! defined __GLIBC__
# include <strings.h>
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

 
#if @GNULIB_EXPLICIT_BZERO@
# if ! @HAVE_EXPLICIT_BZERO@
_GL_FUNCDECL_SYS (explicit_bzero, void,
                  (void *__dest, size_t __n) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (explicit_bzero, void, (void *__dest, size_t __n));
_GL_CXXALIASWARN (explicit_bzero);
#elif defined GNULIB_POSIXCHECK
# undef explicit_bzero
# if HAVE_RAW_DECL_EXPLICIT_BZERO
_GL_WARN_ON_USE (explicit_bzero, "explicit_bzero is unportable - "
                 "use gnulib module explicit_bzero for portability");
# endif
#endif

 
#if @GNULIB_FFSL@
# if !@HAVE_FFSL@
_GL_FUNCDECL_SYS (ffsl, int, (long int i));
# endif
_GL_CXXALIAS_SYS (ffsl, int, (long int i));
_GL_CXXALIASWARN (ffsl);
#elif defined GNULIB_POSIXCHECK
# undef ffsl
# if HAVE_RAW_DECL_FFSL
_GL_WARN_ON_USE (ffsl, "ffsl is not portable - use the ffsl module");
# endif
#endif


 
#if @GNULIB_FFSLL@
# if @REPLACE_FFSLL@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define ffsll rpl_ffsll
#  endif
_GL_FUNCDECL_RPL (ffsll, int, (long long int i));
_GL_CXXALIAS_RPL (ffsll, int, (long long int i));
# else
#  if !@HAVE_FFSLL@
_GL_FUNCDECL_SYS (ffsll, int, (long long int i));
#  endif
_GL_CXXALIAS_SYS (ffsll, int, (long long int i));
# endif
_GL_CXXALIASWARN (ffsll);
#elif defined GNULIB_POSIXCHECK
# undef ffsll
# if HAVE_RAW_DECL_FFSLL
_GL_WARN_ON_USE (ffsll, "ffsll is not portable - use the ffsll module");
# endif
#endif


#if @GNULIB_MDA_MEMCCPY@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef memccpy
#   define memccpy _memccpy
#  endif
_GL_CXXALIAS_MDA (memccpy, void *,
                  (void *dest, const void *src, int c, size_t n));
# else
_GL_CXXALIAS_SYS (memccpy, void *,
                  (void *dest, const void *src, int c, size_t n));
# endif
_GL_CXXALIASWARN (memccpy);
#endif


 
#if @GNULIB_MEMCHR@
# if @REPLACE_MEMCHR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef memchr
#   define memchr rpl_memchr
#  endif
_GL_FUNCDECL_RPL (memchr, void *, (void const *__s, int __c, size_t __n)
                                  _GL_ATTRIBUTE_PURE
                                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (memchr, void *, (void const *__s, int __c, size_t __n));
# else
   
_GL_CXXALIAS_SYS_CAST2 (memchr,
                        void *, (void const *__s, int __c, size_t __n),
                        void const *, (void const *__s, int __c, size_t __n));
# endif
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4) \
         || defined __clang__)
_GL_CXXALIASWARN1 (memchr, void *, (void *__s, int __c, size_t __n) throw ());
_GL_CXXALIASWARN1 (memchr, void const *,
                   (void const *__s, int __c, size_t __n) throw ());
# elif __GLIBC__ >= 2
_GL_CXXALIASWARN (memchr);
# endif
#elif defined GNULIB_POSIXCHECK
# undef memchr
 
_GL_WARN_ON_USE (memchr, "memchr has platform-specific bugs - "
                 "use gnulib module memchr for portability" );
#endif

 
#if @GNULIB_MEMMEM@
# if @REPLACE_MEMMEM@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define memmem rpl_memmem
#  endif
_GL_FUNCDECL_RPL (memmem, void *,
                  (void const *__haystack, size_t __haystack_len,
                   void const *__needle, size_t __needle_len)
                  _GL_ATTRIBUTE_PURE
                  _GL_ARG_NONNULL ((1, 3)));
_GL_CXXALIAS_RPL (memmem, void *,
                  (void const *__haystack, size_t __haystack_len,
                   void const *__needle, size_t __needle_len));
# else
#  if ! @HAVE_DECL_MEMMEM@
_GL_FUNCDECL_SYS (memmem, void *,
                  (void const *__haystack, size_t __haystack_len,
                   void const *__needle, size_t __needle_len)
                  _GL_ATTRIBUTE_PURE
                  _GL_ARG_NONNULL ((1, 3)));
#  endif
_GL_CXXALIAS_SYS (memmem, void *,
                  (void const *__haystack, size_t __haystack_len,
                   void const *__needle, size_t __needle_len));
# endif
_GL_CXXALIASWARN (memmem);
#elif defined GNULIB_POSIXCHECK
# undef memmem
# if HAVE_RAW_DECL_MEMMEM
_GL_WARN_ON_USE (memmem, "memmem is unportable and often quadratic - "
                 "use gnulib module memmem-simple for portability, "
                 "and module memmem for speed" );
# endif
#endif

 
#if @GNULIB_MEMPCPY@
# if @REPLACE_MEMPCPY@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mempcpy
#   define mempcpy rpl_mempcpy
#  endif
_GL_FUNCDECL_RPL (mempcpy, void *,
                  (void *restrict __dest, void const *restrict __src,
                   size_t __n)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (mempcpy, void *,
                  (void *restrict __dest, void const *restrict __src,
                   size_t __n));
# else
#  if !@HAVE_MEMPCPY@
_GL_FUNCDECL_SYS (mempcpy, void *,
                  (void *restrict __dest, void const *restrict __src,
                   size_t __n)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (mempcpy, void *,
                  (void *restrict __dest, void const *restrict __src,
                   size_t __n));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mempcpy);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mempcpy
# if HAVE_RAW_DECL_MEMPCPY
_GL_WARN_ON_USE (mempcpy, "mempcpy is unportable - "
                 "use gnulib module mempcpy for portability");
# endif
#endif

 
#if @GNULIB_MEMRCHR@
# if ! @HAVE_DECL_MEMRCHR@
_GL_FUNCDECL_SYS (memrchr, void *, (void const *, int, size_t)
                                   _GL_ATTRIBUTE_PURE
                                   _GL_ARG_NONNULL ((1)));
# endif
   
_GL_CXXALIAS_SYS_CAST2 (memrchr,
                        void *, (void const *, int, size_t),
                        void const *, (void const *, int, size_t));
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4) \
         || defined __clang__)
_GL_CXXALIASWARN1 (memrchr, void *, (void *, int, size_t) throw ());
_GL_CXXALIASWARN1 (memrchr, void const *, (void const *, int, size_t) throw ());
# elif __GLIBC__ >= 2
_GL_CXXALIASWARN (memrchr);
# endif
#elif defined GNULIB_POSIXCHECK
# undef memrchr
# if HAVE_RAW_DECL_MEMRCHR
_GL_WARN_ON_USE (memrchr, "memrchr is unportable - "
                 "use gnulib module memrchr for portability");
# endif
#endif

 
#if @GNULIB_MEMSET_EXPLICIT@
# if ! @HAVE_MEMSET_EXPLICIT@
_GL_FUNCDECL_SYS (memset_explicit, void *,
                  (void *__dest, int __c, size_t __n) _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (memset_explicit, void *, (void *__dest, int __c, size_t __n));
_GL_CXXALIASWARN (memset_explicit);
#elif defined GNULIB_POSIXCHECK
# undef memset_explicit
# if HAVE_RAW_DECL_MEMSET_EXPLICIT
_GL_WARN_ON_USE (memset_explicit, "memset_explicit is unportable - "
                 "use gnulib module memset_explicit for portability");
# endif
#endif

 
#if @GNULIB_RAWMEMCHR@
# if ! @HAVE_RAWMEMCHR@
_GL_FUNCDECL_SYS (rawmemchr, void *, (void const *__s, int __c_in)
                                     _GL_ATTRIBUTE_PURE
                                     _GL_ARG_NONNULL ((1)));
# endif
   
_GL_CXXALIAS_SYS_CAST2 (rawmemchr,
                        void *, (void const *__s, int __c_in),
                        void const *, (void const *__s, int __c_in));
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4) \
         || defined __clang__)
_GL_CXXALIASWARN1 (rawmemchr, void *, (void *__s, int __c_in) throw ());
_GL_CXXALIASWARN1 (rawmemchr, void const *,
                   (void const *__s, int __c_in) throw ());
# else
_GL_CXXALIASWARN (rawmemchr);
# endif
#elif defined GNULIB_POSIXCHECK
# undef rawmemchr
# if HAVE_RAW_DECL_RAWMEMCHR
_GL_WARN_ON_USE (rawmemchr, "rawmemchr is unportable - "
                 "use gnulib module rawmemchr for portability");
# endif
#endif

 
#if @GNULIB_STPCPY@
# if @REPLACE_STPCPY@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef stpcpy
#   define stpcpy rpl_stpcpy
#  endif
_GL_FUNCDECL_RPL (stpcpy, char *,
                  (char *restrict __dst, char const *restrict __src)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (stpcpy, char *,
                  (char *restrict __dst, char const *restrict __src));
# else
#  if !@HAVE_STPCPY@
_GL_FUNCDECL_SYS (stpcpy, char *,
                  (char *restrict __dst, char const *restrict __src)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (stpcpy, char *,
                  (char *restrict __dst, char const *restrict __src));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (stpcpy);
# endif
#elif defined GNULIB_POSIXCHECK
# undef stpcpy
# if HAVE_RAW_DECL_STPCPY
_GL_WARN_ON_USE (stpcpy, "stpcpy is unportable - "
                 "use gnulib module stpcpy for portability");
# endif
#endif

 
#if @GNULIB_STPNCPY@
# if @REPLACE_STPNCPY@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef stpncpy
#   define stpncpy rpl_stpncpy
#  endif
_GL_FUNCDECL_RPL (stpncpy, char *,
                  (char *restrict __dst, char const *restrict __src,
                   size_t __n)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (stpncpy, char *,
                  (char *restrict __dst, char const *restrict __src,
                   size_t __n));
# else
#  if ! @HAVE_STPNCPY@
_GL_FUNCDECL_SYS (stpncpy, char *,
                  (char *restrict __dst, char const *restrict __src,
                   size_t __n)
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
_GL_CXXALIAS_SYS (stpncpy, char *,
                  (char *restrict __dst, char const *restrict __src,
                   size_t __n));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (stpncpy);
# endif
#elif defined GNULIB_POSIXCHECK
# undef stpncpy
# if HAVE_RAW_DECL_STPNCPY
_GL_WARN_ON_USE (stpncpy, "stpncpy is unportable - "
                 "use gnulib module stpncpy for portability");
# endif
#endif

#if defined GNULIB_POSIXCHECK
 
# undef strchr
 
_GL_WARN_ON_USE_CXX (strchr,
                     const char *, char *, (const char *, int),
                     "strchr cannot work correctly on character strings "
                     "in some multibyte locales - "
                     "use mbschr if you care about internationalization");
#endif

 
#if @GNULIB_STRCHRNUL@
# if @REPLACE_STRCHRNUL@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define strchrnul rpl_strchrnul
#  endif
_GL_FUNCDECL_RPL (strchrnul, char *, (const char *__s, int __c_in)
                                     _GL_ATTRIBUTE_PURE
                                     _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (strchrnul, char *,
                  (const char *str, int ch));
# else
#  if ! @HAVE_STRCHRNUL@
_GL_FUNCDECL_SYS (strchrnul, char *, (char const *__s, int __c_in)
                                     _GL_ATTRIBUTE_PURE
                                     _GL_ARG_NONNULL ((1)));
#  endif
   
_GL_CXXALIAS_SYS_CAST2 (strchrnul,
                        char *, (char const *__s, int __c_in),
                        char const *, (char const *__s, int __c_in));
# endif
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4) \
         || defined __clang__)
_GL_CXXALIASWARN1 (strchrnul, char *, (char *__s, int __c_in) throw ());
_GL_CXXALIASWARN1 (strchrnul, char const *,
                   (char const *__s, int __c_in) throw ());
# elif __GLIBC__ >= 2
_GL_CXXALIASWARN (strchrnul);
# endif
#elif defined GNULIB_POSIXCHECK
# undef strchrnul
# if HAVE_RAW_DECL_STRCHRNUL
_GL_WARN_ON_USE (strchrnul, "strchrnul is unportable - "
                 "use gnulib module strchrnul for portability");
# endif
#endif

 
#if @GNULIB_STRDUP@
# if @REPLACE_STRDUP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef strdup
#   define strdup rpl_strdup
#  endif
_GL_FUNCDECL_RPL (strdup, char *,
                  (char const *__s)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
_GL_CXXALIAS_RPL (strdup, char *, (char const *__s));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef strdup
#   define strdup _strdup
#  endif
_GL_CXXALIAS_MDA (strdup, char *, (char const *__s));
# else
#  if defined __cplusplus && defined GNULIB_NAMESPACE && defined strdup
     
#   undef strdup
#  endif
#  if (!@HAVE_DECL_STRDUP@ || __GNUC__ >= 11) && !defined strdup
_GL_FUNCDECL_SYS (strdup, char *,
                  (char const *__s)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
#  endif
_GL_CXXALIAS_SYS (strdup, char *, (char const *__s));
# endif
_GL_CXXALIASWARN (strdup);
#else
# if __GNUC__ >= 11 && !defined strdup
 
_GL_FUNCDECL_SYS (strdup, char *,
                  (char const *__s)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
# endif
# if defined GNULIB_POSIXCHECK
#  undef strdup
#  if HAVE_RAW_DECL_STRDUP
_GL_WARN_ON_USE (strdup, "strdup is unportable - "
                 "use gnulib module strdup for portability");
#  endif
# elif @GNULIB_MDA_STRDUP@
 
#  if defined _WIN32 && !defined __CYGWIN__
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    undef strdup
#    define strdup _strdup
#   endif
_GL_CXXALIAS_MDA (strdup, char *, (char const *__s));
#  else
#   if defined __cplusplus && defined GNULIB_NAMESPACE && defined strdup
#    undef strdup
#   endif
_GL_CXXALIAS_SYS (strdup, char *, (char const *__s));
#  endif
_GL_CXXALIASWARN (strdup);
# endif
#endif

 
#if @GNULIB_STRNCAT@
# if @REPLACE_STRNCAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef strncat
#   define strncat rpl_strncat
#  endif
_GL_FUNCDECL_RPL (strncat, char *,
                  (char *restrict dest, const char *restrict src, size_t n)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (strncat, char *,
                  (char *restrict dest, const char *restrict src, size_t n));
# else
_GL_CXXALIAS_SYS (strncat, char *,
                  (char *restrict dest, const char *restrict src, size_t n));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (strncat);
# endif
#elif defined GNULIB_POSIXCHECK
# undef strncat
# if HAVE_RAW_DECL_STRNCAT
_GL_WARN_ON_USE (strncat, "strncat is unportable - "
                 "use gnulib module strncat for portability");
# endif
#endif

 
#if @GNULIB_STRNDUP@
# if @REPLACE_STRNDUP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef strndup
#   define strndup rpl_strndup
#  endif
_GL_FUNCDECL_RPL (strndup, char *,
                  (char const *__s, size_t __n)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
_GL_CXXALIAS_RPL (strndup, char *, (char const *__s, size_t __n));
# else
#  if !@HAVE_DECL_STRNDUP@ || (__GNUC__ >= 11 && !defined strndup)
_GL_FUNCDECL_SYS (strndup, char *,
                  (char const *__s, size_t __n)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
#  endif
_GL_CXXALIAS_SYS (strndup, char *, (char const *__s, size_t __n));
# endif
_GL_CXXALIASWARN (strndup);
#else
# if __GNUC__ >= 11 && !defined strndup
 
_GL_FUNCDECL_SYS (strndup, char *,
                  (char const *__s, size_t __n)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE);
# endif
# if defined GNULIB_POSIXCHECK
#  undef strndup
#  if HAVE_RAW_DECL_STRNDUP
_GL_WARN_ON_USE (strndup, "strndup is unportable - "
                 "use gnulib module strndup for portability");
#  endif
# endif
#endif

 
#if @GNULIB_STRNLEN@
# if @REPLACE_STRNLEN@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef strnlen
#   define strnlen rpl_strnlen
#  endif
_GL_FUNCDECL_RPL (strnlen, size_t, (char const *__s, size_t __maxlen)
                                   _GL_ATTRIBUTE_PURE
                                   _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (strnlen, size_t, (char const *__s, size_t __maxlen));
# else
#  if ! @HAVE_DECL_STRNLEN@
_GL_FUNCDECL_SYS (strnlen, size_t, (char const *__s, size_t __maxlen)
                                   _GL_ATTRIBUTE_PURE
                                   _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (strnlen, size_t, (char const *__s, size_t __maxlen));
# endif
_GL_CXXALIASWARN (strnlen);
#elif defined GNULIB_POSIXCHECK
# undef strnlen
# if HAVE_RAW_DECL_STRNLEN
_GL_WARN_ON_USE (strnlen, "strnlen is unportable - "
                 "use gnulib module strnlen for portability");
# endif
#endif

#if defined GNULIB_POSIXCHECK
 
# undef strcspn
 
_GL_WARN_ON_USE (strcspn, "strcspn cannot work correctly on character strings "
                 "in multibyte locales - "
                 "use mbscspn if you care about internationalization");
#endif

 
#if @GNULIB_STRPBRK@
# if ! @HAVE_STRPBRK@
_GL_FUNCDECL_SYS (strpbrk, char *, (char const *__s, char const *__accept)
                                   _GL_ATTRIBUTE_PURE
                                   _GL_ARG_NONNULL ((1, 2)));
# endif
   
_GL_CXXALIAS_SYS_CAST2 (strpbrk,
                        char *, (char const *__s, char const *__accept),
                        const char *, (char const *__s, char const *__accept));
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4) \
         || defined __clang__)
_GL_CXXALIASWARN1 (strpbrk, char *, (char *__s, char const *__accept) throw ());
_GL_CXXALIASWARN1 (strpbrk, char const *,
                   (char const *__s, char const *__accept) throw ());
# elif __GLIBC__ >= 2
_GL_CXXALIASWARN (strpbrk);
# endif
# if defined GNULIB_POSIXCHECK
 
#  undef strpbrk
_GL_WARN_ON_USE_CXX (strpbrk,
                     const char *, char *, (const char *, const char *),
                     "strpbrk cannot work correctly on character strings "
                     "in multibyte locales - "
                     "use mbspbrk if you care about internationalization");
# endif
#elif defined GNULIB_POSIXCHECK
# undef strpbrk
# if HAVE_RAW_DECL_STRPBRK
_GL_WARN_ON_USE_CXX (strpbrk,
                     const char *, char *, (const char *, const char *),
                     "strpbrk is unportable - "
                     "use gnulib module strpbrk for portability");
# endif
#endif

#if defined GNULIB_POSIXCHECK
 
# undef strspn
 
_GL_WARN_ON_USE (strspn, "strspn cannot work correctly on character strings "
                 "in multibyte locales - "
                 "use mbsspn if you care about internationalization");
#endif

#if defined GNULIB_POSIXCHECK
 
# undef strrchr
 
_GL_WARN_ON_USE_CXX (strrchr,
                     const char *, char *, (const char *, int),
                     "strrchr cannot work correctly on character strings "
                     "in some multibyte locales - "
                     "use mbsrchr if you care about internationalization");
#endif

 
#if @GNULIB_STRSEP@
# if ! @HAVE_STRSEP@
_GL_FUNCDECL_SYS (strsep, char *,
                  (char **restrict __stringp, char const *restrict __delim)
                  _GL_ARG_NONNULL ((1, 2)));
# endif
_GL_CXXALIAS_SYS (strsep, char *,
                  (char **restrict __stringp, char const *restrict __delim));
_GL_CXXALIASWARN (strsep);
# if defined GNULIB_POSIXCHECK
#  undef strsep
_GL_WARN_ON_USE (strsep, "strsep cannot work correctly on character strings "
                 "in multibyte locales - "
                 "use mbssep if you care about internationalization");
# endif
#elif defined GNULIB_POSIXCHECK
# undef strsep
# if HAVE_RAW_DECL_STRSEP
_GL_WARN_ON_USE (strsep, "strsep is unportable - "
                 "use gnulib module strsep for portability");
# endif
#endif

#if @GNULIB_STRSTR@
# if @REPLACE_STRSTR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define strstr rpl_strstr
#  endif
_GL_FUNCDECL_RPL (strstr, char *, (const char *haystack, const char *needle)
                                  _GL_ATTRIBUTE_PURE
                                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (strstr, char *, (const char *haystack, const char *needle));
# else
   
_GL_CXXALIAS_SYS_CAST2 (strstr,
                        char *, (const char *haystack, const char *needle),
                        const char *, (const char *haystack, const char *needle));
# endif
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4) \
         || defined __clang__)
_GL_CXXALIASWARN1 (strstr, char *,
                   (char *haystack, const char *needle) throw ());
_GL_CXXALIASWARN1 (strstr, const char *,
                   (const char *haystack, const char *needle) throw ());
# elif __GLIBC__ >= 2
_GL_CXXALIASWARN (strstr);
# endif
#elif defined GNULIB_POSIXCHECK
 
# undef strstr
 
_GL_WARN_ON_USE (strstr, "strstr is quadratic on many systems, and cannot "
                 "work correctly on character strings in most "
                 "multibyte locales - "
                 "use mbsstr if you care about internationalization, "
                 "or use strstr if you care about speed");
#endif

 
#if @GNULIB_STRCASESTR@
# if @REPLACE_STRCASESTR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define strcasestr rpl_strcasestr
#  endif
_GL_FUNCDECL_RPL (strcasestr, char *,
                  (const char *haystack, const char *needle)
                  _GL_ATTRIBUTE_PURE
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (strcasestr, char *,
                  (const char *haystack, const char *needle));
# else
#  if ! @HAVE_STRCASESTR@
_GL_FUNCDECL_SYS (strcasestr, char *,
                  (const char *haystack, const char *needle)
                  _GL_ATTRIBUTE_PURE
                  _GL_ARG_NONNULL ((1, 2)));
#  endif
   
_GL_CXXALIAS_SYS_CAST2 (strcasestr,
                        char *, (const char *haystack, const char *needle),
                        const char *, (const char *haystack, const char *needle));
# endif
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 10) && !defined __UCLIBC__) \
     && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4) \
         || defined __clang__)
_GL_CXXALIASWARN1 (strcasestr, char *,
                   (char *haystack, const char *needle) throw ());
_GL_CXXALIASWARN1 (strcasestr, const char *,
                   (const char *haystack, const char *needle) throw ());
# elif __GLIBC__ >= 2
_GL_CXXALIASWARN (strcasestr);
# endif
#elif defined GNULIB_POSIXCHECK
 
# undef strcasestr
# if HAVE_RAW_DECL_STRCASESTR
_GL_WARN_ON_USE (strcasestr, "strcasestr does work correctly on character "
                 "strings in multibyte locales - "
                 "use mbscasestr if you care about "
                 "internationalization, or use c-strcasestr if you want "
                 "a locale independent function");
# endif
#endif

 
#if @GNULIB_STRTOK_R@
# if @REPLACE_STRTOK_R@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef strtok_r
#   define strtok_r rpl_strtok_r
#  endif
_GL_FUNCDECL_RPL (strtok_r, char *,
                  (char *restrict s, char const *restrict delim,
                   char **restrict save_ptr)
                  _GL_ARG_NONNULL ((2, 3)));
_GL_CXXALIAS_RPL (strtok_r, char *,
                  (char *restrict s, char const *restrict delim,
                   char **restrict save_ptr));
# else
#  if @UNDEFINE_STRTOK_R@ || defined GNULIB_POSIXCHECK
#   undef strtok_r
#  endif
#  if ! @HAVE_DECL_STRTOK_R@
_GL_FUNCDECL_SYS (strtok_r, char *,
                  (char *restrict s, char const *restrict delim,
                   char **restrict save_ptr)
                  _GL_ARG_NONNULL ((2, 3)));
#  endif
_GL_CXXALIAS_SYS (strtok_r, char *,
                  (char *restrict s, char const *restrict delim,
                   char **restrict save_ptr));
# endif
_GL_CXXALIASWARN (strtok_r);
# if defined GNULIB_POSIXCHECK
_GL_WARN_ON_USE (strtok_r, "strtok_r cannot work correctly on character "
                 "strings in multibyte locales - "
                 "use mbstok_r if you care about internationalization");
# endif
#elif defined GNULIB_POSIXCHECK
# undef strtok_r
# if HAVE_RAW_DECL_STRTOK_R
_GL_WARN_ON_USE (strtok_r, "strtok_r is unportable - "
                 "use gnulib module strtok_r for portability");
# endif
#endif


 

#if @GNULIB_MBSLEN@
 
# ifdef __MirBSD__   
#  undef mbslen
# endif
# if @HAVE_MBSLEN@   
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define mbslen rpl_mbslen
#  endif
_GL_FUNCDECL_RPL (mbslen, size_t, (const char *string)
                                  _GL_ATTRIBUTE_PURE
                                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (mbslen, size_t, (const char *string));
# else
_GL_FUNCDECL_SYS (mbslen, size_t, (const char *string)
                                  _GL_ATTRIBUTE_PURE
                                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_SYS (mbslen, size_t, (const char *string));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mbslen);
# endif
#endif

#if @GNULIB_MBSNLEN@
 
_GL_EXTERN_C size_t mbsnlen (const char *string, size_t len)
     _GL_ATTRIBUTE_PURE
     _GL_ARG_NONNULL ((1));
#endif

#if @GNULIB_MBSCHR@
 
# if defined __hpux
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define mbschr rpl_mbschr  
#  endif
_GL_FUNCDECL_RPL (mbschr, char *, (const char *string, int c)
                                  _GL_ATTRIBUTE_PURE
                                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (mbschr, char *, (const char *string, int c));
# else
_GL_FUNCDECL_SYS (mbschr, char *, (const char *string, int c)
                                  _GL_ATTRIBUTE_PURE
                                  _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_SYS (mbschr, char *, (const char *string, int c));
# endif
_GL_CXXALIASWARN (mbschr);
#endif

#if @GNULIB_MBSRCHR@
 
# if defined __hpux || defined __INTERIX
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define mbsrchr rpl_mbsrchr  
#  endif
_GL_FUNCDECL_RPL (mbsrchr, char *, (const char *string, int c)
                                   _GL_ATTRIBUTE_PURE
                                   _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (mbsrchr, char *, (const char *string, int c));
# else
_GL_FUNCDECL_SYS (mbsrchr, char *, (const char *string, int c)
                                   _GL_ATTRIBUTE_PURE
                                   _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_SYS (mbsrchr, char *, (const char *string, int c));
# endif
_GL_CXXALIASWARN (mbsrchr);
#endif

#if @GNULIB_MBSSTR@
 
_GL_EXTERN_C char * mbsstr (const char *haystack, const char *needle)
     _GL_ATTRIBUTE_PURE
     _GL_ARG_NONNULL ((1, 2));
#endif

#if @GNULIB_MBSCASECMP@
 
_GL_EXTERN_C int mbscasecmp (const char *s1, const char *s2)
     _GL_ATTRIBUTE_PURE
     _GL_ARG_NONNULL ((1, 2));
#endif

#if @GNULIB_MBSNCASECMP@
 
_GL_EXTERN_C int mbsncasecmp (const char *s1, const char *s2, size_t n)
     _GL_ATTRIBUTE_PURE
     _GL_ARG_NONNULL ((1, 2));
#endif

#if @GNULIB_MBSPCASECMP@
 
_GL_EXTERN_C char * mbspcasecmp (const char *string, const char *prefix)
     _GL_ATTRIBUTE_PURE
     _GL_ARG_NONNULL ((1, 2));
#endif

#if @GNULIB_MBSCASESTR@
 
_GL_EXTERN_C char * mbscasestr (const char *haystack, const char *needle)
     _GL_ATTRIBUTE_PURE
     _GL_ARG_NONNULL ((1, 2));
#endif

#if @GNULIB_MBSCSPN@
 
_GL_EXTERN_C size_t mbscspn (const char *string, const char *accept)
     _GL_ATTRIBUTE_PURE
     _GL_ARG_NONNULL ((1, 2));
#endif

#if @GNULIB_MBSPBRK@
 
# if defined __hpux
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define mbspbrk rpl_mbspbrk  
#  endif
_GL_FUNCDECL_RPL (mbspbrk, char *, (const char *string, const char *accept)
                                   _GL_ATTRIBUTE_PURE
                                   _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (mbspbrk, char *, (const char *string, const char *accept));
# else
_GL_FUNCDECL_SYS (mbspbrk, char *, (const char *string, const char *accept)
                                   _GL_ATTRIBUTE_PURE
                                   _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_SYS (mbspbrk, char *, (const char *string, const char *accept));
# endif
_GL_CXXALIASWARN (mbspbrk);
#endif

#if @GNULIB_MBSSPN@
 
_GL_EXTERN_C size_t mbsspn (const char *string, const char *reject)
     _GL_ATTRIBUTE_PURE
     _GL_ARG_NONNULL ((1, 2));
#endif

#if @GNULIB_MBSSEP@
 
_GL_EXTERN_C char * mbssep (char **stringp, const char *delim)
     _GL_ARG_NONNULL ((1, 2));
#endif

#if @GNULIB_MBSTOK_R@
 
_GL_EXTERN_C char * mbstok_r (char *restrict string, const char *delim,
                              char **save_ptr)
     _GL_ARG_NONNULL ((2, 3));
#endif

 
#if @GNULIB_STRERROR@
# if @REPLACE_STRERROR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef strerror
#   define strerror rpl_strerror
#  endif
_GL_FUNCDECL_RPL (strerror, char *, (int));
_GL_CXXALIAS_RPL (strerror, char *, (int));
# else
_GL_CXXALIAS_SYS (strerror, char *, (int));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (strerror);
# endif
#elif defined GNULIB_POSIXCHECK
# undef strerror
 
_GL_WARN_ON_USE (strerror, "strerror is unportable - "
                 "use gnulib module strerror to guarantee non-NULL result");
#endif

 
#if @GNULIB_STRERROR_R@
# if @REPLACE_STRERROR_R@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef strerror_r
#   define strerror_r rpl_strerror_r
#  endif
_GL_FUNCDECL_RPL (strerror_r, int, (int errnum, char *buf, size_t buflen)
                                   _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (strerror_r, int, (int errnum, char *buf, size_t buflen));
# else
#  if !@HAVE_DECL_STRERROR_R@
_GL_FUNCDECL_SYS (strerror_r, int, (int errnum, char *buf, size_t buflen)
                                   _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (strerror_r, int, (int errnum, char *buf, size_t buflen));
# endif
# if __GLIBC__ >= 2 && @HAVE_DECL_STRERROR_R@
_GL_CXXALIASWARN (strerror_r);
# endif
#elif defined GNULIB_POSIXCHECK
# undef strerror_r
# if HAVE_RAW_DECL_STRERROR_R
_GL_WARN_ON_USE (strerror_r, "strerror_r is unportable - "
                 "use gnulib module strerror_r-posix for portability");
# endif
#endif

 
#if @GNULIB_STRERRORNAME_NP@
# if @REPLACE_STRERRORNAME_NP@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef strerrorname_np
#   define strerrorname_np rpl_strerrorname_np
#  endif
_GL_FUNCDECL_RPL (strerrorname_np, const char *, (int errnum));
_GL_CXXALIAS_RPL (strerrorname_np, const char *, (int errnum));
# else
#  if !@HAVE_STRERRORNAME_NP@
_GL_FUNCDECL_SYS (strerrorname_np, const char *, (int errnum));
#  endif
_GL_CXXALIAS_SYS (strerrorname_np, const char *, (int errnum));
# endif
_GL_CXXALIASWARN (strerrorname_np);
#elif defined GNULIB_POSIXCHECK
# undef strerrorname_np
# if HAVE_RAW_DECL_STRERRORNAME_NP
_GL_WARN_ON_USE (strerrorname_np, "strerrorname_np is unportable - "
                 "use gnulib module strerrorname_np for portability");
# endif
#endif

 
#if @GNULIB_SIGABBREV_NP@
# if ! @HAVE_SIGABBREV_NP@
_GL_FUNCDECL_SYS (sigabbrev_np, const char *, (int sig));
# endif
_GL_CXXALIAS_SYS (sigabbrev_np, const char *, (int sig));
_GL_CXXALIASWARN (sigabbrev_np);
#elif defined GNULIB_POSIXCHECK
# undef sigabbrev_np
# if HAVE_RAW_DECL_SIGABBREV_NP
_GL_WARN_ON_USE (sigabbrev_np, "sigabbrev_np is unportable - "
                 "use gnulib module sigabbrev_np for portability");
# endif
#endif

 
#if @GNULIB_SIGDESCR_NP@
# if ! @HAVE_SIGDESCR_NP@
_GL_FUNCDECL_SYS (sigdescr_np, const char *, (int sig));
# endif
_GL_CXXALIAS_SYS (sigdescr_np, const char *, (int sig));
_GL_CXXALIASWARN (sigdescr_np);
#elif defined GNULIB_POSIXCHECK
# undef sigdescr_np
# if HAVE_RAW_DECL_SIGDESCR_NP
_GL_WARN_ON_USE (sigdescr_np, "sigdescr_np is unportable - "
                 "use gnulib module sigdescr_np for portability");
# endif
#endif

#if @GNULIB_STRSIGNAL@
# if @REPLACE_STRSIGNAL@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define strsignal rpl_strsignal
#  endif
_GL_FUNCDECL_RPL (strsignal, char *, (int __sig));
_GL_CXXALIAS_RPL (strsignal, char *, (int __sig));
# else
#  if ! @HAVE_DECL_STRSIGNAL@
_GL_FUNCDECL_SYS (strsignal, char *, (int __sig));
#  endif
 
_GL_CXXALIAS_SYS_CAST (strsignal, char *, (int __sig));
# endif
_GL_CXXALIASWARN (strsignal);
#elif defined GNULIB_POSIXCHECK
# undef strsignal
# if HAVE_RAW_DECL_STRSIGNAL
_GL_WARN_ON_USE (strsignal, "strsignal is unportable - "
                 "use gnulib module strsignal for portability");
# endif
#endif

#if @GNULIB_STRVERSCMP@
# if !@HAVE_STRVERSCMP@
_GL_FUNCDECL_SYS (strverscmp, int, (const char *, const char *)
                                   _GL_ATTRIBUTE_PURE
                                   _GL_ARG_NONNULL ((1, 2)));
# endif
_GL_CXXALIAS_SYS (strverscmp, int, (const char *, const char *));
_GL_CXXALIASWARN (strverscmp);
#elif defined GNULIB_POSIXCHECK
# undef strverscmp
# if HAVE_RAW_DECL_STRVERSCMP
_GL_WARN_ON_USE (strverscmp, "strverscmp is unportable - "
                 "use gnulib module strverscmp for portability");
# endif
#endif


#endif  
#endif  
#endif
