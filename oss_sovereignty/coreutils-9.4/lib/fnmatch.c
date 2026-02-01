 
#ifndef _GNU_SOURCE
# define _GNU_SOURCE    1
#endif

#include <fnmatch.h>

#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdckdint.h>
#include <stdlib.h>
#if defined _LIBC || HAVE_ALLOCA
# include <alloca.h>
#endif
#include <stddef.h>
#include <uchar.h>
#if defined _LIBC || !_GL_SMALL_WCHAR_T
 
# include <wchar.h>
# include <wctype.h>
# define WCHAR_T wchar_t
# define WINT_T wint_t
# define BTOWC btowc
# define MBSRTOWCS mbsrtowcs
# define WCSLEN wcslen
# define WCSCAT wcscat
# define WMEMPCPY wmempcpy
# define WMEMCHR wmemchr
# define TOWLOWER towlower
# define WCTYPE_T wctype_t
# define WCTYPE wctype
# define ISWCTYPE iswctype
#else
 
# include "unistr.h"
# define WCHAR_T char32_t
# define WINT_T char32_t
# define BTOWC btoc32
# define MBSRTOWCS mbsrtoc32s
# define WCSLEN u32_strlen
# define WCSCAT u32_strcat
# define WMEMPCPY u32_pcpy
# define WMEMCHR(S, C, N) u32_chr (S, N, C)
# define TOWLOWER c32tolower
# define WCTYPE_T c32_type_test_t
# define WCTYPE c32_get_type_test
# define ISWCTYPE c32_apply_type_test
#endif

 
#ifdef _LIBC
# include "../locale/localeinfo.h"
# include "../locale/coll-lookup.h"
# include <shlib-compat.h>

# define CONCAT(a,b) __CONCAT(a,b)
# define btowc __btowc
# define iswctype __iswctype
# define mbsrtowcs __mbsrtowcs
# define mempcpy __mempcpy
# define strnlen __strnlen
# define towlower __towlower
# define wcscat __wcscat
# define wcslen __wcslen
# define wctype __wctype
# define wmemchr __wmemchr
# define wmempcpy __wmempcpy
# define fnmatch __fnmatch
extern int fnmatch (const char *pattern, const char *string, int flags);
#endif

#ifdef _LIBC
# if __glibc_has_attribute (__fallthrough__)
#  define FALLTHROUGH __attribute__ ((__fallthrough__))
# else
#  define FALLTHROUGH ((void) 0)
# endif
#else
# include "attribute.h"
#endif

#include <flexmember.h>

#ifdef _LIBC
typedef ptrdiff_t idx_t;
#else
# include "idx.h"
#endif

 
#define NO_LEADING_PERIOD(flags) \
  ((flags & (FNM_FILE_NAME | FNM_PERIOD)) == (FNM_FILE_NAME | FNM_PERIOD))

#ifndef _LIBC
# if HAVE_ALLOCA
 
#  define __libc_use_alloca(n) ((n) < 4032)
# else
 
#  define __libc_use_alloca(n) false
#  undef alloca
#  define alloca(n) malloc (n)
# endif
# define alloca_account(size, avar) ((avar) += (size), alloca (size))
#endif

 
#ifdef CHARCLASS_NAME_MAX
# define CHAR_CLASS_MAX_LENGTH CHARCLASS_NAME_MAX
#else
 
# define CHAR_CLASS_MAX_LENGTH 256
#endif

#define IS_CHAR_CLASS(string) WCTYPE (string)

 

 
static int posixly_correct;

 
#define FOLD(c) ((flags & FNM_CASEFOLD) ? tolower (c) : (c))
#define CHAR    char
#define UCHAR   unsigned char
#define INT     int
#define FCT     internal_fnmatch
#define EXT     ext_match
#define END     end_pattern
#define STRUCT  fnmatch_struct
#define L_(CS)  CS
#define UCHAR_TO_WCHAR(C) BTOWC (C)
#define STRLEN(S) strlen (S)
#define STRCAT(D, S) strcat (D, S)
#define MEMPCPY(D, S, N) mempcpy (D, S, N)
#define MEMCHR(S, C, N) memchr (S, C, N)
#define WIDE_CHAR_VERSION 0
#ifdef _LIBC
# include <locale/weight.h>
# define FINDIDX findidx
#endif
#include "fnmatch_loop.c"


#define FOLD(c) ((flags & FNM_CASEFOLD) ? TOWLOWER (c) : (c))
#define CHAR    WCHAR_T
#define UCHAR   WINT_T
#define INT     WINT_T
#define FCT     internal_fnwmatch
#define EXT     ext_wmatch
#define END     end_wpattern
#define L_(CS)  L##CS
#define UCHAR_TO_WCHAR(C) (C)
#define STRLEN(S) WCSLEN (S)
#define STRCAT(D, S) WCSCAT (D, S)
#define MEMPCPY(D, S, N) WMEMPCPY (D, S, N)
#define MEMCHR(S, C, N) WMEMCHR (S, C, N)
#define WIDE_CHAR_VERSION 1
#ifdef _LIBC
 
# define findidx findidxwc
# include <locale/weightwc.h>
# undef findidx
# define FINDIDX findidxwc
#endif

#undef IS_CHAR_CLASS
 
static WCTYPE_T
is_char_class (const WCHAR_T *wcs)
{
  char s[CHAR_CLASS_MAX_LENGTH + 1];
  char *cp = s;

  do
    {
       
#ifdef _LIBC
      if (*wcs < 0x20 || *wcs > 0x7e
          || *wcs == 0x24 || *wcs == 0x40 || *wcs == 0x60)
        return (WCTYPE_T) 0;
#else
      switch (*wcs)
        {
        case L' ': case L'!': case L'"': case L'#': case L'%':
        case L'&': case L'\'': case L'(': case L')': case L'*':
        case L'+': case L',': case L'-': case L'.': case L'/':
        case L'0': case L'1': case L'2': case L'3': case L'4':
        case L'5': case L'6': case L'7': case L'8': case L'9':
        case L':': case L';': case L'<': case L'=': case L'>':
        case L'?':
        case L'A': case L'B': case L'C': case L'D': case L'E':
        case L'F': case L'G': case L'H': case L'I': case L'J':
        case L'K': case L'L': case L'M': case L'N': case L'O':
        case L'P': case L'Q': case L'R': case L'S': case L'T':
        case L'U': case L'V': case L'W': case L'X': case L'Y':
        case L'Z':
        case L'[': case L'\\': case L']': case L'^': case L'_':
        case L'a': case L'b': case L'c': case L'd': case L'e':
        case L'f': case L'g': case L'h': case L'i': case L'j':
        case L'k': case L'l': case L'm': case L'n': case L'o':
        case L'p': case L'q': case L'r': case L's': case L't':
        case L'u': case L'v': case L'w': case L'x': case L'y':
        case L'z': case L'{': case L'|': case L'}': case L'~':
          break;
        default:
          return (WCTYPE_T) 0;
        }
#endif

       
      if (cp == s + CHAR_CLASS_MAX_LENGTH)
        return (WCTYPE_T) 0;

      *cp++ = (char) *wcs++;
    }
  while (*wcs != L'\0');

  *cp = '\0';

  return WCTYPE (s);
}
#define IS_CHAR_CLASS(string) is_char_class (string)

#include "fnmatch_loop.c"


int
fnmatch (const char *pattern, const char *string, int flags)
{
  if (__glibc_unlikely (MB_CUR_MAX != 1))
    {
      mbstate_t ps;
      size_t n;
      const char *p;
      WCHAR_T *wpattern_malloc = NULL;
      WCHAR_T *wpattern;
      WCHAR_T *wstring_malloc = NULL;
      WCHAR_T *wstring;
      size_t alloca_used = 0;

       
      memset (&ps, '\0', sizeof (ps));
      p = pattern;
      n = strnlen (pattern, 1024);
      if (__glibc_likely (n < 1024))
        {
          wpattern = (WCHAR_T *) alloca_account ((n + 1) * sizeof (WCHAR_T),
                                                 alloca_used);
          n = MBSRTOWCS (wpattern, &p, n + 1, &ps);
          if (__glibc_unlikely (n == (size_t) -1))
             
            return -1;
          if (p)
            {
              memset (&ps, '\0', sizeof (ps));
              goto prepare_wpattern;
            }
        }
      else
        {
        prepare_wpattern:
          n = MBSRTOWCS (NULL, &pattern, 0, &ps);
          if (__glibc_unlikely (n == (size_t) -1))
             
            return -1;
          if (__glibc_unlikely (n >= (size_t) -1 / sizeof (WCHAR_T)))
            {
              __set_errno (ENOMEM);
              return -2;
            }
          wpattern_malloc = wpattern
            = (WCHAR_T *) malloc ((n + 1) * sizeof (WCHAR_T));
          assert (mbsinit (&ps));
          if (wpattern == NULL)
            return -2;
          (void) MBSRTOWCS (wpattern, &pattern, n + 1, &ps);
        }

      assert (mbsinit (&ps));
      n = strnlen (string, 1024);
      p = string;
      if (__glibc_likely (n < 1024))
        {
          wstring = (WCHAR_T *) alloca_account ((n + 1) * sizeof (WCHAR_T),
                                                alloca_used);
          n = MBSRTOWCS (wstring, &p, n + 1, &ps);
          if (__glibc_unlikely (n == (size_t) -1))
            {
               
            free_return:
              free (wpattern_malloc);
              return -1;
            }
          if (p)
            {
              memset (&ps, '\0', sizeof (ps));
              goto prepare_wstring;
            }
        }
      else
        {
        prepare_wstring:
          n = MBSRTOWCS (NULL, &string, 0, &ps);
          if (__glibc_unlikely (n == (size_t) -1))
             
            goto free_return;
          if (__glibc_unlikely (n >= (size_t) -1 / sizeof (WCHAR_T)))
            {
              free (wpattern_malloc);
              __set_errno (ENOMEM);
              return -2;
            }

          wstring_malloc = wstring
            = (WCHAR_T *) malloc ((n + 1) * sizeof (WCHAR_T));
          if (wstring == NULL)
            {
              free (wpattern_malloc);
              return -2;
            }
          assert (mbsinit (&ps));
          (void) MBSRTOWCS (wstring, &string, n + 1, &ps);
        }

      int res = internal_fnwmatch (wpattern, wstring, wstring + n,
                                   flags & FNM_PERIOD, flags, NULL,
                                   alloca_used);

      free (wstring_malloc);
      free (wpattern_malloc);

      return res;
    }

  return internal_fnmatch (pattern, string, string + strlen (string),
                           flags & FNM_PERIOD, flags, NULL, 0);
}

#undef fnmatch
versioned_symbol (libc, __fnmatch, fnmatch, GLIBC_2_2_3);
#if SHLIB_COMPAT(libc, GLIBC_2_0, GLIBC_2_2_3)
strong_alias (__fnmatch, __fnmatch_old)
compat_symbol (libc, __fnmatch_old, fnmatch, GLIBC_2_0);
#endif
libc_hidden_ver (__fnmatch, fnmatch)
