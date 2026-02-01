 

 

#ifndef _@GUARD_PREFIX@_UCHAR_H

#if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
#endif
@PRAGMA_COLUMNS@

 
#if @HAVE_UCHAR_H@
# if defined __HAIKU__
 
# if defined __cplusplus && defined _AIX && defined __ibmxl__ && defined __clang__
#  define char16_t gl_char16_t
#  define char32_t gl_char32_t
# endif
# @INCLUDE_NEXT@ @NEXT_UCHAR_H@
#endif

#ifndef _@GUARD_PREFIX@_UCHAR_H
#define _@GUARD_PREFIX@_UCHAR_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <stdint.h>

 
#include <wchar.h>

 
#include <string.h>
#include <wctype.h>

 
#ifndef _GL_ATTRIBUTE_PURE
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96) || defined __clang__
#  define _GL_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define _GL_ATTRIBUTE_PURE  
# endif
#endif

 

 

 


_GL_INLINE_HEADER_BEGIN


#if !(@HAVE_UCHAR_H@ || (defined __cplusplus && @CXX_HAS_CHAR8_TYPE@))

 
typedef unsigned char char8_t;

#elif @GNULIBHEADERS_OVERRIDE_CHAR8_T@

typedef unsigned char gl_char8_t;
# define char8_t gl_char8_t

#endif

#if !(@HAVE_UCHAR_H@ || (defined __cplusplus && @CXX_HAS_UCHAR_TYPES@))

 
typedef uint_least16_t char16_t;

#elif @GNULIBHEADERS_OVERRIDE_CHAR16_T@

typedef uint_least16_t gl_char16_t;
# define char16_t gl_char16_t

#endif

#if !(@HAVE_UCHAR_H@ || (defined __cplusplus && @CXX_HAS_UCHAR_TYPES@))

 
typedef uint_least32_t char32_t;

#elif @GNULIBHEADERS_OVERRIDE_CHAR32_T@

typedef uint_least32_t gl_char32_t;
# define char32_t gl_char32_t

#endif

 
#if @SMALL_WCHAR_T@                     
# define _GL_SMALL_WCHAR_T 1
#endif

 
#if defined __STDC_ISO_10646__ && !_GL_SMALL_WCHAR_T
 
# define _GL_WCHAR_T_IS_UCS4 1
#endif
#if _GL_WCHAR_T_IS_UCS4
static_assert (sizeof (char32_t) == sizeof (wchar_t));
#endif


 
#if @GNULIB_BTOC32@
# if _GL_WCHAR_T_IS_UCS4 && !defined IN_BTOC32
_GL_BEGIN_C_LINKAGE
_GL_INLINE _GL_ATTRIBUTE_PURE wint_t
btoc32 (int c)
{
  return btowc (c);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (btoc32, wint_t, (int c) _GL_ATTRIBUTE_PURE);
# endif
_GL_CXXALIAS_SYS (btoc32, wint_t, (int c));
_GL_CXXALIASWARN (btoc32);
#endif


 
#if @GNULIB_C32ISALNUM@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISALNUM
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32isalnum (wint_t wc)
{
  return iswalnum (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32isalnum, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32isalnum, int, (wint_t wc));
_GL_CXXALIASWARN (c32isalnum);
#endif
#if @GNULIB_C32ISALPHA@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISALPHA
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32isalpha (wint_t wc)
{
  return iswalpha (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32isalpha, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32isalpha, int, (wint_t wc));
_GL_CXXALIASWARN (c32isalpha);
#endif
#if @GNULIB_C32ISBLANK@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISBLANK
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32isblank (wint_t wc)
{
  return iswblank (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32isblank, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32isblank, int, (wint_t wc));
_GL_CXXALIASWARN (c32isblank);
#endif
#if @GNULIB_C32ISCNTRL@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISCNTRL
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32iscntrl (wint_t wc)
{
  return iswcntrl (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32iscntrl, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32iscntrl, int, (wint_t wc));
_GL_CXXALIASWARN (c32iscntrl);
#endif
#if @GNULIB_C32ISDIGIT@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISDIGIT
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32isdigit (wint_t wc)
{
  return iswdigit (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32isdigit, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32isdigit, int, (wint_t wc));
_GL_CXXALIASWARN (c32isdigit);
#endif
#if @GNULIB_C32ISGRAPH@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISGRAPH
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32isgraph (wint_t wc)
{
  return iswgraph (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32isgraph, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32isgraph, int, (wint_t wc));
_GL_CXXALIASWARN (c32isgraph);
#endif
#if @GNULIB_C32ISLOWER@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISLOWER
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32islower (wint_t wc)
{
  return iswlower (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32islower, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32islower, int, (wint_t wc));
_GL_CXXALIASWARN (c32islower);
#endif
#if @GNULIB_C32ISPRINT@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISPRINT
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32isprint (wint_t wc)
{
  return iswprint (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32isprint, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32isprint, int, (wint_t wc));
_GL_CXXALIASWARN (c32isprint);
#endif
#if @GNULIB_C32ISPUNCT@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISPUNCT
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32ispunct (wint_t wc)
{
  return iswpunct (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32ispunct, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32ispunct, int, (wint_t wc));
_GL_CXXALIASWARN (c32ispunct);
#endif
#if @GNULIB_C32ISSPACE@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISSPACE
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32isspace (wint_t wc)
{
  return iswspace (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32isspace, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32isspace, int, (wint_t wc));
_GL_CXXALIASWARN (c32isspace);
#endif
#if @GNULIB_C32ISUPPER@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISUPPER
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32isupper (wint_t wc)
{
  return iswupper (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32isupper, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32isupper, int, (wint_t wc));
_GL_CXXALIASWARN (c32isupper);
#endif
#if @GNULIB_C32ISXDIGIT@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32ISXDIGIT
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32isxdigit (wint_t wc)
{
  return iswxdigit (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32isxdigit, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32isxdigit, int, (wint_t wc));
_GL_CXXALIASWARN (c32isxdigit);
#endif


 
#if @GNULIB_C32TOLOWER@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32TOLOWER
_GL_BEGIN_C_LINKAGE
_GL_INLINE wint_t
c32tolower (wint_t wc)
{
  return towlower (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32tolower, wint_t, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32tolower, wint_t, (wint_t wc));
_GL_CXXALIASWARN (c32tolower);
#endif
#if @GNULIB_C32TOUPPER@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32TOUPPER
_GL_BEGIN_C_LINKAGE
_GL_INLINE wint_t
c32toupper (wint_t wc)
{
  return towupper (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32toupper, wint_t, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32toupper, wint_t, (wint_t wc));
_GL_CXXALIASWARN (c32toupper);
#endif


 
#if @GNULIB_C32WIDTH@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32WIDTH
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32width (char32_t wc)
{
  return wcwidth (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32width, int, (char32_t wc));
# endif
_GL_CXXALIAS_SYS (c32width, int, (char32_t wc));
_GL_CXXALIASWARN (c32width);
#endif


 
#if @GNULIB_C32RTOMB@
# if @REPLACE_C32RTOMB@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef c32rtomb
#   define c32rtomb rpl_c32rtomb
#  endif
_GL_FUNCDECL_RPL (c32rtomb, size_t, (char *s, char32_t wc, mbstate_t *ps));
_GL_CXXALIAS_RPL (c32rtomb, size_t, (char *s, char32_t wc, mbstate_t *ps));
# else
#  if !@HAVE_C32RTOMB@
_GL_FUNCDECL_SYS (c32rtomb, size_t, (char *s, char32_t wc, mbstate_t *ps));
#  endif
_GL_CXXALIAS_SYS (c32rtomb, size_t, (char *s, char32_t wc, mbstate_t *ps));
# endif
# if __GLIBC__ + (__GLIBC_MINOR__ >= 16) > 2
_GL_CXXALIASWARN (c32rtomb);
# endif
#elif defined GNULIB_POSIXCHECK
# undef c32rtomb
# if HAVE_RAW_DECL_C32RTOMB
_GL_WARN_ON_USE (c32rtomb, "c32rtomb is not portable - "
                 "use gnulib module c32rtomb for portability");
# endif
#endif


 
#if @GNULIB_C32SNRTOMBS@
# if _GL_WCHAR_T_IS_UCS4 && !defined IN_C32SNRTOMBS
_GL_BEGIN_C_LINKAGE
_GL_INLINE _GL_ARG_NONNULL ((2)) size_t
c32snrtombs (char *dest, const char32_t **srcp, size_t srclen, size_t len,
             mbstate_t *ps)
{
  return wcsnrtombs (dest, (const wchar_t **) srcp, srclen, len, ps);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32snrtombs, size_t,
                  (char *dest, const char32_t **srcp, size_t srclen, size_t len,
                   mbstate_t *ps)
                  _GL_ARG_NONNULL ((2)));
# endif
_GL_CXXALIAS_SYS (c32snrtombs, size_t,
                  (char *dest, const char32_t **srcp, size_t srclen, size_t len,
                   mbstate_t *ps));
_GL_CXXALIASWARN (c32snrtombs);
#endif


 
#if @GNULIB_C32SRTOMBS@
# if _GL_WCHAR_T_IS_UCS4 && !defined IN_C32SRTOMBS
_GL_BEGIN_C_LINKAGE
_GL_INLINE _GL_ARG_NONNULL ((2)) size_t
c32srtombs (char *dest, const char32_t **srcp, size_t len, mbstate_t *ps)
{
  return wcsrtombs (dest, (const wchar_t **) srcp, len, ps);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32srtombs, size_t,
                  (char *dest, const char32_t **srcp, size_t len, mbstate_t *ps)
                  _GL_ARG_NONNULL ((2)));
# endif
_GL_CXXALIAS_SYS (c32srtombs, size_t,
                  (char *dest, const char32_t **srcp, size_t len,
                   mbstate_t *ps));
_GL_CXXALIASWARN (c32srtombs);
#endif


 
#if @GNULIB_C32STOMBS@
# if _GL_WCHAR_T_IS_UCS4 && !defined IN_C32STOMBS
_GL_BEGIN_C_LINKAGE
_GL_INLINE _GL_ARG_NONNULL ((2)) size_t
c32stombs (char *dest, const char32_t *src, size_t len)
{
  mbstate_t state;

  mbszero (&state);
  return c32srtombs (dest, &src, len, &state);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32stombs, size_t,
                  (char *dest, const char32_t *src, size_t len)
                  _GL_ARG_NONNULL ((2)));
# endif
_GL_CXXALIAS_SYS (c32stombs, size_t,
                  (char *dest, const char32_t *src, size_t len));
_GL_CXXALIASWARN (c32stombs);
#endif


 
#if @GNULIB_C32SWIDTH@
# if (_GL_WCHAR_T_IS_UCS4 && !GNULIB_defined_mbstate_t) && !defined IN_C32SWIDTH
_GL_BEGIN_C_LINKAGE
_GL_INLINE _GL_ARG_NONNULL ((1)) int
c32swidth (const char32_t *s, size_t n)
{
  return wcswidth ((const wchar_t *) s, n);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32swidth, int, (const char32_t *s, size_t n)
                                  _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (c32swidth, int, (const char32_t *s, size_t n));
_GL_CXXALIASWARN (c32swidth);
#endif


 
#if @GNULIB_C32TOB@
# if _GL_WCHAR_T_IS_UCS4 && !defined IN_C32TOB
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32tob (wint_t wc)
{
  return wctob (wc);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32tob, int, (wint_t wc));
# endif
_GL_CXXALIAS_SYS (c32tob, int, (wint_t wc));
_GL_CXXALIASWARN (c32tob);
#endif


 
#if @GNULIB_MBRTOC32@
# if @REPLACE_MBRTOC32@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mbrtoc32
#   define mbrtoc32 rpl_mbrtoc32
#  endif
_GL_FUNCDECL_RPL (mbrtoc32, size_t,
                  (char32_t *pc, const char *s, size_t n, mbstate_t *ps));
_GL_CXXALIAS_RPL (mbrtoc32, size_t,
                  (char32_t *pc, const char *s, size_t n, mbstate_t *ps));
# else
#  if !@HAVE_MBRTOC32@
_GL_FUNCDECL_SYS (mbrtoc32, size_t,
                  (char32_t *pc, const char *s, size_t n, mbstate_t *ps));
#  endif
_GL_CXXALIAS_SYS (mbrtoc32, size_t,
                  (char32_t *pc, const char *s, size_t n, mbstate_t *ps));
# endif
# if __GLIBC__ + (__GLIBC_MINOR__ >= 16) > 2
_GL_CXXALIASWARN (mbrtoc32);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mbrtoc32
# if HAVE_RAW_DECL_MBRTOC32
_GL_WARN_ON_USE (mbrtoc32, "mbrtoc32 is not portable - "
                 "use gnulib module mbrtoc32 for portability");
# endif
#endif


 
#if @GNULIB_MBRTOC16@
# if @REPLACE_MBRTOC16@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mbrtoc16
#   define mbrtoc16 rpl_mbrtoc16
#  endif
_GL_FUNCDECL_RPL (mbrtoc16, size_t,
                  (char16_t *pc, const char *s, size_t n, mbstate_t *ps));
_GL_CXXALIAS_RPL (mbrtoc16, size_t,
                  (char16_t *pc, const char *s, size_t n, mbstate_t *ps));
# else
#  if !@HAVE_MBRTOC32@
_GL_FUNCDECL_SYS (mbrtoc16, size_t,
                  (char16_t *pc, const char *s, size_t n, mbstate_t *ps));
#  endif
_GL_CXXALIAS_SYS (mbrtoc16, size_t,
                  (char16_t *pc, const char *s, size_t n, mbstate_t *ps));
# endif
# if __GLIBC__ + (__GLIBC_MINOR__ >= 16) > 2
_GL_CXXALIASWARN (mbrtoc16);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mbrtoc16
# if HAVE_RAW_DECL_MBRTOC16
_GL_WARN_ON_USE (mbrtoc16, "mbrtoc16 is not portable - "
                 "use gnulib module mbrtoc16 for portability");
# endif
#endif


 
#if @GNULIB_MBSNRTOC32S@
# if _GL_WCHAR_T_IS_UCS4 && !defined IN_MBSNRTOC32S
_GL_BEGIN_C_LINKAGE
_GL_INLINE _GL_ARG_NONNULL ((2)) size_t
mbsnrtoc32s (char32_t *dest, const char **srcp, size_t srclen, size_t len,
             mbstate_t *ps)
{
  return mbsnrtowcs ((wchar_t *) dest, srcp, srclen, len, ps);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (mbsnrtoc32s, size_t,
                  (char32_t *dest, const char **srcp, size_t srclen, size_t len,
                   mbstate_t *ps)
                  _GL_ARG_NONNULL ((2)));
# endif
_GL_CXXALIAS_SYS (mbsnrtoc32s, size_t,
                  (char32_t *dest, const char **srcp, size_t srclen, size_t len,
                   mbstate_t *ps));
_GL_CXXALIASWARN (mbsnrtoc32s);
#endif


 
#if @GNULIB_MBSRTOC32S@
# if _GL_WCHAR_T_IS_UCS4 && !defined IN_MBSRTOC32S
_GL_BEGIN_C_LINKAGE
_GL_INLINE _GL_ARG_NONNULL ((2)) size_t
mbsrtoc32s (char32_t *dest, const char **srcp, size_t len, mbstate_t *ps)
{
  return mbsrtowcs ((wchar_t *) dest, srcp, len, ps);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (mbsrtoc32s, size_t,
                  (char32_t *dest, const char **srcp, size_t len, mbstate_t *ps)
                  _GL_ARG_NONNULL ((2)));
# endif
_GL_CXXALIAS_SYS (mbsrtoc32s, size_t,
                  (char32_t *dest, const char **srcp, size_t len,
                   mbstate_t *ps));
_GL_CXXALIASWARN (mbsrtoc32s);
#endif


 
#if @GNULIB_MBSTOC32S@
# if _GL_WCHAR_T_IS_UCS4 && !defined IN_MBSTOC32S
_GL_BEGIN_C_LINKAGE
_GL_INLINE _GL_ARG_NONNULL ((2)) size_t
mbstoc32s (char32_t *dest, const char *src, size_t len)
{
  mbstate_t state;

  mbszero (&state);
  return mbsrtoc32s (dest, &src, len, &state);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (mbstoc32s, size_t,
                  (char32_t *dest, const char *src, size_t len)
                  _GL_ARG_NONNULL ((2)));
# endif
_GL_CXXALIAS_SYS (mbstoc32s, size_t,
                  (char32_t *dest, const char *src, size_t len));
_GL_CXXALIASWARN (mbstoc32s);
#endif


#if @GNULIB_C32_GET_TYPE_TEST@ || @GNULIB_C32_APPLY_TYPE_TEST@
 
# if _GL_WCHAR_T_IS_UCS4
typedef wctype_t c32_type_test_t;
# else
typedef  int (*c32_type_test_t) (wint_t wc);
# endif
#endif

 
#if @GNULIB_C32_GET_TYPE_TEST@
# if _GL_WCHAR_T_IS_UCS4 && !defined IN_C32_GET_TYPE_TEST
_GL_BEGIN_C_LINKAGE
_GL_INLINE _GL_ARG_NONNULL ((1)) c32_type_test_t
c32_get_type_test (const char *name)
{
  return wctype (name);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32_get_type_test, c32_type_test_t, (const char *name)
                                                      _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (c32_get_type_test, c32_type_test_t, (const char *name));
_GL_CXXALIASWARN (c32_get_type_test);
#endif

 
#if @GNULIB_C32_APPLY_TYPE_TEST@
# if _GL_WCHAR_T_IS_UCS4
#  if !defined IN_C32_APPLY_TYPE_TEST
_GL_BEGIN_C_LINKAGE
_GL_INLINE int
c32_apply_type_test (wint_t wc, c32_type_test_t property)
{
  return iswctype (wc, property);
}
_GL_END_C_LINKAGE
#  else
_GL_FUNCDECL_SYS (c32_apply_type_test, int,
                  (wint_t wc, c32_type_test_t property));
#  endif
# else
_GL_FUNCDECL_SYS (c32_apply_type_test, int,
                  (wint_t wc, c32_type_test_t property)
                  _GL_ARG_NONNULL ((2)));
# endif
_GL_CXXALIAS_SYS (c32_apply_type_test, int,
                  (wint_t wc, c32_type_test_t property));
_GL_CXXALIASWARN (c32_apply_type_test);
#endif


#if @GNULIB_C32_GET_MAPPING@ || @GNULIB_C32_APPLY_MAPPING@
 
# if _GL_WCHAR_T_IS_UCS4
typedef wctrans_t c32_mapping_t;
# else
typedef wint_t (*c32_mapping_t) (wint_t wc);
# endif
#endif

 
#if @GNULIB_C32_GET_MAPPING@
# if _GL_WCHAR_T_IS_UCS4 && !defined IN_C32_GET_MAPPING
_GL_BEGIN_C_LINKAGE
_GL_INLINE _GL_ARG_NONNULL ((1)) c32_mapping_t
c32_get_mapping (const char *name)
{
  return wctrans (name);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32_get_mapping, c32_mapping_t, (const char *name)
                                                  _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (c32_get_mapping, c32_mapping_t, (const char *name));
_GL_CXXALIASWARN (c32_get_mapping);
#endif

 
#if @GNULIB_C32_APPLY_MAPPING@
# if _GL_WCHAR_T_IS_UCS4 && !defined IN_C32_APPLY_MAPPING
_GL_BEGIN_C_LINKAGE
_GL_INLINE _GL_ARG_NONNULL ((2)) wint_t
c32_apply_mapping (wint_t wc, c32_mapping_t mapping)
{
  return towctrans (wc, mapping);
}
_GL_END_C_LINKAGE
# else
_GL_FUNCDECL_SYS (c32_apply_mapping, wint_t,
                  (wint_t wc, c32_mapping_t mapping)
                  _GL_ARG_NONNULL ((2)));
# endif
_GL_CXXALIAS_SYS (c32_apply_mapping, wint_t,
                  (wint_t wc, c32_mapping_t mapping));
_GL_CXXALIASWARN (c32_apply_mapping);
#endif


_GL_INLINE_HEADER_END

#endif  
#endif  
