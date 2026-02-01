 

 
#ifndef _GNU_SOURCE
# define _GNU_SOURCE    1
#endif

#ifndef VASNPRINTF
# include <config.h>
#endif

 
#if 10 <= __GNUC__
# pragma GCC diagnostic ignored "-Wanalyzer-null-argument"
#endif

#include <alloca.h>

 
#ifndef VASNPRINTF
# if WIDE_CHAR_VERSION
#  include "vasnwprintf.h"
# else
#  include "vasnprintf.h"
# endif
#endif

#include <locale.h>      
#include <stdio.h>       
#include <stdlib.h>      
#include <string.h>      
#include <wchar.h>       
#include <errno.h>       
#include <limits.h>      
#include <float.h>       
#if HAVE_NL_LANGINFO
# include <langinfo.h>
#endif
#ifndef VASNPRINTF
# if WIDE_CHAR_VERSION
#  include "wprintf-parse.h"
# else
#  include "printf-parse.h"
# endif
#endif

 
#include "xsize.h"

#include "attribute.h"

#if NEED_PRINTF_DOUBLE || NEED_PRINTF_LONG_DOUBLE || (NEED_WPRINTF_DIRECTIVE_LA && WIDE_CHAR_VERSION)
# include <math.h>
# include "float+.h"
#endif

#if NEED_PRINTF_DOUBLE || NEED_PRINTF_INFINITE_DOUBLE
# include <math.h>
# include "isnand-nolibm.h"
#endif

#if NEED_PRINTF_LONG_DOUBLE || NEED_PRINTF_INFINITE_LONG_DOUBLE || (NEED_WPRINTF_DIRECTIVE_LA && WIDE_CHAR_VERSION)
# include <math.h>
# include "isnanl-nolibm.h"
# include "fpucw.h"
#endif

#if NEED_PRINTF_DIRECTIVE_A || NEED_PRINTF_DOUBLE
# include <math.h>
# include "isnand-nolibm.h"
# include "printf-frexp.h"
#endif

#if NEED_PRINTF_DIRECTIVE_A || NEED_PRINTF_LONG_DOUBLE || (NEED_WPRINTF_DIRECTIVE_LA && WIDE_CHAR_VERSION)
# include <math.h>
# include "isnanl-nolibm.h"
# include "printf-frexpl.h"
# include "fpucw.h"
#endif

 
#ifndef VASNPRINTF
# if WIDE_CHAR_VERSION
#  define VASNPRINTF vasnwprintf
#  define FCHAR_T wchar_t
#  define DCHAR_T wchar_t
#  define DIRECTIVE wchar_t_directive
#  define DIRECTIVES wchar_t_directives
#  define PRINTF_PARSE wprintf_parse
#  define DCHAR_CPY wmemcpy
#  define DCHAR_SET wmemset
# else
#  define VASNPRINTF vasnprintf
#  define FCHAR_T char
#  define DCHAR_T char
#  define TCHAR_T char
#  define DCHAR_IS_TCHAR 1
#  define DIRECTIVE char_directive
#  define DIRECTIVES char_directives
#  define PRINTF_PARSE printf_parse
#  define DCHAR_CPY memcpy
#  define DCHAR_SET memset
# endif
#endif
#if WIDE_CHAR_VERSION
   
# if HAVE_DECL__SNWPRINTF || (HAVE_SWPRINTF && HAVE_WORKING_SWPRINTF)
#  define TCHAR_T wchar_t
#  define DCHAR_IS_TCHAR 1
#  define USE_SNPRINTF 1
#  if HAVE_DECL__SNWPRINTF
     
#   if defined __MINGW32__
#    define SNPRINTF snwprintf
#   else
#    define SNPRINTF _snwprintf
#    define USE_MSVC__SNPRINTF 1
#   endif
#  else
     
#   define SNPRINTF swprintf
#  endif
# else
    
#   define TCHAR_T char
# endif
#endif
#if !WIDE_CHAR_VERSION || !DCHAR_IS_TCHAR
   
   
# if (HAVE_DECL__SNPRINTF || HAVE_SNPRINTF) && !defined __BEOS__ && !(__GNU_LIBRARY__ == 1)
#  define USE_SNPRINTF 1
# else
#  define USE_SNPRINTF 0
# endif
# if HAVE_DECL__SNPRINTF
    
#  if defined __MINGW32__
#   define SNPRINTF snprintf
     
#   undef snprintf
#  else
     
#   define SNPRINTF _snprintf
#   define USE_MSVC__SNPRINTF 1
#  endif
# else
    
#  define SNPRINTF snprintf
    
#  undef snprintf
# endif
#endif
 
#undef sprintf

 
#if defined GCC_LINT || defined lint
# define IF_LINT(Code) Code
#else
# define IF_LINT(Code)  
#endif

 
#undef exp
#define exp expo
#undef remainder
#define remainder rem

#if (!USE_SNPRINTF || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF) && !WIDE_CHAR_VERSION
# if (HAVE_STRNLEN && !defined _AIX)
#  define local_strnlen strnlen
# else
#  ifndef local_strnlen_defined
#   define local_strnlen_defined 1
static size_t
local_strnlen (const char *string, size_t maxlen)
{
  const char *end = memchr (string, '\0', maxlen);
  return end ? (size_t) (end - string) : maxlen;
}
#  endif
# endif
#endif

#if (((!USE_SNPRINTF || WIDE_CHAR_VERSION || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF || NEED_WPRINTF_DIRECTIVE_LC) && WIDE_CHAR_VERSION) || ((!USE_SNPRINTF || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF || NEED_PRINTF_DIRECTIVE_LS) && !WIDE_CHAR_VERSION && DCHAR_IS_TCHAR)) && HAVE_WCHAR_T
# if HAVE_WCSLEN
#  define local_wcslen wcslen
# else
    
#  ifndef local_wcslen_defined
#   define local_wcslen_defined 1
static size_t
local_wcslen (const wchar_t *s)
{
  const wchar_t *ptr;

  for (ptr = s; *ptr != (wchar_t) 0; ptr++)
    ;
  return ptr - s;
}
#  endif
# endif
#endif

#if (!USE_SNPRINTF || (WIDE_CHAR_VERSION && DCHAR_IS_TCHAR) || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF) && HAVE_WCHAR_T && WIDE_CHAR_VERSION
# if HAVE_WCSNLEN && HAVE_DECL_WCSNLEN
#  define local_wcsnlen wcsnlen
# else
#  ifndef local_wcsnlen_defined
#   define local_wcsnlen_defined 1
static size_t
local_wcsnlen (const wchar_t *s, size_t maxlen)
{
  const wchar_t *ptr;

  for (ptr = s; maxlen > 0 && *ptr != (wchar_t) 0; ptr++, maxlen--)
    ;
  return ptr - s;
}
#  endif
# endif
#endif

#if (((!USE_SNPRINTF || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF || NEED_PRINTF_DIRECTIVE_LS || ENABLE_WCHAR_FALLBACK) && HAVE_WCHAR_T) || ((NEED_PRINTF_DIRECTIVE_LC || ENABLE_WCHAR_FALLBACK) && HAVE_WINT_T)) && !WIDE_CHAR_VERSION
# if ENABLE_WCHAR_FALLBACK
static size_t
wctomb_fallback (char *s, wchar_t wc)
{
  static char hex[16] = "0123456789ABCDEF";

  s[0] = '\\';
  if (sizeof (wchar_t) > 2 && wc > 0xffff)
    {
#  if __STDC_ISO_10646__ || (__GLIBC__ >= 2) || (defined _WIN32 || defined __CYGWIN__)
      s[1] = 'U';
#  else
      s[1] = 'W';
#  endif
      s[2] = hex[(wc & 0xf0000000U) >> 28];
      s[3] = hex[(wc & 0xf000000U) >> 24];
      s[4] = hex[(wc & 0xf00000U) >> 20];
      s[5] = hex[(wc & 0xf0000U) >> 16];
      s[6] = hex[(wc & 0xf000U) >> 12];
      s[7] = hex[(wc & 0xf00U) >> 8];
      s[8] = hex[(wc & 0xf0U) >> 4];
      s[9] = hex[wc & 0xfU];
      return 10;
    }
  else
    {
#  if __STDC_ISO_10646__ || (__GLIBC__ >= 2) || (defined _WIN32 || defined __CYGWIN__)
      s[1] = 'u';
#  else
      s[1] = 'w';
#  endif
      s[2] = hex[(wc & 0xf000U) >> 12];
      s[3] = hex[(wc & 0xf00U) >> 8];
      s[4] = hex[(wc & 0xf0U) >> 4];
      s[5] = hex[wc & 0xfU];
      return 6;
    }
}
#  if HAVE_WCRTOMB && !defined GNULIB_defined_mbstate_t
static size_t
local_wcrtomb (char *s, wchar_t wc, mbstate_t *ps)
{
  size_t count = wcrtomb (s, wc, ps);
  if (count == (size_t)(-1))
    count = wctomb_fallback (s, wc);
  return count;
}
#  else
static int
local_wctomb (char *s, wchar_t wc)
{
  int count = wctomb (s, wc);
  if (count < 0)
    count = wctomb_fallback (s, wc);
  return count;
}
#   define local_wcrtomb(S, WC, PS)  local_wctomb ((S), (WC))
#  endif
# else
#  if HAVE_WCRTOMB && !defined GNULIB_defined_mbstate_t
#   define local_wcrtomb(S, WC, PS)  wcrtomb ((S), (WC), (PS))
#  else
#   define local_wcrtomb(S, WC, PS)  wctomb ((S), (WC))
#  endif
# endif
#endif

#if NEED_PRINTF_DIRECTIVE_A || NEED_PRINTF_LONG_DOUBLE || NEED_PRINTF_INFINITE_LONG_DOUBLE || NEED_PRINTF_DOUBLE || NEED_PRINTF_INFINITE_DOUBLE || (NEED_WPRINTF_DIRECTIVE_LA && WIDE_CHAR_VERSION)
 
# ifndef decimal_point_char_defined
#  define decimal_point_char_defined 1
static char
decimal_point_char (void)
{
  const char *point;
   
#  if HAVE_NL_LANGINFO && (__GLIBC__ || defined __UCLIBC__ || (defined __APPLE__ && defined __MACH__))
  point = nl_langinfo (RADIXCHAR);
#  elif 1
  char pointbuf[5];
  sprintf (pointbuf, "%#.0f", 1.0);
  point = &pointbuf[1];
#  else
  point = localeconv () -> decimal_point;
#  endif
   
  return (point[0] != '\0' ? point[0] : '.');
}
# endif
#endif

#if NEED_PRINTF_INFINITE_DOUBLE && !NEED_PRINTF_DOUBLE

 
static int
is_infinite_or_zero (double x)
{
  return isnand (x) || x + x == x;
}

#endif

#if NEED_PRINTF_INFINITE_LONG_DOUBLE && !NEED_PRINTF_LONG_DOUBLE

 
static int
is_infinite_or_zerol (long double x)
{
  return isnanl (x) || x + x == x;
}

#endif

#if NEED_PRINTF_LONG_DOUBLE || NEED_PRINTF_DOUBLE

 

typedef unsigned int mp_limb_t;
# define GMP_LIMB_BITS 32
static_assert (sizeof (mp_limb_t) * CHAR_BIT == GMP_LIMB_BITS);

typedef unsigned long long mp_twolimb_t;
# define GMP_TWOLIMB_BITS 64
static_assert (sizeof (mp_twolimb_t) * CHAR_BIT == GMP_TWOLIMB_BITS);

 
typedef struct
{
  size_t nlimbs;
  mp_limb_t *limbs;  
} mpn_t;

 
static void *
multiply (mpn_t src1, mpn_t src2, mpn_t *dest)
{
  const mp_limb_t *p1;
  const mp_limb_t *p2;
  size_t len1;
  size_t len2;

  if (src1.nlimbs <= src2.nlimbs)
    {
      len1 = src1.nlimbs;
      p1 = src1.limbs;
      len2 = src2.nlimbs;
      p2 = src2.limbs;
    }
  else
    {
      len1 = src2.nlimbs;
      p1 = src2.limbs;
      len2 = src1.nlimbs;
      p2 = src1.limbs;
    }
   
  if (len1 == 0)
    {
       
      dest->nlimbs = 0;
      dest->limbs = (mp_limb_t *) malloc (1);
    }
  else
    {
       
      size_t dlen;
      mp_limb_t *dp;
      size_t k, i, j;

      dlen = len1 + len2;
      dp = (mp_limb_t *) malloc (dlen * sizeof (mp_limb_t));
      if (dp == NULL)
        return NULL;
      for (k = len2; k > 0; )
        dp[--k] = 0;
      for (i = 0; i < len1; i++)
        {
          mp_limb_t digit1 = p1[i];
          mp_twolimb_t carry = 0;
          for (j = 0; j < len2; j++)
            {
              mp_limb_t digit2 = p2[j];
              carry += (mp_twolimb_t) digit1 * (mp_twolimb_t) digit2;
              carry += dp[i + j];
              dp[i + j] = (mp_limb_t) carry;
              carry = carry >> GMP_LIMB_BITS;
            }
          dp[i + len2] = (mp_limb_t) carry;
        }
       
      while (dlen > 0 && dp[dlen - 1] == 0)
        dlen--;
      dest->nlimbs = dlen;
      dest->limbs = dp;
    }
  return dest->limbs;
}

 
static void *
divide (mpn_t a, mpn_t b, mpn_t *q)
{
   
  const mp_limb_t *a_ptr = a.limbs;
  size_t a_len = a.nlimbs;
  const mp_limb_t *b_ptr = b.limbs;
  size_t b_len = b.nlimbs;
  mp_limb_t *roomptr;
  mp_limb_t *tmp_roomptr = NULL;
  mp_limb_t *q_ptr;
  size_t q_len;
  mp_limb_t *r_ptr;
  size_t r_len;

   
  roomptr = (mp_limb_t *) malloc ((a_len + 2) * sizeof (mp_limb_t));
  if (roomptr == NULL)
    return NULL;

   
  while (a_len > 0 && a_ptr[a_len - 1] == 0)
    a_len--;

   
  for (;;)
    {
      if (b_len == 0)
         
        abort ();
      if (b_ptr[b_len - 1] == 0)
        b_len--;
      else
        break;
    }

   

  if (a_len < b_len)
    {
       
      r_ptr = roomptr;
      r_len = a_len;
      memcpy (r_ptr, a_ptr, a_len * sizeof (mp_limb_t));
      q_ptr = roomptr + a_len;
      q_len = 0;
    }
  else if (b_len == 1)
    {
       
      r_ptr = roomptr;
      q_ptr = roomptr + 1;
      {
        mp_limb_t den = b_ptr[0];
        mp_limb_t remainder = 0;
        const mp_limb_t *sourceptr = a_ptr + a_len;
        mp_limb_t *destptr = q_ptr + a_len;
        size_t count;
        for (count = a_len; count > 0; count--)
          {
            mp_twolimb_t num =
              ((mp_twolimb_t) remainder << GMP_LIMB_BITS) | *--sourceptr;
            *--destptr = num / den;
            remainder = num % den;
          }
         
        if (remainder > 0)
          {
            r_ptr[0] = remainder;
            r_len = 1;
          }
        else
          r_len = 0;
         
        q_len = a_len;
        if (q_ptr[q_len - 1] == 0)
          q_len--;
      }
    }
  else
    {
       
       
      size_t s;
      {
        mp_limb_t msd = b_ptr[b_len - 1];  
         
# if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) \
     || (__clang_major__ >= 4)
        s = __builtin_clz (msd);
# else
#  if defined DBL_EXPBIT0_WORD && defined DBL_EXPBIT0_BIT
        if (GMP_LIMB_BITS <= DBL_MANT_BIT)
          {
             
#   define DBL_EXP_MASK ((DBL_MAX_EXP - DBL_MIN_EXP) | 7)
#   define DBL_EXP_BIAS (DBL_EXP_MASK / 2 - 1)
#   define NWORDS \
     ((sizeof (double) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
            union { double value; unsigned int word[NWORDS]; } m;

             
            m.value = msd;

            s = GMP_LIMB_BITS
                - (((m.word[DBL_EXPBIT0_WORD] >> DBL_EXPBIT0_BIT) & DBL_EXP_MASK)
                   - DBL_EXP_BIAS);
          }
        else
#   undef NWORDS
#  endif
          {
            s = 31;
            if (msd >= 0x10000)
              {
                msd = msd >> 16;
                s -= 16;
              }
            if (msd >= 0x100)
              {
                msd = msd >> 8;
                s -= 8;
              }
            if (msd >= 0x10)
              {
                msd = msd >> 4;
                s -= 4;
              }
            if (msd >= 0x4)
              {
                msd = msd >> 2;
                s -= 2;
              }
            if (msd >= 0x2)
              {
                msd = msd >> 1;
                s -= 1;
              }
          }
# endif
      }
       
      if (s > 0)
        {
          tmp_roomptr = (mp_limb_t *) malloc (b_len * sizeof (mp_limb_t));
          if (tmp_roomptr == NULL)
            {
              free (roomptr);
              return NULL;
            }
          {
            const mp_limb_t *sourceptr = b_ptr;
            mp_limb_t *destptr = tmp_roomptr;
            mp_twolimb_t accu = 0;
            size_t count;
            for (count = b_len; count > 0; count--)
              {
                accu += (mp_twolimb_t) *sourceptr++ << s;
                *destptr++ = (mp_limb_t) accu;
                accu = accu >> GMP_LIMB_BITS;
              }
             
            if (accu != 0)
              abort ();
          }
          b_ptr = tmp_roomptr;
        }
       
      r_ptr = roomptr;
      if (s == 0)
        {
          memcpy (r_ptr, a_ptr, a_len * sizeof (mp_limb_t));
          r_ptr[a_len] = 0;
        }
      else
        {
          const mp_limb_t *sourceptr = a_ptr;
          mp_limb_t *destptr = r_ptr;
          mp_twolimb_t accu = 0;
          size_t count;
          for (count = a_len; count > 0; count--)
            {
              accu += (mp_twolimb_t) *sourceptr++ << s;
              *destptr++ = (mp_limb_t) accu;
              accu = accu >> GMP_LIMB_BITS;
            }
          *destptr++ = (mp_limb_t) accu;
        }
      q_ptr = roomptr + b_len;
      q_len = a_len - b_len + 1;  
      {
        size_t j = a_len - b_len;  
        mp_limb_t b_msd = b_ptr[b_len - 1];  
        mp_limb_t b_2msd = b_ptr[b_len - 2];  
        mp_twolimb_t b_msdd =  
          ((mp_twolimb_t) b_msd << GMP_LIMB_BITS) | b_2msd;
         
        for (;;)
          {
            mp_limb_t q_star;
            mp_limb_t c1;
            if (r_ptr[j + b_len] < b_msd)  
              {
                 
                mp_twolimb_t num =
                  ((mp_twolimb_t) r_ptr[j + b_len] << GMP_LIMB_BITS)
                  | r_ptr[j + b_len - 1];
                q_star = num / b_msd;
                c1 = num % b_msd;
              }
            else
              {
                 
                q_star = (mp_limb_t)~(mp_limb_t)0;  
                 
                if (r_ptr[j + b_len] > b_msd
                    || (c1 = r_ptr[j + b_len - 1] + b_msd) < b_msd)
                   
                  goto subtract;
              }
             
            {
              mp_twolimb_t c2 =  
                ((mp_twolimb_t) c1 << GMP_LIMB_BITS) | r_ptr[j + b_len - 2];
              mp_twolimb_t c3 =  
                (mp_twolimb_t) b_2msd * (mp_twolimb_t) q_star;
               
              if (c3 > c2)
                {
                  q_star = q_star - 1;  
                  if (c3 - c2 > b_msdd)
                    q_star = q_star - 1;  
                }
            }
            if (q_star > 0)
              subtract:
              {
                 
                mp_limb_t cr;
                {
                  const mp_limb_t *sourceptr = b_ptr;
                  mp_limb_t *destptr = r_ptr + j;
                  mp_twolimb_t carry = 0;
                  size_t count;
                  for (count = b_len; count > 0; count--)
                    {
                       
                      carry =
                        carry
                        + (mp_twolimb_t) q_star * (mp_twolimb_t) *sourceptr++
                        + (mp_limb_t) ~(*destptr);
                       
                      *destptr++ = ~(mp_limb_t) carry;
                      carry = carry >> GMP_LIMB_BITS;  
                    }
                  cr = (mp_limb_t) carry;
                }
                 
                if (cr > r_ptr[j + b_len])
                  {
                     
                    q_star = q_star - 1;  
                     
                    {
                      const mp_limb_t *sourceptr = b_ptr;
                      mp_limb_t *destptr = r_ptr + j;
                      mp_limb_t carry = 0;
                      size_t count;
                      for (count = b_len; count > 0; count--)
                        {
                          mp_limb_t source1 = *sourceptr++;
                          mp_limb_t source2 = *destptr;
                          *destptr++ = source1 + source2 + carry;
                          carry =
                            (carry
                             ? source1 >= (mp_limb_t) ~source2
                             : source1 > (mp_limb_t) ~source2);
                        }
                    }
                     
                  }
              }
             
            q_ptr[j] = q_star;
            if (j == 0)
              break;
            j--;
          }
      }
      r_len = b_len;
       
      if (q_ptr[q_len - 1] == 0)
        q_len--;
# if 0  
       
      if (s > 0)
        {
          mp_limb_t ptr = r_ptr + r_len;
          mp_twolimb_t accu = 0;
          size_t count;
          for (count = r_len; count > 0; count--)
            {
              accu = (mp_twolimb_t) (mp_limb_t) accu << GMP_LIMB_BITS;
              accu += (mp_twolimb_t) *--ptr << (GMP_LIMB_BITS - s);
              *ptr = (mp_limb_t) (accu >> GMP_LIMB_BITS);
            }
        }
# endif
       
      while (r_len > 0 && r_ptr[r_len - 1] == 0)
        r_len--;
    }
   
  if (r_len > b_len)
    goto increment_q;
  {
    size_t i;
    for (i = b_len;;)
      {
        mp_limb_t r_i =
          (i <= r_len && i > 0 ? r_ptr[i - 1] >> (GMP_LIMB_BITS - 1) : 0)
          | (i < r_len ? r_ptr[i] << 1 : 0);
        mp_limb_t b_i = (i < b_len ? b_ptr[i] : 0);
        if (r_i > b_i)
          goto increment_q;
        if (r_i < b_i)
          goto keep_q;
        if (i == 0)
          break;
        i--;
      }
  }
  if (q_len > 0 && ((q_ptr[0] & 1) != 0))
     
    increment_q:
    {
      size_t i;
      for (i = 0; i < q_len; i++)
        if (++(q_ptr[i]) != 0)
          goto keep_q;
      q_ptr[q_len++] = 1;
    }
  keep_q:
  free (tmp_roomptr);
  q->limbs = q_ptr;
  q->nlimbs = q_len;
  return roomptr;
}

 
# if __GNUC__ >= 7
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Walloc-size-larger-than="
# endif

 
static char *
convert_to_decimal (mpn_t a, size_t extra_zeroes)
{
  mp_limb_t *a_ptr = a.limbs;
  size_t a_len = a.nlimbs;
   
  size_t c_len = 9 * ((size_t)(a_len * (GMP_LIMB_BITS * 0.03345f)) + 1);
   
  char *c_ptr = (char *) malloc (xsum (xsum (extra_zeroes, c_len), 1));
  if (c_ptr != NULL)
    {
      char *d_ptr = c_ptr;
      for (; extra_zeroes > 0; extra_zeroes--)
        *d_ptr++ = '0';
      while (a_len > 0)
        {
           
          mp_limb_t remainder = 0;
          mp_limb_t *ptr = a_ptr + a_len;
          size_t count;
          for (count = a_len; count > 0; count--)
            {
              mp_twolimb_t num =
                ((mp_twolimb_t) remainder << GMP_LIMB_BITS) | *--ptr;
              *ptr = num / 1000000000;
              remainder = num % 1000000000;
            }
           
          for (count = 9; count > 0; count--)
            {
              *d_ptr++ = '0' + (remainder % 10);
              remainder = remainder / 10;
            }
           
          if (a_ptr[a_len - 1] == 0)
            a_len--;
        }
       
      while (d_ptr > c_ptr && d_ptr[-1] == '0')
        d_ptr--;
       
      if (d_ptr == c_ptr)
        *d_ptr++ = '0';
       
      *d_ptr = '\0';
    }
  return c_ptr;
}

# if __GNUC__ >= 7
#  pragma GCC diagnostic pop
# endif

# if NEED_PRINTF_LONG_DOUBLE

 
static void *
decode_long_double (long double x, int *ep, mpn_t *mp)
{
  mpn_t m;
  int exp;
  long double y;
  size_t i;

   
  m.nlimbs = (LDBL_MANT_BIT + GMP_LIMB_BITS - 1) / GMP_LIMB_BITS;
  m.limbs = (mp_limb_t *) malloc (m.nlimbs * sizeof (mp_limb_t));
  if (m.limbs == NULL)
    return NULL;
   
  y = frexpl (x, &exp);
  if (!(y >= 0.0L && y < 1.0L))
    abort ();
   
   
#  if (LDBL_MANT_BIT % GMP_LIMB_BITS) != 0
#   if (LDBL_MANT_BIT % GMP_LIMB_BITS) > GMP_LIMB_BITS / 2
    {
      mp_limb_t hi, lo;
      y *= (mp_limb_t) 1 << (LDBL_MANT_BIT % (GMP_LIMB_BITS / 2));
      hi = (int) y;
      y -= hi;
      if (!(y >= 0.0L && y < 1.0L))
        abort ();
      y *= (mp_limb_t) 1 << (GMP_LIMB_BITS / 2);
      lo = (int) y;
      y -= lo;
      if (!(y >= 0.0L && y < 1.0L))
        abort ();
      m.limbs[LDBL_MANT_BIT / GMP_LIMB_BITS] = (hi << (GMP_LIMB_BITS / 2)) | lo;
    }
#   else
    {
      mp_limb_t d;
      y *= (mp_limb_t) 1 << (LDBL_MANT_BIT % GMP_LIMB_BITS);
      d = (int) y;
      y -= d;
      if (!(y >= 0.0L && y < 1.0L))
        abort ();
      m.limbs[LDBL_MANT_BIT / GMP_LIMB_BITS] = d;
    }
#   endif
#  endif
  for (i = LDBL_MANT_BIT / GMP_LIMB_BITS; i > 0; )
    {
      mp_limb_t hi, lo;
      y *= (mp_limb_t) 1 << (GMP_LIMB_BITS / 2);
      hi = (int) y;
      y -= hi;
      if (!(y >= 0.0L && y < 1.0L))
        abort ();
      y *= (mp_limb_t) 1 << (GMP_LIMB_BITS / 2);
      lo = (int) y;
      y -= lo;
      if (!(y >= 0.0L && y < 1.0L))
        abort ();
      m.limbs[--i] = (hi << (GMP_LIMB_BITS / 2)) | lo;
    }
#  if 0  
  if (!(y == 0.0L))
    abort ();
#  endif
   
  while (m.nlimbs > 0 && m.limbs[m.nlimbs - 1] == 0)
    m.nlimbs--;
  *mp = m;
  *ep = exp - LDBL_MANT_BIT;
  return m.limbs;
}

# endif

# if NEED_PRINTF_DOUBLE

 
static void *
decode_double (double x, int *ep, mpn_t *mp)
{
  mpn_t m;
  int exp;
  double y;
  size_t i;

   
  m.nlimbs = (DBL_MANT_BIT + GMP_LIMB_BITS - 1) / GMP_LIMB_BITS;
  m.limbs = (mp_limb_t *) malloc (m.nlimbs * sizeof (mp_limb_t));
  if (m.limbs == NULL)
    return NULL;
   
  y = frexp (x, &exp);
  if (!(y >= 0.0 && y < 1.0))
    abort ();
   
   
#  if (DBL_MANT_BIT % GMP_LIMB_BITS) != 0
#   if (DBL_MANT_BIT % GMP_LIMB_BITS) > GMP_LIMB_BITS / 2
    {
      mp_limb_t hi, lo;
      y *= (mp_limb_t) 1 << (DBL_MANT_BIT % (GMP_LIMB_BITS / 2));
      hi = (int) y;
      y -= hi;
      if (!(y >= 0.0 && y < 1.0))
        abort ();
      y *= (mp_limb_t) 1 << (GMP_LIMB_BITS / 2);
      lo = (int) y;
      y -= lo;
      if (!(y >= 0.0 && y < 1.0))
        abort ();
      m.limbs[DBL_MANT_BIT / GMP_LIMB_BITS] = (hi << (GMP_LIMB_BITS / 2)) | lo;
    }
#   else
    {
      mp_limb_t d;
      y *= (mp_limb_t) 1 << (DBL_MANT_BIT % GMP_LIMB_BITS);
      d = (int) y;
      y -= d;
      if (!(y >= 0.0 && y < 1.0))
        abort ();
      m.limbs[DBL_MANT_BIT / GMP_LIMB_BITS] = d;
    }
#   endif
#  endif
  for (i = DBL_MANT_BIT / GMP_LIMB_BITS; i > 0; )
    {
      mp_limb_t hi, lo;
      y *= (mp_limb_t) 1 << (GMP_LIMB_BITS / 2);
      hi = (int) y;
      y -= hi;
      if (!(y >= 0.0 && y < 1.0))
        abort ();
      y *= (mp_limb_t) 1 << (GMP_LIMB_BITS / 2);
      lo = (int) y;
      y -= lo;
      if (!(y >= 0.0 && y < 1.0))
        abort ();
      m.limbs[--i] = (hi << (GMP_LIMB_BITS / 2)) | lo;
    }
  if (!(y == 0.0))
    abort ();
   
  while (m.nlimbs > 0 && m.limbs[m.nlimbs - 1] == 0)
    m.nlimbs--;
  *mp = m;
  *ep = exp - DBL_MANT_BIT;
  return m.limbs;
}

# endif

 
static char *
scale10_round_decimal_decoded (int e, mpn_t m, void *memory, int n)
{
  int s;
  size_t extra_zeroes;
  unsigned int abs_n;
  unsigned int abs_s;
  mp_limb_t *pow5_ptr;
  size_t pow5_len;
  unsigned int s_limbs;
  unsigned int s_bits;
  mpn_t pow5;
  mpn_t z;
  void *z_memory;
  char *digits;

   
  s = e + n;
  extra_zeroes = 0;
   
  if (s > 0 && n > 0)
    {
      extra_zeroes = (s < n ? s : n);
      s -= extra_zeroes;
      n -= extra_zeroes;
    }
   
   
  abs_n = (n >= 0 ? n : -n);
  abs_s = (s >= 0 ? s : -s);
  pow5_ptr = (mp_limb_t *) malloc (((int)(abs_n * (2.322f / GMP_LIMB_BITS)) + 1
                                    + abs_s / GMP_LIMB_BITS + 1)
                                   * sizeof (mp_limb_t));
  if (pow5_ptr == NULL)
    {
      free (memory);
      return NULL;
    }
   
  pow5_ptr[0] = 1;
  pow5_len = 1;
   
  if (abs_n > 0)
    {
      static mp_limb_t const small_pow5[13 + 1] =
        {
          1, 5, 25, 125, 625, 3125, 15625, 78125, 390625, 1953125, 9765625,
          48828125, 244140625, 1220703125
        };
      unsigned int n13;
      for (n13 = 0; n13 <= abs_n; n13 += 13)
        {
          mp_limb_t digit1 = small_pow5[n13 + 13 <= abs_n ? 13 : abs_n - n13];
          size_t j;
          mp_twolimb_t carry = 0;
          for (j = 0; j < pow5_len; j++)
            {
              mp_limb_t digit2 = pow5_ptr[j];
              carry += (mp_twolimb_t) digit1 * (mp_twolimb_t) digit2;
              pow5_ptr[j] = (mp_limb_t) carry;
              carry = carry >> GMP_LIMB_BITS;
            }
          if (carry > 0)
            pow5_ptr[pow5_len++] = (mp_limb_t) carry;
        }
    }
  s_limbs = abs_s / GMP_LIMB_BITS;
  s_bits = abs_s % GMP_LIMB_BITS;
  if (n >= 0 ? s >= 0 : s <= 0)
    {
       
      if (s_bits > 0)
        {
          mp_limb_t *ptr = pow5_ptr;
          mp_twolimb_t accu = 0;
          size_t count;
          for (count = pow5_len; count > 0; count--)
            {
              accu += (mp_twolimb_t) *ptr << s_bits;
              *ptr++ = (mp_limb_t) accu;
              accu = accu >> GMP_LIMB_BITS;
            }
          if (accu > 0)
            {
              *ptr = (mp_limb_t) accu;
              pow5_len++;
            }
        }
      if (s_limbs > 0)
        {
          size_t count;
          for (count = pow5_len; count > 0;)
            {
              count--;
              pow5_ptr[s_limbs + count] = pow5_ptr[count];
            }
          for (count = s_limbs; count > 0;)
            {
              count--;
              pow5_ptr[count] = 0;
            }
          pow5_len += s_limbs;
        }
      pow5.limbs = pow5_ptr;
      pow5.nlimbs = pow5_len;
      if (n >= 0)
        {
           
          z_memory = multiply (m, pow5, &z);
        }
      else
        {
           
          z_memory = divide (m, pow5, &z);
        }
    }
  else
    {
      pow5.limbs = pow5_ptr;
      pow5.nlimbs = pow5_len;
      if (n >= 0)
        {
           
          mpn_t numerator;
          mpn_t denominator;
          void *tmp_memory;
          tmp_memory = multiply (m, pow5, &numerator);
          if (tmp_memory == NULL)
            {
              free (pow5_ptr);
              free (memory);
              return NULL;
            }
           
          {
            mp_limb_t *ptr = pow5_ptr + pow5_len;
            size_t i;
            for (i = 0; i < s_limbs; i++)
              ptr[i] = 0;
            ptr[s_limbs] = (mp_limb_t) 1 << s_bits;
            denominator.limbs = ptr;
            denominator.nlimbs = s_limbs + 1;
          }
          z_memory = divide (numerator, denominator, &z);
          free (tmp_memory);
        }
      else
        {
           
          mpn_t numerator;
          mp_limb_t *num_ptr;
          num_ptr = (mp_limb_t *) malloc ((m.nlimbs + s_limbs + 1)
                                          * sizeof (mp_limb_t));
          if (num_ptr == NULL)
            {
              free (pow5_ptr);
              free (memory);
              return NULL;
            }
          {
            mp_limb_t *destptr = num_ptr;
            {
              size_t i;
              for (i = 0; i < s_limbs; i++)
                *destptr++ = 0;
            }
            if (s_bits > 0)
              {
                const mp_limb_t *sourceptr = m.limbs;
                mp_twolimb_t accu = 0;
                size_t count;
                for (count = m.nlimbs; count > 0; count--)
                  {
                    accu += (mp_twolimb_t) *sourceptr++ << s_bits;
                    *destptr++ = (mp_limb_t) accu;
                    accu = accu >> GMP_LIMB_BITS;
                  }
                if (accu > 0)
                  *destptr++ = (mp_limb_t) accu;
              }
            else
              {
                const mp_limb_t *sourceptr = m.limbs;
                size_t count;
                for (count = m.nlimbs; count > 0; count--)
                  *destptr++ = *sourceptr++;
              }
            numerator.limbs = num_ptr;
            numerator.nlimbs = destptr - num_ptr;
          }
          z_memory = divide (numerator, pow5, &z);
          free (num_ptr);
        }
    }
  free (pow5_ptr);
  free (memory);

   

  if (z_memory == NULL)
    return NULL;
  digits = convert_to_decimal (z, extra_zeroes);
  free (z_memory);
  return digits;
}

# if NEED_PRINTF_LONG_DOUBLE

 
static char *
scale10_round_decimal_long_double (long double x, int n)
{
  int e;
  mpn_t m;
  void *memory = decode_long_double (x, &e, &m);
  if (memory != NULL)
    return scale10_round_decimal_decoded (e, m, memory, n);
  else
    return NULL;
}

# endif

# if NEED_PRINTF_DOUBLE

 
static char *
scale10_round_decimal_double (double x, int n)
{
  int e;
  mpn_t m;
  void *memory = decode_double (x, &e, &m);
  if (memory != NULL)
    return scale10_round_decimal_decoded (e, m, memory, n);
  else
    return NULL;
}

# endif

# if NEED_PRINTF_LONG_DOUBLE

 
static int
floorlog10l (long double x)
{
  int exp;
  long double y;
  double z;
  double l;

   
  y = frexpl (x, &exp);
  if (!(y >= 0.0L && y < 1.0L))
    abort ();
  if (y == 0.0L)
    return INT_MIN;
  if (y < 0.5L)
    {
      while (y < (1.0L / (1 << (GMP_LIMB_BITS / 2)) / (1 << (GMP_LIMB_BITS / 2))))
        {
          y *= 1.0L * (1 << (GMP_LIMB_BITS / 2)) * (1 << (GMP_LIMB_BITS / 2));
          exp -= GMP_LIMB_BITS;
        }
      if (y < (1.0L / (1 << 16)))
        {
          y *= 1.0L * (1 << 16);
          exp -= 16;
        }
      if (y < (1.0L / (1 << 8)))
        {
          y *= 1.0L * (1 << 8);
          exp -= 8;
        }
      if (y < (1.0L / (1 << 4)))
        {
          y *= 1.0L * (1 << 4);
          exp -= 4;
        }
      if (y < (1.0L / (1 << 2)))
        {
          y *= 1.0L * (1 << 2);
          exp -= 2;
        }
      if (y < (1.0L / (1 << 1)))
        {
          y *= 1.0L * (1 << 1);
          exp -= 1;
        }
    }
  if (!(y >= 0.5L && y < 1.0L))
    abort ();
   
  l = exp;
  z = y;
  if (z < 0.70710678118654752444)
    {
      z *= 1.4142135623730950488;
      l -= 0.5;
    }
  if (z < 0.8408964152537145431)
    {
      z *= 1.1892071150027210667;
      l -= 0.25;
    }
  if (z < 0.91700404320467123175)
    {
      z *= 1.0905077326652576592;
      l -= 0.125;
    }
  if (z < 0.9576032806985736469)
    {
      z *= 1.0442737824274138403;
      l -= 0.0625;
    }
   
  z = 1 - z;
   
  l -= 1.4426950408889634074 * z * (1.0 + z * (0.5 + z * ((1.0 / 3) + z * 0.25)));
   
  l *= 0.30102999566398119523;
   
  return (int) l + (l < 0 ? -1 : 0);
}

# endif

# if NEED_PRINTF_DOUBLE

 
static int
floorlog10 (double x)
{
  int exp;
  double y;
  double z;
  double l;

   
  y = frexp (x, &exp);
  if (!(y >= 0.0 && y < 1.0))
    abort ();
  if (y == 0.0)
    return INT_MIN;
  if (y < 0.5)
    {
      while (y < (1.0 / (1 << (GMP_LIMB_BITS / 2)) / (1 << (GMP_LIMB_BITS / 2))))
        {
          y *= 1.0 * (1 << (GMP_LIMB_BITS / 2)) * (1 << (GMP_LIMB_BITS / 2));
          exp -= GMP_LIMB_BITS;
        }
      if (y < (1.0 / (1 << 16)))
        {
          y *= 1.0 * (1 << 16);
          exp -= 16;
        }
      if (y < (1.0 / (1 << 8)))
        {
          y *= 1.0 * (1 << 8);
          exp -= 8;
        }
      if (y < (1.0 / (1 << 4)))
        {
          y *= 1.0 * (1 << 4);
          exp -= 4;
        }
      if (y < (1.0 / (1 << 2)))
        {
          y *= 1.0 * (1 << 2);
          exp -= 2;
        }
      if (y < (1.0 / (1 << 1)))
        {
          y *= 1.0 * (1 << 1);
          exp -= 1;
        }
    }
  if (!(y >= 0.5 && y < 1.0))
    abort ();
   
  l = exp;
  z = y;
  if (z < 0.70710678118654752444)
    {
      z *= 1.4142135623730950488;
      l -= 0.5;
    }
  if (z < 0.8408964152537145431)
    {
      z *= 1.1892071150027210667;
      l -= 0.25;
    }
  if (z < 0.91700404320467123175)
    {
      z *= 1.0905077326652576592;
      l -= 0.125;
    }
  if (z < 0.9576032806985736469)
    {
      z *= 1.0442737824274138403;
      l -= 0.0625;
    }
   
  z = 1 - z;
   
  l -= 1.4426950408889634074 * z * (1.0 + z * (0.5 + z * ((1.0 / 3) + z * 0.25)));
   
  l *= 0.30102999566398119523;
   
  return (int) l + (l < 0 ? -1 : 0);
}

# endif

 
static int
is_borderline (const char *digits, size_t precision)
{
  for (; precision > 0; precision--, digits++)
    if (*digits != '0')
      return 0;
  if (*digits != '1')
    return 0;
  digits++;
  return *digits == '\0';
}

#endif

#if !USE_SNPRINTF || (WIDE_CHAR_VERSION && DCHAR_IS_TCHAR) || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF

 
# if WIDE_CHAR_VERSION
#  define MAX_ROOM_NEEDED wmax_room_needed
# else
#  define MAX_ROOM_NEEDED max_room_needed
# endif

 
static size_t
MAX_ROOM_NEEDED (const arguments *ap, size_t arg_index, FCHAR_T conversion,
                 arg_type type, int flags, size_t width, int has_precision,
                 size_t precision, int pad_ourselves)
{
  size_t tmp_length;

  switch (conversion)
    {
    case 'd': case 'i': case 'u':
      switch (type)
        {
        default:
          tmp_length =
            (unsigned int) (sizeof (unsigned int) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_LONGINT:
          tmp_length =
            (unsigned int) (sizeof (long int) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_ULONGINT:
          tmp_length =
            (unsigned int) (sizeof (unsigned long int) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_LONGLONGINT:
          tmp_length =
            (unsigned int) (sizeof (long long int) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_ULONGLONGINT:
          tmp_length =
            (unsigned int) (sizeof (unsigned long long int) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_INT8_T:
          tmp_length =
            (unsigned int) (sizeof (int8_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_UINT8_T:
          tmp_length =
            (unsigned int) (sizeof (uint8_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_INT16_T:
          tmp_length =
            (unsigned int) (sizeof (int16_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_UINT16_T:
          tmp_length =
            (unsigned int) (sizeof (uint16_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_INT32_T:
          tmp_length =
            (unsigned int) (sizeof (int32_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_UINT32_T:
          tmp_length =
            (unsigned int) (sizeof (uint32_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_INT64_T:
          tmp_length =
            (unsigned int) (sizeof (int64_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_UINT64_T:
          tmp_length =
            (unsigned int) (sizeof (uint64_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_INT_FAST8_T:
          tmp_length =
            (unsigned int) (sizeof (int_fast8_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST8_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast8_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_INT_FAST16_T:
          tmp_length =
            (unsigned int) (sizeof (int_fast16_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST16_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast16_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_INT_FAST32_T:
          tmp_length =
            (unsigned int) (sizeof (int_fast32_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST32_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast32_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_INT_FAST64_T:
          tmp_length =
            (unsigned int) (sizeof (int_fast64_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST64_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast64_t) * CHAR_BIT
                            * 0.30103  
                           )
            + 1;  
          break;
        }
      if (tmp_length < precision)
        tmp_length = precision;
       
      tmp_length = xsum (tmp_length, tmp_length);
       
      tmp_length = xsum (tmp_length, 1);
      break;

    case 'b':
    #if SUPPORT_GNU_PRINTF_DIRECTIVES \
        || (__GLIBC__ + (__GLIBC_MINOR__ >= 35) > 2)
    case 'B':
    #endif
      switch (type)
        {
        default:
          tmp_length =
            (unsigned int) (sizeof (unsigned int) * CHAR_BIT)
            + 1;  
          break;
        case TYPE_ULONGINT:
          tmp_length =
            (unsigned int) (sizeof (unsigned long int) * CHAR_BIT)
            + 1;  
          break;
        case TYPE_ULONGLONGINT:
          tmp_length =
            (unsigned int) (sizeof (unsigned long long int) * CHAR_BIT)
            + 1;  
          break;
        case TYPE_UINT8_T:
          tmp_length =
            (unsigned int) (sizeof (uint8_t) * CHAR_BIT)
            + 1;  
          break;
        case TYPE_UINT16_T:
          tmp_length =
            (unsigned int) (sizeof (uint16_t) * CHAR_BIT)
            + 1;  
          break;
        case TYPE_UINT32_T:
          tmp_length =
            (unsigned int) (sizeof (uint32_t) * CHAR_BIT)
            + 1;  
          break;
        case TYPE_UINT64_T:
          tmp_length =
            (unsigned int) (sizeof (uint64_t) * CHAR_BIT)
            + 1;  
          break;
        case TYPE_UINT_FAST8_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast8_t) * CHAR_BIT)
            + 1;  
          break;
        case TYPE_UINT_FAST16_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast16_t) * CHAR_BIT)
            + 1;  
          break;
        case TYPE_UINT_FAST32_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast32_t) * CHAR_BIT)
            + 1;  
          break;
        case TYPE_UINT_FAST64_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast64_t) * CHAR_BIT)
            + 1;  
          break;
        }
      if (tmp_length < precision)
        tmp_length = precision;
       
      tmp_length = xsum (tmp_length, 2);
      break;

    case 'o':
      switch (type)
        {
        default:
          tmp_length =
            (unsigned int) (sizeof (unsigned int) * CHAR_BIT
                            * 0.333334  
                           )
            + 1;  
          break;
        case TYPE_ULONGINT:
          tmp_length =
            (unsigned int) (sizeof (unsigned long int) * CHAR_BIT
                            * 0.333334  
                           )
            + 1;  
          break;
        case TYPE_ULONGLONGINT:
          tmp_length =
            (unsigned int) (sizeof (unsigned long long int) * CHAR_BIT
                            * 0.333334  
                           )
            + 1;  
          break;
        case TYPE_UINT8_T:
          tmp_length =
            (unsigned int) (sizeof (uint8_t) * CHAR_BIT
                            * 0.333334  
                           )
            + 1;  
          break;
        case TYPE_UINT16_T:
          tmp_length =
            (unsigned int) (sizeof (uint16_t) * CHAR_BIT
                            * 0.333334  
                           )
            + 1;  
          break;
        case TYPE_UINT32_T:
          tmp_length =
            (unsigned int) (sizeof (uint32_t) * CHAR_BIT
                            * 0.333334  
                           )
            + 1;  
          break;
        case TYPE_UINT64_T:
          tmp_length =
            (unsigned int) (sizeof (uint64_t) * CHAR_BIT
                            * 0.333334  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST8_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast8_t) * CHAR_BIT
                            * 0.333334  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST16_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast16_t) * CHAR_BIT
                            * 0.333334  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST32_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast32_t) * CHAR_BIT
                            * 0.333334  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST64_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast64_t) * CHAR_BIT
                            * 0.333334  
                           )
            + 1;  
          break;
        }
      if (tmp_length < precision)
        tmp_length = precision;
       
      tmp_length = xsum (tmp_length, 1);
      break;

    case 'x': case 'X':
      switch (type)
        {
        default:
          tmp_length =
            (unsigned int) (sizeof (unsigned int) * CHAR_BIT
                            * 0.25  
                           )
            + 1;  
          break;
        case TYPE_ULONGINT:
          tmp_length =
            (unsigned int) (sizeof (unsigned long int) * CHAR_BIT
                            * 0.25  
                           )
            + 1;  
          break;
        case TYPE_ULONGLONGINT:
          tmp_length =
            (unsigned int) (sizeof (unsigned long long int) * CHAR_BIT
                            * 0.25  
                           )
            + 1;  
          break;
        case TYPE_UINT8_T:
          tmp_length =
            (unsigned int) (sizeof (uint8_t) * CHAR_BIT
                            * 0.25  
                           )
            + 1;  
          break;
        case TYPE_UINT16_T:
          tmp_length =
            (unsigned int) (sizeof (uint16_t) * CHAR_BIT
                            * 0.25  
                           )
            + 1;  
          break;
        case TYPE_UINT32_T:
          tmp_length =
            (unsigned int) (sizeof (uint32_t) * CHAR_BIT
                            * 0.25  
                           )
            + 1;  
          break;
        case TYPE_UINT64_T:
          tmp_length =
            (unsigned int) (sizeof (uint64_t) * CHAR_BIT
                            * 0.25  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST8_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast8_t) * CHAR_BIT
                            * 0.25  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST16_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast16_t) * CHAR_BIT
                            * 0.25  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST32_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast32_t) * CHAR_BIT
                            * 0.25  
                           )
            + 1;  
          break;
        case TYPE_UINT_FAST64_T:
          tmp_length =
            (unsigned int) (sizeof (uint_fast64_t) * CHAR_BIT
                            * 0.25  
                           )
            + 1;  
          break;
        }
      if (tmp_length < precision)
        tmp_length = precision;
       
      tmp_length = xsum (tmp_length, 2);
      break;

    case 'f': case 'F':
      if (type == TYPE_LONGDOUBLE)
        tmp_length =
          (unsigned int) (LDBL_MAX_EXP
                          * 0.30103  
                          * 2  
                         )
          + 1  
          + 10;  
      else
        tmp_length =
          (unsigned int) (DBL_MAX_EXP
                          * 0.30103  
                          * 2  
                         )
          + 1  
          + 10;  
      tmp_length = xsum (tmp_length, precision);
      break;

    case 'e': case 'E': case 'g': case 'G':
      tmp_length =
        12;  
      tmp_length = xsum (tmp_length, precision);
      break;

    case 'a': case 'A':
      if (type == TYPE_LONGDOUBLE)
        tmp_length =
          (unsigned int) (LDBL_DIG
                          * 0.831  
                         )
          + 1;  
      else
        tmp_length =
          (unsigned int) (DBL_DIG
                          * 0.831  
                         )
          + 1;  
      if (tmp_length < precision)
        tmp_length = precision;
       
      tmp_length = xsum (tmp_length, 12);
      break;

    case 'c':
# if HAVE_WINT_T && !WIDE_CHAR_VERSION
      if (type == TYPE_WIDE_CHAR)
        {
          tmp_length = MB_CUR_MAX;
#  if ENABLE_WCHAR_FALLBACK
          if (tmp_length < (sizeof (wchar_t) > 2 ? 10 : 6))
            tmp_length = (sizeof (wchar_t) > 2 ? 10 : 6);
#  endif
        }
      else
# endif
        tmp_length = 1;
      break;

    case 's':
# if HAVE_WCHAR_T
      if (type == TYPE_WIDE_STRING)
        {
#  if WIDE_CHAR_VERSION
           
          const wchar_t *arg = ap->arg[arg_index].a.a_wide_string;

          if (has_precision)
            tmp_length = local_wcsnlen (arg, precision);
          else
            tmp_length = local_wcslen (arg);
#  else
           
           
          abort ();
#  endif
        }
      else
# endif
        {
# if WIDE_CHAR_VERSION
           
           
          abort ();
# else
           
          const char *arg = ap->arg[arg_index].a.a_string;

          if (has_precision)
            tmp_length = local_strnlen (arg, precision);
          else
            tmp_length = strlen (arg);
# endif
        }
      break;

    case 'p':
      tmp_length =
        (unsigned int) (sizeof (void *) * CHAR_BIT
                        * 0.25  
                       )
          + 1  
          + 2;  
      break;

    default:
      abort ();
    }

  if (!pad_ourselves)
    {
# if ENABLE_UNISTDIO
       
      tmp_length = xsum (tmp_length, width);
# else
       
      if (tmp_length < width)
        tmp_length = width;
# endif
    }

  tmp_length = xsum (tmp_length, 1);  

  return tmp_length;
}

#endif

DCHAR_T *
VASNPRINTF (DCHAR_T *resultbuf, size_t *lengthp,
            const FCHAR_T *format, va_list args)
{
  DIRECTIVES d;
  arguments a;

  if (PRINTF_PARSE (format, &d, &a) < 0)
     
    return NULL;

   
#define CLEANUP() \
  if (d.dir != d.direct_alloc_dir)                                      \
    free (d.dir);                                                       \
  if (a.arg != a.direct_alloc_arg)                                      \
    free (a.arg);

  if (PRINTF_FETCHARGS (args, &a) < 0)
    goto fail_1_with_EINVAL;

  {
    size_t buf_neededlength;
    TCHAR_T *buf;
    TCHAR_T *buf_malloced;
    const FCHAR_T *cp;
    size_t i;
    DIRECTIVE *dp;
     
    DCHAR_T *result;
    size_t allocated;
    size_t length;

     
    buf_neededlength =
      xsum4 (7, d.max_width_length, d.max_precision_length, 6);
#if HAVE_ALLOCA
    if (buf_neededlength < 4000 / sizeof (TCHAR_T))
      {
        buf = (TCHAR_T *) alloca (buf_neededlength * sizeof (TCHAR_T));
        buf_malloced = NULL;
      }
    else
#endif
      {
        size_t buf_memsize = xtimes (buf_neededlength, sizeof (TCHAR_T));
        if (size_overflow_p (buf_memsize))
          goto out_of_memory_1;
        buf = (TCHAR_T *) malloc (buf_memsize);
        if (buf == NULL)
          goto out_of_memory_1;
        buf_malloced = buf;
      }

    result = resultbuf;
    allocated = (resultbuf != NULL ? *lengthp : 0);
    length = 0;
     

     
#define ENSURE_ALLOCATION_ELSE(needed, oom_statement) \
    if ((needed) > allocated)                                                \
      {                                                                      \
        size_t memory_size;                                                  \
        DCHAR_T *memory;                                                     \
                                                                             \
        allocated = (allocated > 0 ? xtimes (allocated, 2) : 12);            \
        if ((needed) > allocated)                                            \
          allocated = (needed);                                              \
        memory_size = xtimes (allocated, sizeof (DCHAR_T));                  \
        if (size_overflow_p (memory_size))                                   \
          oom_statement                                                      \
        if (result == resultbuf)                                             \
          memory = (DCHAR_T *) malloc (memory_size);                         \
        else                                                                 \
          memory = (DCHAR_T *) realloc (result, memory_size);                \
        if (memory == NULL)                                                  \
          oom_statement                                                      \
        if (result == resultbuf && length > 0)                               \
          DCHAR_CPY (memory, result, length);                                \
        result = memory;                                                     \
      }
#define ENSURE_ALLOCATION(needed) \
  ENSURE_ALLOCATION_ELSE((needed), goto out_of_memory; )

    for (cp = format, i = 0, dp = &d.dir[0]; ; cp = dp->dir_end, i++, dp++)
      {
        if (cp != dp->dir_start)
          {
            size_t n = dp->dir_start - cp;
            size_t augmented_length = xsum (length, n);

            ENSURE_ALLOCATION (augmented_length);
             
            if (sizeof (FCHAR_T) == sizeof (DCHAR_T))
              {
                DCHAR_CPY (result + length, (const DCHAR_T *) cp, n);
                length = augmented_length;
              }
            else
              {
                do
                  result[length++] = *cp++;
                while (--n > 0);
              }
          }
        if (i == d.count)
          break;

         
        if (dp->conversion == '%')
          {
            size_t augmented_length;

            if (!(dp->arg_index == ARG_NONE))
              abort ();
            augmented_length = xsum (length, 1);
            ENSURE_ALLOCATION (augmented_length);
            result[length] = '%';
            length = augmented_length;
          }
        else
          {
            if (!(dp->arg_index != ARG_NONE))
              abort ();

            if (dp->conversion == 'n')
              {
                switch (a.arg[dp->arg_index].type)
                  {
                  case TYPE_COUNT_SCHAR_POINTER:
                    *a.arg[dp->arg_index].a.a_count_schar_pointer = length;
                    break;
                  case TYPE_COUNT_SHORT_POINTER:
                    *a.arg[dp->arg_index].a.a_count_short_pointer = length;
                    break;
                  case TYPE_COUNT_INT_POINTER:
                    *a.arg[dp->arg_index].a.a_count_int_pointer = length;
                    break;
                  case TYPE_COUNT_LONGINT_POINTER:
                    *a.arg[dp->arg_index].a.a_count_longint_pointer = length;
                    break;
                  case TYPE_COUNT_LONGLONGINT_POINTER:
                    *a.arg[dp->arg_index].a.a_count_longlongint_pointer = length;
                    break;
                  case TYPE_COUNT_INT8_T_POINTER:
                    *a.arg[dp->arg_index].a.a_count_int8_t_pointer = length;
                    break;
                  case TYPE_COUNT_INT16_T_POINTER:
                    *a.arg[dp->arg_index].a.a_count_int16_t_pointer = length;
                    break;
                  case TYPE_COUNT_INT32_T_POINTER:
                    *a.arg[dp->arg_index].a.a_count_int32_t_pointer = length;
                    break;
                  case TYPE_COUNT_INT64_T_POINTER:
                    *a.arg[dp->arg_index].a.a_count_int64_t_pointer = length;
                    break;
                  case TYPE_COUNT_INT_FAST8_T_POINTER:
                    *a.arg[dp->arg_index].a.a_count_int_fast8_t_pointer = length;
                    break;
                  case TYPE_COUNT_INT_FAST16_T_POINTER:
                    *a.arg[dp->arg_index].a.a_count_int_fast16_t_pointer = length;
                    break;
                  case TYPE_COUNT_INT_FAST32_T_POINTER:
                    *a.arg[dp->arg_index].a.a_count_int_fast32_t_pointer = length;
                    break;
                  case TYPE_COUNT_INT_FAST64_T_POINTER:
                    *a.arg[dp->arg_index].a.a_count_int_fast64_t_pointer = length;
                    break;
                  default:
                    abort ();
                  }
              }
#if ENABLE_UNISTDIO
             
            else if (dp->conversion == 'U')
              {
                arg_type type = a.arg[dp->arg_index].type;
                int flags = dp->flags;
                int has_width;
                size_t width;
                int has_precision;
                size_t precision;

                has_width = 0;
                width = 0;
                if (dp->width_start != dp->width_end)
                  {
                    if (dp->width_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->width_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->width_arg_index].a.a_int;
                        width = arg;
                        if (arg < 0)
                          {
                             
                            flags |= FLAG_LEFT;
                            width = -width;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->width_start;

                        do
                          width = xsum (xtimes (width, 10), *digitp++ - '0');
                        while (digitp != dp->width_end);
                      }
                    has_width = 1;
                  }

                has_precision = 0;
                precision = 0;
                if (dp->precision_start != dp->precision_end)
                  {
                    if (dp->precision_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->precision_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->precision_arg_index].a.a_int;
                         
                        if (arg >= 0)
                          {
                            precision = arg;
                            has_precision = 1;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->precision_start + 1;

                        precision = 0;
                        while (digitp != dp->precision_end)
                          precision = xsum (xtimes (precision, 10), *digitp++ - '0');
                        has_precision = 1;
                      }
                  }

                switch (type)
                  {
                  case TYPE_U8_STRING:
                    {
                      const uint8_t *arg = a.arg[dp->arg_index].a.a_u8_string;
                      const uint8_t *arg_end;
                      size_t characters;

                      if (has_precision)
                        {
                           
                          arg_end = arg;
                          characters = 0;
                          for (; precision > 0; precision--)
                            {
                              int count = u8_strmblen (arg_end);
                              if (count == 0)
                                break;
                              if (count < 0)
                                goto fail_with_EILSEQ;
                              arg_end += count;
                              characters++;
                            }
                        }
                      else if (has_width)
                        {
                           
                          arg_end = arg;
                          characters = 0;
                          for (;;)
                            {
                              int count = u8_strmblen (arg_end);
                              if (count == 0)
                                break;
                              if (count < 0)
                                goto fail_with_EILSEQ;
                              arg_end += count;
                              characters++;
                            }
                        }
                      else
                        {
                           
                          arg_end = arg + u8_strlen (arg);
                           
                          characters = 0;
                        }

                      if (characters < width && !(flags & FLAG_LEFT))
                        {
                          size_t n = width - characters;
                          ENSURE_ALLOCATION (xsum (length, n));
                          DCHAR_SET (result + length, ' ', n);
                          length += n;
                        }

# if DCHAR_IS_UINT8_T
                      {
                        size_t n = arg_end - arg;
                        ENSURE_ALLOCATION (xsum (length, n));
                        DCHAR_CPY (result + length, arg, n);
                        length += n;
                      }
# else
                      {  
                        DCHAR_T *converted = result + length;
                        size_t converted_len = allocated - length;
#  if DCHAR_IS_TCHAR
                         
                        converted =
                          u8_conv_to_encoding (locale_charset (),
                                               iconveh_question_mark,
                                               arg, arg_end - arg, NULL,
                                               converted, &converted_len);
#  else
                         
                        converted =
                          U8_TO_DCHAR (arg, arg_end - arg,
                                       converted, &converted_len);
#  endif
                        if (converted == NULL)
                          goto fail_with_errno;
                        if (converted != result + length)
                          {
                            ENSURE_ALLOCATION_ELSE (xsum (length, converted_len),
                                                    { free (converted); goto out_of_memory; });
                            DCHAR_CPY (result + length, converted, converted_len);
                            free (converted);
                          }
                        length += converted_len;
                      }
# endif

                      if (characters < width && (flags & FLAG_LEFT))
                        {
                          size_t n = width - characters;
                          ENSURE_ALLOCATION (xsum (length, n));
                          DCHAR_SET (result + length, ' ', n);
                          length += n;
                        }
                    }
                    break;

                  case TYPE_U16_STRING:
                    {
                      const uint16_t *arg = a.arg[dp->arg_index].a.a_u16_string;
                      const uint16_t *arg_end;
                      size_t characters;

                      if (has_precision)
                        {
                           
                          arg_end = arg;
                          characters = 0;
                          for (; precision > 0; precision--)
                            {
                              int count = u16_strmblen (arg_end);
                              if (count == 0)
                                break;
                              if (count < 0)
                                goto fail_with_EILSEQ;
                              arg_end += count;
                              characters++;
                            }
                        }
                      else if (has_width)
                        {
                           
                          arg_end = arg;
                          characters = 0;
                          for (;;)
                            {
                              int count = u16_strmblen (arg_end);
                              if (count == 0)
                                break;
                              if (count < 0)
                                goto fail_with_EILSEQ;
                              arg_end += count;
                              characters++;
                            }
                        }
                      else
                        {
                           
                          arg_end = arg + u16_strlen (arg);
                           
                          characters = 0;
                        }

                      if (characters < width && !(flags & FLAG_LEFT))
                        {
                          size_t n = width - characters;
                          ENSURE_ALLOCATION (xsum (length, n));
                          DCHAR_SET (result + length, ' ', n);
                          length += n;
                        }

# if DCHAR_IS_UINT16_T
                      {
                        size_t n = arg_end - arg;
                        ENSURE_ALLOCATION (xsum (length, n));
                        DCHAR_CPY (result + length, arg, n);
                        length += n;
                      }
# else
                      {  
                        DCHAR_T *converted = result + length;
                        size_t converted_len = allocated - length;
#  if DCHAR_IS_TCHAR
                         
                        converted =
                          u16_conv_to_encoding (locale_charset (),
                                                iconveh_question_mark,
                                                arg, arg_end - arg, NULL,
                                                converted, &converted_len);
#  else
                         
                        converted =
                          U16_TO_DCHAR (arg, arg_end - arg,
                                        converted, &converted_len);
#  endif
                        if (converted == NULL)
                          goto fail_with_errno;
                        if (converted != result + length)
                          {
                            ENSURE_ALLOCATION_ELSE (xsum (length, converted_len),
                                                    { free (converted); goto out_of_memory; });
                            DCHAR_CPY (result + length, converted, converted_len);
                            free (converted);
                          }
                        length += converted_len;
                      }
# endif

                      if (characters < width && (flags & FLAG_LEFT))
                        {
                          size_t n = width - characters;
                          ENSURE_ALLOCATION (xsum (length, n));
                          DCHAR_SET (result + length, ' ', n);
                          length += n;
                        }
                    }
                    break;

                  case TYPE_U32_STRING:
                    {
                      const uint32_t *arg = a.arg[dp->arg_index].a.a_u32_string;
                      const uint32_t *arg_end;
                      size_t characters;

                      if (has_precision)
                        {
                           
                          arg_end = arg;
                          characters = 0;
                          for (; precision > 0; precision--)
                            {
                              int count = u32_strmblen (arg_end);
                              if (count == 0)
                                break;
                              if (count < 0)
                                goto fail_with_EILSEQ;
                              arg_end += count;
                              characters++;
                            }
                        }
                      else if (has_width)
                        {
                           
                          arg_end = arg;
                          characters = 0;
                          for (;;)
                            {
                              int count = u32_strmblen (arg_end);
                              if (count == 0)
                                break;
                              if (count < 0)
                                goto fail_with_EILSEQ;
                              arg_end += count;
                              characters++;
                            }
                        }
                      else
                        {
                           
                          arg_end = arg + u32_strlen (arg);
                           
                          characters = 0;
                        }

                      if (characters < width && !(flags & FLAG_LEFT))
                        {
                          size_t n = width - characters;
                          ENSURE_ALLOCATION (xsum (length, n));
                          DCHAR_SET (result + length, ' ', n);
                          length += n;
                        }

# if DCHAR_IS_UINT32_T
                      {
                        size_t n = arg_end - arg;
                        ENSURE_ALLOCATION (xsum (length, n));
                        DCHAR_CPY (result + length, arg, n);
                        length += n;
                      }
# else
                      {  
                        DCHAR_T *converted = result + length;
                        size_t converted_len = allocated - length;
#  if DCHAR_IS_TCHAR
                         
                        converted =
                          u32_conv_to_encoding (locale_charset (),
                                                iconveh_question_mark,
                                                arg, arg_end - arg, NULL,
                                                converted, &converted_len);
#  else
                         
                        converted =
                          U32_TO_DCHAR (arg, arg_end - arg,
                                        converted, &converted_len);
#  endif
                        if (converted == NULL)
                          goto fail_with_errno;
                        if (converted != result + length)
                          {
                            ENSURE_ALLOCATION_ELSE (xsum (length, converted_len),
                                                    { free (converted); goto out_of_memory; });
                            DCHAR_CPY (result + length, converted, converted_len);
                            free (converted);
                          }
                        length += converted_len;
                      }
# endif

                      if (characters < width && (flags & FLAG_LEFT))
                        {
                          size_t n = width - characters;
                          ENSURE_ALLOCATION (xsum (length, n));
                          DCHAR_SET (result + length, ' ', n);
                          length += n;
                        }
                    }
                    break;

                  default:
                    abort ();
                  }
              }
#endif
#if WIDE_CHAR_VERSION && (!DCHAR_IS_TCHAR || NEED_WPRINTF_DIRECTIVE_LC)
            else if ((dp->conversion == 's'
                      && a.arg[dp->arg_index].type == TYPE_WIDE_STRING)
                     || (dp->conversion == 'c'
                         && a.arg[dp->arg_index].type == TYPE_WIDE_CHAR))
              {
                 
                 
                int flags = dp->flags;
                size_t width;

                width = 0;
                if (dp->width_start != dp->width_end)
                  {
                    if (dp->width_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->width_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->width_arg_index].a.a_int;
                        width = arg;
                        if (arg < 0)
                          {
                             
                            flags |= FLAG_LEFT;
                            width = -width;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->width_start;

                        do
                          width = xsum (xtimes (width, 10), *digitp++ - '0');
                        while (digitp != dp->width_end);
                      }
                  }

                {
                  const wchar_t *ls_arg;
                  wchar_t lc_arg[1];
                  size_t characters;

                  if (dp->conversion == 's')
                    {
                      int has_precision;
                      size_t precision;

                      has_precision = 0;
                      precision = 6;
                      if (dp->precision_start != dp->precision_end)
                        {
                          if (dp->precision_arg_index != ARG_NONE)
                            {
                              int arg;

                              if (!(a.arg[dp->precision_arg_index].type == TYPE_INT))
                                abort ();
                              arg = a.arg[dp->precision_arg_index].a.a_int;
                               
                              if (arg >= 0)
                                {
                                  precision = arg;
                                  has_precision = 1;
                                }
                            }
                          else
                            {
                              const FCHAR_T *digitp = dp->precision_start + 1;

                              precision = 0;
                              while (digitp != dp->precision_end)
                                precision = xsum (xtimes (precision, 10), *digitp++ - '0');
                              has_precision = 1;
                            }
                        }

                      ls_arg = a.arg[dp->arg_index].a.a_wide_string;

                      if (has_precision)
                        {
                           
                          const wchar_t *ls_arg_end;

                          ls_arg_end = ls_arg;
                          characters = 0;
                          for (; precision > 0; precision--)
                            {
                              if (*ls_arg_end == 0)
                                 
                                break;
                              ls_arg_end++;
                              characters++;
                            }
                        }
                      else
                        {
                           
                          characters = local_wcslen (ls_arg);
                        }
                    }
                  else  
                    {
                      lc_arg[0] = (wchar_t) a.arg[dp->arg_index].a.a_wide_char;
                      ls_arg = lc_arg;
                      characters = 1;
                    }

                  {
                    size_t total = (characters < width ? width : characters);
                    ENSURE_ALLOCATION (xsum (length, total));

                    if (characters < width && !(flags & FLAG_LEFT))
                      {
                        size_t n = width - characters;
                        DCHAR_SET (result + length, ' ', n);
                        length += n;
                      }

                    if (characters > 0)
                      {
                        DCHAR_CPY (result + length, ls_arg, characters);
                        length += characters;
                      }

                    if (characters < width && (flags & FLAG_LEFT))
                      {
                        size_t n = width - characters;
                        DCHAR_SET (result + length, ' ', n);
                        length += n;
                      }
                  }
                }
              }
#endif
#if (!USE_SNPRINTF || WIDE_CHAR_VERSION || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF || NEED_PRINTF_DIRECTIVE_LS || ENABLE_WCHAR_FALLBACK) && HAVE_WCHAR_T
            else if (dp->conversion == 's'
# if WIDE_CHAR_VERSION
                     && a.arg[dp->arg_index].type != TYPE_WIDE_STRING
# else
                     && a.arg[dp->arg_index].type == TYPE_WIDE_STRING
# endif
                    )
              {
                 
                int flags = dp->flags;
                int has_width;
                size_t width;
                int has_precision;
                size_t precision;

                has_width = 0;
                width = 0;
                if (dp->width_start != dp->width_end)
                  {
                    if (dp->width_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->width_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->width_arg_index].a.a_int;
                        width = arg;
                        if (arg < 0)
                          {
                             
                            flags |= FLAG_LEFT;
                            width = -width;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->width_start;

                        do
                          width = xsum (xtimes (width, 10), *digitp++ - '0');
                        while (digitp != dp->width_end);
                      }
                    has_width = 1;
                  }

                has_precision = 0;
                precision = 6;
                if (dp->precision_start != dp->precision_end)
                  {
                    if (dp->precision_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->precision_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->precision_arg_index].a.a_int;
                         
                        if (arg >= 0)
                          {
                            precision = arg;
                            has_precision = 1;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->precision_start + 1;

                        precision = 0;
                        while (digitp != dp->precision_end)
                          precision = xsum (xtimes (precision, 10), *digitp++ - '0');
                        has_precision = 1;
                      }
                  }

# if WIDE_CHAR_VERSION
                 
                {
                  const char *arg = a.arg[dp->arg_index].a.a_string;
                  const char *arg_end;
                  size_t characters;

                  if (has_precision)
                    {
                       
#  if HAVE_MBRTOWC
                      mbstate_t state;
                      mbszero (&state);
#  endif
                      arg_end = arg;
                      characters = 0;
                      for (; precision > 0; precision--)
                        {
                          int count;
#  if HAVE_MBRTOWC
                          count = mbrlen (arg_end, MB_CUR_MAX, &state);
#  else
                          count = mblen (arg_end, MB_CUR_MAX);
#  endif
                          if (count == 0)
                             
                            break;
                          if (count < 0)
                             
                            goto fail_with_EILSEQ;
                          arg_end += count;
                          characters++;
                        }
                    }
                  else if (has_width)
                    {
                       
#  if HAVE_MBRTOWC
                      mbstate_t state;
                      mbszero (&state);
#  endif
                      arg_end = arg;
                      characters = 0;
                      for (;;)
                        {
                          int count;
#  if HAVE_MBRTOWC
                          count = mbrlen (arg_end, MB_CUR_MAX, &state);
#  else
                          count = mblen (arg_end, MB_CUR_MAX);
#  endif
                          if (count == 0)
                             
                            break;
                          if (count < 0)
                             
                            goto fail_with_EILSEQ;
                          arg_end += count;
                          characters++;
                        }
                    }
                  else
                    {
                       
                      arg_end = arg + strlen (arg);
                       
                      characters = 0;
                    }

                  if (characters < width && !(flags & FLAG_LEFT))
                    {
                      size_t n = width - characters;
                      ENSURE_ALLOCATION (xsum (length, n));
                      DCHAR_SET (result + length, ' ', n);
                      length += n;
                    }

                  if (has_precision || has_width)
                    {
                       
                      size_t remaining;
#  if HAVE_MBRTOWC
                      mbstate_t state;
                      mbszero (&state);
#  endif
                      ENSURE_ALLOCATION (xsum (length, characters));
                      for (remaining = characters; remaining > 0; remaining--)
                        {
                          wchar_t wc;
                          int count;
#  if HAVE_MBRTOWC
                          count = mbrtowc (&wc, arg, arg_end - arg, &state);
#  else
                          count = mbtowc (&wc, arg, arg_end - arg);
#  endif
                          if (count <= 0)
                             
                            abort ();
                          result[length++] = wc;
                          arg += count;
                        }
                      if (!(arg == arg_end))
                        abort ();
                    }
                  else
                    {
#  if HAVE_MBRTOWC
                      mbstate_t state;
                      mbszero (&state);
#  endif
                      while (arg < arg_end)
                        {
                          wchar_t wc;
                          int count;
#  if HAVE_MBRTOWC
                          count = mbrtowc (&wc, arg, arg_end - arg, &state);
#  else
                          count = mbtowc (&wc, arg, arg_end - arg);
#  endif
                          if (count == 0)
                             
                            abort ();
                          if (count < 0)
                             
                            goto fail_with_EILSEQ;
                          ENSURE_ALLOCATION (xsum (length, 1));
                          result[length++] = wc;
                          arg += count;
                        }
                    }

                  if (characters < width && (flags & FLAG_LEFT))
                    {
                      size_t n = width - characters;
                      ENSURE_ALLOCATION (xsum (length, n));
                      DCHAR_SET (result + length, ' ', n);
                      length += n;
                    }
                }
# else
                 
                {
                  const wchar_t *arg = a.arg[dp->arg_index].a.a_wide_string;
                  const wchar_t *arg_end;
                  size_t characters;
#  if !DCHAR_IS_TCHAR
                   
                  static_assert (sizeof (TCHAR_T) == 1);
                  TCHAR_T *tmpsrc;
                  DCHAR_T *tmpdst;
                  size_t tmpdst_len;
#  endif
                  size_t w;

                  if (has_precision)
                    {
                       
#  if HAVE_WCRTOMB && !defined GNULIB_defined_mbstate_t
                      mbstate_t state;
                      mbszero (&state);
#  endif
                      arg_end = arg;
                      characters = 0;
                      while (precision > 0)
                        {
                          char cbuf[64];  
                          int count;

                          if (*arg_end == 0)
                             
                            break;
                          count = local_wcrtomb (cbuf, *arg_end, &state);
                          if (count < 0)
                             
                            goto fail_with_EILSEQ;
                          if (precision < (unsigned int) count)
                            break;
                          arg_end++;
                          characters += count;
                          precision -= count;
                        }
                    }
#  if DCHAR_IS_TCHAR
                  else if (has_width)
#  else
                  else
#  endif
                    {
                       
#  if HAVE_WCRTOMB && !defined GNULIB_defined_mbstate_t
                      mbstate_t state;
                      mbszero (&state);
#  endif
                      arg_end = arg;
                      characters = 0;
                      for (;;)
                        {
                          char cbuf[64];  
                          int count;

                          if (*arg_end == 0)
                             
                            break;
                          count = local_wcrtomb (cbuf, *arg_end, &state);
                          if (count < 0)
                             
                            goto fail_with_EILSEQ;
                          arg_end++;
                          characters += count;
                        }
                    }
#  if DCHAR_IS_TCHAR
                  else
                    {
                       
                      arg_end = arg + local_wcslen (arg);
                       
                      characters = 0;
                    }
#  endif

#  if !DCHAR_IS_TCHAR
                   
                  tmpsrc = (TCHAR_T *) malloc (characters * sizeof (TCHAR_T));
                  if (tmpsrc == NULL)
                    goto out_of_memory;
                  {
                    TCHAR_T *tmpptr = tmpsrc;
                    size_t remaining;
#   if HAVE_WCRTOMB && !defined GNULIB_defined_mbstate_t
                    mbstate_t state;
                    mbszero (&state);
#   endif
                    for (remaining = characters; remaining > 0; )
                      {
                        char cbuf[64];  
                        int count;

                        if (*arg == 0)
                          abort ();
                        count = local_wcrtomb (cbuf, *arg, &state);
                        if (count <= 0)
                           
                          abort ();
                        memcpy (tmpptr, cbuf, count);
                        tmpptr += count;
                        arg++;
                        remaining -= count;
                      }
                    if (!(arg == arg_end))
                      abort ();
                  }

                   
                  tmpdst =
                    DCHAR_CONV_FROM_ENCODING (locale_charset (),
                                              iconveh_question_mark,
                                              tmpsrc, characters,
                                              NULL,
                                              NULL, &tmpdst_len);
                  if (tmpdst == NULL)
                    {
                      free (tmpsrc);
                      goto fail_with_errno;
                    }
                  free (tmpsrc);
#  endif

                  if (has_width)
                    {
#  if ENABLE_UNISTDIO
                       
                      w = DCHAR_MBSNLEN (result + length, characters);
#  else
                       
                      w = characters;
#  endif
                    }
                  else
                     
                    w = 0;

                  if (w < width && !(flags & FLAG_LEFT))
                    {
                      size_t n = width - w;
                      ENSURE_ALLOCATION (xsum (length, n));
                      DCHAR_SET (result + length, ' ', n);
                      length += n;
                    }

#  if DCHAR_IS_TCHAR
                  if (has_precision || has_width)
                    {
                       
                      size_t remaining;
#   if HAVE_WCRTOMB && !defined GNULIB_defined_mbstate_t
                      mbstate_t state;
                      mbszero (&state);
#   endif
                      ENSURE_ALLOCATION (xsum (length, characters));
                      for (remaining = characters; remaining > 0; )
                        {
                          char cbuf[64];  
                          int count;

                          if (*arg == 0)
                            abort ();
                          count = local_wcrtomb (cbuf, *arg, &state);
                          if (count <= 0)
                             
                            abort ();
                          memcpy (result + length, cbuf, count);
                          length += count;
                          arg++;
                          remaining -= count;
                        }
                      if (!(arg == arg_end))
                        abort ();
                    }
                  else
                    {
#   if HAVE_WCRTOMB && !defined GNULIB_defined_mbstate_t
                      mbstate_t state;
                      mbszero (&state);
#   endif
                      while (arg < arg_end)
                        {
                          char cbuf[64];  
                          int count;

                          if (*arg == 0)
                            abort ();
                          count = local_wcrtomb (cbuf, *arg, &state);
                          if (count <= 0)
                             
                            goto fail_with_EILSEQ;
                          ENSURE_ALLOCATION (xsum (length, count));
                          memcpy (result + length, cbuf, count);
                          length += count;
                          arg++;
                        }
                    }
#  else
                  ENSURE_ALLOCATION_ELSE (xsum (length, tmpdst_len),
                                          { free (tmpdst); goto out_of_memory; });
                  DCHAR_CPY (result + length, tmpdst, tmpdst_len);
                  free (tmpdst);
                  length += tmpdst_len;
#  endif

                  if (w < width && (flags & FLAG_LEFT))
                    {
                      size_t n = width - w;
                      ENSURE_ALLOCATION (xsum (length, n));
                      DCHAR_SET (result + length, ' ', n);
                      length += n;
                    }
                }
# endif
              }
#endif
#if (NEED_PRINTF_DIRECTIVE_LC || ENABLE_WCHAR_FALLBACK) && HAVE_WINT_T && !WIDE_CHAR_VERSION
            else if (dp->conversion == 'c'
                     && a.arg[dp->arg_index].type == TYPE_WIDE_CHAR)
              {
                 
                int flags = dp->flags;
                int has_width;
                size_t width;

                has_width = 0;
                width = 0;
                if (dp->width_start != dp->width_end)
                  {
                    if (dp->width_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->width_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->width_arg_index].a.a_int;
                        width = arg;
                        if (arg < 0)
                          {
                             
                            flags |= FLAG_LEFT;
                            width = -width;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->width_start;

                        do
                          width = xsum (xtimes (width, 10), *digitp++ - '0');
                        while (digitp != dp->width_end);
                      }
                    has_width = 1;
                  }

                 
                {
                  wchar_t arg = (wchar_t) a.arg[dp->arg_index].a.a_wide_char;
                  size_t characters;
# if !DCHAR_IS_TCHAR
                   
                  static_assert (sizeof (TCHAR_T) == 1);
                  TCHAR_T tmpsrc[64];  
                  DCHAR_T *tmpdst;
                  size_t tmpdst_len;
# endif
                  size_t w;

# if DCHAR_IS_TCHAR
                  if (has_width)
# endif
                    {
                       
                      characters = 0;
                      if (arg != 0)
                        {
                          char cbuf[64];  
                          int count;
# if HAVE_WCRTOMB && !defined GNULIB_defined_mbstate_t
                          mbstate_t state;
                          mbszero (&state);
# endif

                          count = local_wcrtomb (cbuf, arg, &state);
                          if (count < 0)
                             
                            goto fail_with_EILSEQ;
                          characters = count;
                        }
                    }
# if DCHAR_IS_TCHAR
                  else
                    {
                       
                      characters = 0;
                    }
# endif

# if !DCHAR_IS_TCHAR
                   
                  if (characters > 0)  
                    {
                      char cbuf[64];  
                      int count;
#  if HAVE_WCRTOMB && !defined GNULIB_defined_mbstate_t
                      mbstate_t state;
                      mbszero (&state);
#  endif

                      count = local_wcrtomb (cbuf, arg, &state);
                      if (count <= 0)
                         
                        abort ();
                      memcpy (tmpsrc, cbuf, count);
                    }

                   
                  tmpdst =
                    DCHAR_CONV_FROM_ENCODING (locale_charset (),
                                              iconveh_question_mark,
                                              tmpsrc, characters,
                                              NULL,
                                              NULL, &tmpdst_len);
                  if (tmpdst == NULL)
                    goto fail_with_errno;
# endif

                  if (has_width)
                    {
# if ENABLE_UNISTDIO
                       
                      w = DCHAR_MBSNLEN (result + length, characters);
# else
                       
                      w = characters;
# endif
                    }
                  else
                     
                    w = 0;

                  if (w < width && !(flags & FLAG_LEFT))
                    {
                      size_t n = width - w;
                      ENSURE_ALLOCATION (xsum (length, n));
                      DCHAR_SET (result + length, ' ', n);
                      length += n;
                    }

# if DCHAR_IS_TCHAR
                  if (has_width)
                    {
                       
                      ENSURE_ALLOCATION (xsum (length, characters));
                      if (characters > 0)  
                        {
                          int count;
#  if HAVE_WCRTOMB && !defined GNULIB_defined_mbstate_t
                          mbstate_t state;
                          mbszero (&state);
#  endif

                          count = local_wcrtomb (result + length, arg, &state);
                          if (count <= 0)
                             
                            abort ();
                          length += count;
                        }
                    }
                  else
                    {
                      if (arg != 0)
                        {
                          char cbuf[64];  
                          int count;
#  if HAVE_WCRTOMB && !defined GNULIB_defined_mbstate_t
                          mbstate_t state;
                          mbszero (&state);
#  endif

                          count = local_wcrtomb (cbuf, arg, &state);
                          if (count < 0)
                             
                            goto fail_with_EILSEQ;
                          ENSURE_ALLOCATION (xsum (length, count));
                          memcpy (result + length, cbuf, count);
                          length += count;
                        }
                    }
# else
                  ENSURE_ALLOCATION_ELSE (xsum (length, tmpdst_len),
                                          { free (tmpdst); goto out_of_memory; });
                  DCHAR_CPY (result + length, tmpdst, tmpdst_len);
                  free (tmpdst);
                  length += tmpdst_len;
# endif

                  if (w < width && (flags & FLAG_LEFT))
                    {
                      size_t n = width - w;
                      ENSURE_ALLOCATION (xsum (length, n));
                      DCHAR_SET (result + length, ' ', n);
                      length += n;
                    }
                }
              }
#endif
#if NEED_WPRINTF_DIRECTIVE_C && WIDE_CHAR_VERSION
            else if (dp->conversion == 'c'
                     && a.arg[dp->arg_index].type != TYPE_WIDE_CHAR)
              {
                 
                int flags = dp->flags;
                size_t width;

                width = 0;
                if (dp->width_start != dp->width_end)
                  {
                    if (dp->width_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->width_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->width_arg_index].a.a_int;
                        width = arg;
                        if (arg < 0)
                          {
                             
                            flags |= FLAG_LEFT;
                            width = -width;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->width_start;

                        do
                          width = xsum (xtimes (width, 10), *digitp++ - '0');
                        while (digitp != dp->width_end);
                      }
                  }

                 
                {
                  char arg = (char) a.arg[dp->arg_index].a.a_char;
                  mbstate_t state;
                  wchar_t wc;

                  mbszero (&state);
                  int count = mbrtowc (&wc, &arg, 1, &state);
                  if (count < 0)
                     
                    goto fail_with_EILSEQ;

                  if (1 < width && !(flags & FLAG_LEFT))
                    {
                      size_t n = width - 1;
                      ENSURE_ALLOCATION (xsum (length, n));
                      DCHAR_SET (result + length, ' ', n);
                      length += n;
                    }

                  ENSURE_ALLOCATION (xsum (length, 1));
                  result[length++] = wc;

                  if (1 < width && (flags & FLAG_LEFT))
                    {
                      size_t n = width - 1;
                      ENSURE_ALLOCATION (xsum (length, n));
                      DCHAR_SET (result + length, ' ', n);
                      length += n;
                    }
                }
              }
#endif
#if NEED_PRINTF_DIRECTIVE_B || NEED_PRINTF_DIRECTIVE_UPPERCASE_B
            else if (0
# if NEED_PRINTF_DIRECTIVE_B
                     || (dp->conversion == 'b')
# endif
# if NEED_PRINTF_DIRECTIVE_UPPERCASE_B
                     || (dp->conversion == 'B')
# endif
                    )
              {
                arg_type type = a.arg[dp->arg_index].type;
                int flags = dp->flags;
                int has_width;
                size_t width;
                int has_precision;
                size_t precision;
                size_t tmp_length;
                size_t count;
                DCHAR_T tmpbuf[700];
                DCHAR_T *tmp;
                DCHAR_T *tmp_end;
                DCHAR_T *tmp_start;
                DCHAR_T *pad_ptr;
                DCHAR_T *p;

                has_width = 0;
                width = 0;
                if (dp->width_start != dp->width_end)
                  {
                    if (dp->width_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->width_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->width_arg_index].a.a_int;
                        width = arg;
                        if (arg < 0)
                          {
                             
                            flags |= FLAG_LEFT;
                            width = -width;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->width_start;

                        do
                          width = xsum (xtimes (width, 10), *digitp++ - '0');
                        while (digitp != dp->width_end);
                      }
                    has_width = 1;
                  }

                has_precision = 0;
                precision = 1;
                if (dp->precision_start != dp->precision_end)
                  {
                    if (dp->precision_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->precision_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->precision_arg_index].a.a_int;
                         
                        if (arg >= 0)
                          {
                            precision = arg;
                            has_precision = 1;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->precision_start + 1;

                        precision = 0;
                        while (digitp != dp->precision_end)
                          precision = xsum (xtimes (precision, 10), *digitp++ - '0');
                        has_precision = 1;
                      }
                  }

                 
                switch (type)
                  {
                  default:
                    tmp_length =
                      (unsigned int) (sizeof (unsigned int) * CHAR_BIT)
                      + 1;  
                    break;
                  case TYPE_ULONGINT:
                    tmp_length =
                      (unsigned int) (sizeof (unsigned long int) * CHAR_BIT)
                      + 1;  
                    break;
                  case TYPE_ULONGLONGINT:
                    tmp_length =
                      (unsigned int) (sizeof (unsigned long long int) * CHAR_BIT)
                      + 1;  
                    break;
                  case TYPE_UINT8_T:
                    tmp_length =
                      (unsigned int) (sizeof (uint8_t) * CHAR_BIT)
                      + 1;  
                    break;
                  case TYPE_UINT16_T:
                    tmp_length =
                      (unsigned int) (sizeof (uint16_t) * CHAR_BIT)
                      + 1;  
                    break;
                  case TYPE_UINT32_T:
                    tmp_length =
                      (unsigned int) (sizeof (uint32_t) * CHAR_BIT)
                      + 1;  
                    break;
                  case TYPE_UINT64_T:
                    tmp_length =
                      (unsigned int) (sizeof (uint64_t) * CHAR_BIT)
                      + 1;  
                    break;
                  case TYPE_UINT_FAST8_T:
                    tmp_length =
                      (unsigned int) (sizeof (uint_fast8_t) * CHAR_BIT)
                      + 1;  
                    break;
                  case TYPE_UINT_FAST16_T:
                    tmp_length =
                      (unsigned int) (sizeof (uint_fast16_t) * CHAR_BIT)
                      + 1;  
                    break;
                  case TYPE_UINT_FAST32_T:
                    tmp_length =
                      (unsigned int) (sizeof (uint_fast32_t) * CHAR_BIT)
                      + 1;  
                    break;
                  case TYPE_UINT_FAST64_T:
                    tmp_length =
                      (unsigned int) (sizeof (uint_fast64_t) * CHAR_BIT)
                      + 1;  
                    break;
                  }
                if (tmp_length < precision)
                  tmp_length = precision;
                 
                tmp_length = xsum (tmp_length, 2);

                if (tmp_length < width)
                  tmp_length = width;

                if (tmp_length <= sizeof (tmpbuf) / sizeof (DCHAR_T))
                  tmp = tmpbuf;
                else
                  {
                    size_t tmp_memsize = xtimes (tmp_length, sizeof (DCHAR_T));

                    if (size_overflow_p (tmp_memsize))
                       
                      goto out_of_memory;
                    tmp = (DCHAR_T *) malloc (tmp_memsize);
                    if (tmp == NULL)
                       
                      goto out_of_memory;
                  }

                tmp_end = tmp + tmp_length;

                unsigned long long arg;
                switch (type)
                  {
                  case TYPE_UCHAR:
                    arg = a.arg[dp->arg_index].a.a_uchar;
                    break;
                  case TYPE_USHORT:
                    arg = a.arg[dp->arg_index].a.a_ushort;
                    break;
                  case TYPE_UINT:
                    arg = a.arg[dp->arg_index].a.a_uint;
                    break;
                  case TYPE_ULONGINT:
                    arg = a.arg[dp->arg_index].a.a_ulongint;
                    break;
                  case TYPE_ULONGLONGINT:
                    arg = a.arg[dp->arg_index].a.a_ulonglongint;
                    break;
                  case TYPE_UINT8_T:
                    arg = a.arg[dp->arg_index].a.a_uint8_t;
                    break;
                  case TYPE_UINT16_T:
                    arg = a.arg[dp->arg_index].a.a_uint16_t;
                    break;
                  case TYPE_UINT32_T:
                    arg = a.arg[dp->arg_index].a.a_uint32_t;
                    break;
                  case TYPE_UINT64_T:
                    arg = a.arg[dp->arg_index].a.a_uint64_t;
                    break;
                  case TYPE_UINT_FAST8_T:
                    arg = a.arg[dp->arg_index].a.a_uint_fast8_t;
                    break;
                  case TYPE_UINT_FAST16_T:
                    arg = a.arg[dp->arg_index].a.a_uint_fast16_t;
                    break;
                  case TYPE_UINT_FAST32_T:
                    arg = a.arg[dp->arg_index].a.a_uint_fast32_t;
                    break;
                  case TYPE_UINT_FAST64_T:
                    arg = a.arg[dp->arg_index].a.a_uint_fast64_t;
                    break;
                  default:
                    abort ();
                  }
                int need_prefix = ((flags & FLAG_ALT) && arg != 0);

                p = tmp_end;
                 
                if (!(has_precision && precision == 0 && arg == 0))
                  {
                    do
                      {
                        *--p = '0' + (arg & 1);
                        arg = arg >> 1;
                      }
                    while (arg != 0);
                  }

                if (has_precision)
                  {
                    DCHAR_T *digits_start = tmp_end - precision;
                    while (p > digits_start)
                      *--p = '0';
                  }

                pad_ptr = p;

                if (need_prefix)
                  {
# if NEED_PRINTF_DIRECTIVE_B && !NEED_PRINTF_DIRECTIVE_UPPERCASE_B
                    *--p = 'b';
# elif NEED_PRINTF_DIRECTIVE_UPPERCASE_B && !NEED_PRINTF_DIRECTIVE_B
                    *--p = 'B';
# else
                    *--p = dp->conversion;
# endif
                    *--p = '0';
                  }
                tmp_start = p;

                 
                count = tmp_end - tmp_start;

                if (count < width)
                  {
                    size_t pad = width - count;

                    if (flags & FLAG_LEFT)
                      {
                         
                        for (p = tmp_start; p < tmp_end; p++)
                          *(p - pad) = *p;
                        for (p = tmp_end - pad; p < tmp_end; p++)
                          *p = ' ';
                      }
                    else if ((flags & FLAG_ZERO)
                              
                             && !(has_width && has_precision))
                      {
                         
                        for (p = tmp_start; p < pad_ptr; p++)
                          *(p - pad) = *p;
                        for (p = pad_ptr - pad; p < pad_ptr; p++)
                          *p = '0';
                      }
                    else
                      {
                         
                        for (p = tmp_start - pad; p < tmp_start; p++)
                          *p = ' ';
                      }

                    tmp_start = tmp_start - pad;
                  }

                count = tmp_end - tmp_start;

                if (count > tmp_length)
                   
                  abort ();

                 
                if (count >= allocated - length)
                  {
                    size_t n = xsum (length, count);

                    ENSURE_ALLOCATION (n);
                  }

                 
                memcpy (result + length, tmp_start, count * sizeof (DCHAR_T));
                if (tmp != tmpbuf)
                  free (tmp);
                length += count;
              }
#endif
#if NEED_PRINTF_DIRECTIVE_A || NEED_PRINTF_LONG_DOUBLE || NEED_PRINTF_DOUBLE || (NEED_WPRINTF_DIRECTIVE_LA && WIDE_CHAR_VERSION)
            else if ((dp->conversion == 'a' || dp->conversion == 'A')
# if !(NEED_PRINTF_DIRECTIVE_A || (NEED_PRINTF_LONG_DOUBLE && NEED_PRINTF_DOUBLE))
                     && (0
#  if NEED_PRINTF_DOUBLE
                         || a.arg[dp->arg_index].type == TYPE_DOUBLE
#  endif
#  if NEED_PRINTF_LONG_DOUBLE || (NEED_WPRINTF_DIRECTIVE_LA && WIDE_CHAR_VERSION)
                         || a.arg[dp->arg_index].type == TYPE_LONGDOUBLE
#  endif
                        )
# endif
                    )
              {
                arg_type type = a.arg[dp->arg_index].type;
                int flags = dp->flags;
                size_t width;
                int has_precision;
                size_t precision;
                size_t tmp_length;
                size_t count;
                DCHAR_T tmpbuf[700];
                DCHAR_T *tmp;
                DCHAR_T *pad_ptr;
                DCHAR_T *p;

                width = 0;
                if (dp->width_start != dp->width_end)
                  {
                    if (dp->width_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->width_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->width_arg_index].a.a_int;
                        width = arg;
                        if (arg < 0)
                          {
                             
                            flags |= FLAG_LEFT;
                            width = -width;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->width_start;

                        do
                          width = xsum (xtimes (width, 10), *digitp++ - '0');
                        while (digitp != dp->width_end);
                      }
                  }

                has_precision = 0;
                precision = 0;
                if (dp->precision_start != dp->precision_end)
                  {
                    if (dp->precision_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->precision_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->precision_arg_index].a.a_int;
                         
                        if (arg >= 0)
                          {
                            precision = arg;
                            has_precision = 1;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->precision_start + 1;

                        precision = 0;
                        while (digitp != dp->precision_end)
                          precision = xsum (xtimes (precision, 10), *digitp++ - '0');
                        has_precision = 1;
                      }
                  }

                 
                if (type == TYPE_LONGDOUBLE)
                  tmp_length =
                    (unsigned int) ((LDBL_DIG + 1)
                                    * 0.831  
                                   )
                    + 1;  
                else
                  tmp_length =
                    (unsigned int) ((DBL_DIG + 1)
                                    * 0.831  
                                   )
                    + 1;  
                if (tmp_length < precision)
                  tmp_length = precision;
                 
                tmp_length = xsum (tmp_length, 12);

                if (tmp_length < width)
                  tmp_length = width;

                tmp_length = xsum (tmp_length, 1);  

                if (tmp_length <= sizeof (tmpbuf) / sizeof (DCHAR_T))
                  tmp = tmpbuf;
                else
                  {
                    size_t tmp_memsize = xtimes (tmp_length, sizeof (DCHAR_T));

                    if (size_overflow_p (tmp_memsize))
                       
                      goto out_of_memory;
                    tmp = (DCHAR_T *) malloc (tmp_memsize);
                    if (tmp == NULL)
                       
                      goto out_of_memory;
                  }

                pad_ptr = NULL;
                p = tmp;
                if (type == TYPE_LONGDOUBLE)
                  {
# if NEED_PRINTF_DIRECTIVE_A || NEED_PRINTF_LONG_DOUBLE || (NEED_WPRINTF_DIRECTIVE_LA && WIDE_CHAR_VERSION)
                    long double arg = a.arg[dp->arg_index].a.a_longdouble;

                    if (isnanl (arg))
                      {
                        if (dp->conversion == 'A')
                          {
                            *p++ = 'N'; *p++ = 'A'; *p++ = 'N';
                          }
                        else
                          {
                            *p++ = 'n'; *p++ = 'a'; *p++ = 'n';
                          }
                      }
                    else
                      {
                        int sign = 0;
                        DECL_LONG_DOUBLE_ROUNDING

                        BEGIN_LONG_DOUBLE_ROUNDING ();

                        if (signbit (arg))  
                          {
                            sign = -1;
                            arg = -arg;
                          }

                        if (sign < 0)
                          *p++ = '-';
                        else if (flags & FLAG_SHOWSIGN)
                          *p++ = '+';
                        else if (flags & FLAG_SPACE)
                          *p++ = ' ';

                        if (arg > 0.0L && arg + arg == arg)
                          {
                            if (dp->conversion == 'A')
                              {
                                *p++ = 'I'; *p++ = 'N'; *p++ = 'F';
                              }
                            else
                              {
                                *p++ = 'i'; *p++ = 'n'; *p++ = 'f';
                              }
                          }
                        else
                          {
                            int exponent;
                            long double mantissa;

                            if (arg > 0.0L)
                              mantissa = printf_frexpl (arg, &exponent);
                            else
                              {
                                exponent = 0;
                                mantissa = 0.0L;
                              }

                            if (has_precision
                                && precision < (unsigned int) ((LDBL_DIG + 1) * 0.831) + 1)
                              {
                                 
                                long double tail = mantissa;
                                size_t q;

                                for (q = precision; ; q--)
                                  {
                                    int digit = (int) tail;
                                    tail -= digit;
                                    if (q == 0)
                                      {
                                        if (digit & 1 ? tail >= 0.5L : tail > 0.5L)
                                          tail = 1 - tail;
                                        else
                                          tail = - tail;
                                        break;
                                      }
                                    tail *= 16.0L;
                                  }
                                if (tail != 0.0L)
                                  for (q = precision; q > 0; q--)
                                    tail *= 0.0625L;
                                mantissa += tail;
                              }

                            *p++ = '0';
                            *p++ = dp->conversion - 'A' + 'X';
                            pad_ptr = p;
                            {
                              int digit;

                              digit = (int) mantissa;
                              mantissa -= digit;
                              *p++ = '0' + digit;
                              if ((flags & FLAG_ALT)
                                  || mantissa > 0.0L || precision > 0)
                                {
                                  *p++ = decimal_point_char ();
                                   
                                  while (mantissa > 0.0L)
                                    {
                                      mantissa *= 16.0L;
                                      digit = (int) mantissa;
                                      mantissa -= digit;
                                      *p++ = digit
                                             + (digit < 10
                                                ? '0'
                                                : dp->conversion - 10);
                                      if (precision > 0)
                                        precision--;
                                    }
                                  while (precision > 0)
                                    {
                                      *p++ = '0';
                                      precision--;
                                    }
                                }
                              }
                              *p++ = dp->conversion - 'A' + 'P';
#  if WIDE_CHAR_VERSION && DCHAR_IS_TCHAR
                              {
                                static const wchar_t decimal_format[] =
                                  { '%', '+', 'd', '\0' };
                                SNPRINTF (p, 6 + 1, decimal_format, exponent);
                              }
                              while (*p != '\0')
                                p++;
#  else
                              if (sizeof (DCHAR_T) == 1)
                                {
                                  sprintf ((char *) p, "%+d", exponent);
                                  while (*p != '\0')
                                    p++;
                                }
                              else
                                {
                                  char expbuf[6 + 1];
                                  const char *ep;
                                  sprintf (expbuf, "%+d", exponent);
                                  for (ep = expbuf; (*p = *ep) != '\0'; ep++)
                                    p++;
                                }
#  endif
                          }

                        END_LONG_DOUBLE_ROUNDING ();
                      }
# else
                    abort ();
# endif
                  }
                else
                  {
# if NEED_PRINTF_DIRECTIVE_A || NEED_PRINTF_DOUBLE
                    double arg = a.arg[dp->arg_index].a.a_double;

                    if (isnand (arg))
                      {
                        if (dp->conversion == 'A')
                          {
                            *p++ = 'N'; *p++ = 'A'; *p++ = 'N';
                          }
                        else
                          {
                            *p++ = 'n'; *p++ = 'a'; *p++ = 'n';
                          }
                      }
                    else
                      {
                        int sign = 0;

                        if (signbit (arg))  
                          {
                            sign = -1;
                            arg = -arg;
                          }

                        if (sign < 0)
                          *p++ = '-';
                        else if (flags & FLAG_SHOWSIGN)
                          *p++ = '+';
                        else if (flags & FLAG_SPACE)
                          *p++ = ' ';

                        if (arg > 0.0 && arg + arg == arg)
                          {
                            if (dp->conversion == 'A')
                              {
                                *p++ = 'I'; *p++ = 'N'; *p++ = 'F';
                              }
                            else
                              {
                                *p++ = 'i'; *p++ = 'n'; *p++ = 'f';
                              }
                          }
                        else
                          {
                            int exponent;
                            double mantissa;

                            if (arg > 0.0)
                              mantissa = printf_frexp (arg, &exponent);
                            else
                              {
                                exponent = 0;
                                mantissa = 0.0;
                              }

                            if (has_precision
                                && precision < (unsigned int) ((DBL_DIG + 1) * 0.831) + 1)
                              {
                                 
                                double tail = mantissa;
                                size_t q;

                                for (q = precision; ; q--)
                                  {
                                    int digit = (int) tail;
                                    tail -= digit;
                                    if (q == 0)
                                      {
                                        if (digit & 1 ? tail >= 0.5 : tail > 0.5)
                                          tail = 1 - tail;
                                        else
                                          tail = - tail;
                                        break;
                                      }
                                    tail *= 16.0;
                                  }
                                if (tail != 0.0)
                                  for (q = precision; q > 0; q--)
                                    tail *= 0.0625;
                                mantissa += tail;
                              }

                            *p++ = '0';
                            *p++ = dp->conversion - 'A' + 'X';
                            pad_ptr = p;
                            {
                              int digit;

                              digit = (int) mantissa;
                              mantissa -= digit;
                              *p++ = '0' + digit;
                              if ((flags & FLAG_ALT)
                                  || mantissa > 0.0 || precision > 0)
                                {
                                  *p++ = decimal_point_char ();
                                   
                                  while (mantissa > 0.0)
                                    {
                                      mantissa *= 16.0;
                                      digit = (int) mantissa;
                                      mantissa -= digit;
                                      *p++ = digit
                                             + (digit < 10
                                                ? '0'
                                                : dp->conversion - 10);
                                      if (precision > 0)
                                        precision--;
                                    }
                                  while (precision > 0)
                                    {
                                      *p++ = '0';
                                      precision--;
                                    }
                                }
                              }
                              *p++ = dp->conversion - 'A' + 'P';
#  if WIDE_CHAR_VERSION && DCHAR_IS_TCHAR
                              {
                                static const wchar_t decimal_format[] =
                                  { '%', '+', 'd', '\0' };
                                SNPRINTF (p, 6 + 1, decimal_format, exponent);
                              }
                              while (*p != '\0')
                                p++;
#  else
                              if (sizeof (DCHAR_T) == 1)
                                {
                                  sprintf ((char *) p, "%+d", exponent);
                                  while (*p != '\0')
                                    p++;
                                }
                              else
                                {
                                  char expbuf[6 + 1];
                                  const char *ep;
                                  sprintf (expbuf, "%+d", exponent);
                                  for (ep = expbuf; (*p = *ep) != '\0'; ep++)
                                    p++;
                                }
#  endif
                          }
                      }
# else
                    abort ();
# endif
                  }

                 
                count = p - tmp;

                if (count < width)
                  {
                    size_t pad = width - count;
                    DCHAR_T *end = p + pad;

                    if (flags & FLAG_LEFT)
                      {
                         
                        for (; pad > 0; pad--)
                          *p++ = ' ';
                      }
                    else if ((flags & FLAG_ZERO) && pad_ptr != NULL)
                      {
                         
                        DCHAR_T *q = end;

                        while (p > pad_ptr)
                          *--q = *--p;
                        for (; pad > 0; pad--)
                          *p++ = '0';
                      }
                    else
                      {
                         
                        DCHAR_T *q = end;

                        while (p > tmp)
                          *--q = *--p;
                        for (; pad > 0; pad--)
                          *p++ = ' ';
                      }

                    p = end;
                  }

                count = p - tmp;

                if (count >= tmp_length)
                   
                  abort ();

                 
                if (count >= allocated - length)
                  {
                    size_t n = xsum (length, count);

                    ENSURE_ALLOCATION (n);
                  }

                 
                memcpy (result + length, tmp, count * sizeof (DCHAR_T));
                if (tmp != tmpbuf)
                  free (tmp);
                length += count;
              }
#endif
#if NEED_PRINTF_INFINITE_DOUBLE || NEED_PRINTF_DOUBLE || NEED_PRINTF_INFINITE_LONG_DOUBLE || NEED_PRINTF_LONG_DOUBLE
            else if ((dp->conversion == 'f' || dp->conversion == 'F'
                      || dp->conversion == 'e' || dp->conversion == 'E'
                      || dp->conversion == 'g' || dp->conversion == 'G'
                      || dp->conversion == 'a' || dp->conversion == 'A')
                     && (0
# if NEED_PRINTF_DOUBLE
                         || a.arg[dp->arg_index].type == TYPE_DOUBLE
# elif NEED_PRINTF_INFINITE_DOUBLE
                         || (a.arg[dp->arg_index].type == TYPE_DOUBLE
                              
                             && is_infinite_or_zero (a.arg[dp->arg_index].a.a_double))
# endif
# if NEED_PRINTF_LONG_DOUBLE
                         || a.arg[dp->arg_index].type == TYPE_LONGDOUBLE
# elif NEED_PRINTF_INFINITE_LONG_DOUBLE
                         || (a.arg[dp->arg_index].type == TYPE_LONGDOUBLE
                              
                             && is_infinite_or_zerol (a.arg[dp->arg_index].a.a_longdouble))
# endif
                        ))
              {
# if (NEED_PRINTF_DOUBLE || NEED_PRINTF_INFINITE_DOUBLE) && (NEED_PRINTF_LONG_DOUBLE || NEED_PRINTF_INFINITE_LONG_DOUBLE)
                arg_type type = a.arg[dp->arg_index].type;
# endif
                int flags = dp->flags;
                size_t width;
                size_t count;
                int has_precision;
                size_t precision;
                size_t tmp_length;
                DCHAR_T tmpbuf[700];
                DCHAR_T *tmp;
                DCHAR_T *pad_ptr;
                DCHAR_T *p;

                width = 0;
                if (dp->width_start != dp->width_end)
                  {
                    if (dp->width_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->width_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->width_arg_index].a.a_int;
                        width = arg;
                        if (arg < 0)
                          {
                             
                            flags |= FLAG_LEFT;
                            width = -width;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->width_start;

                        do
                          width = xsum (xtimes (width, 10), *digitp++ - '0');
                        while (digitp != dp->width_end);
                      }
                  }

                has_precision = 0;
                precision = 0;
                if (dp->precision_start != dp->precision_end)
                  {
                    if (dp->precision_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->precision_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->precision_arg_index].a.a_int;
                         
                        if (arg >= 0)
                          {
                            precision = arg;
                            has_precision = 1;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->precision_start + 1;

                        precision = 0;
                        while (digitp != dp->precision_end)
                          precision = xsum (xtimes (precision, 10), *digitp++ - '0');
                        has_precision = 1;
                      }
                  }

                 
                if (!has_precision)
                  if (!(dp->conversion == 'a' || dp->conversion == 'A'))
                    precision = 6;

                 
# if NEED_PRINTF_DOUBLE && NEED_PRINTF_LONG_DOUBLE
                tmp_length = (type == TYPE_LONGDOUBLE ? LDBL_DIG + 1 : DBL_DIG + 1);
# elif NEED_PRINTF_INFINITE_DOUBLE && NEED_PRINTF_LONG_DOUBLE
                tmp_length = (type == TYPE_LONGDOUBLE ? LDBL_DIG + 1 : 0);
# elif NEED_PRINTF_LONG_DOUBLE
                tmp_length = LDBL_DIG + 1;
# elif NEED_PRINTF_DOUBLE
                tmp_length = DBL_DIG + 1;
# else
                tmp_length = 0;
# endif
                if (tmp_length < precision)
                  tmp_length = precision;
# if NEED_PRINTF_LONG_DOUBLE
#  if NEED_PRINTF_DOUBLE || NEED_PRINTF_INFINITE_DOUBLE
                if (type == TYPE_LONGDOUBLE)
#  endif
                  if (dp->conversion == 'f' || dp->conversion == 'F')
                    {
                      long double arg = a.arg[dp->arg_index].a.a_longdouble;
                      if (!(isnanl (arg) || arg + arg == arg))
                        {
                           
                          int exponent = floorlog10l (arg < 0 ? -arg : arg);
                          if (exponent >= 0 && tmp_length < exponent + precision)
                            tmp_length = exponent + precision;
                        }
                    }
# endif
# if NEED_PRINTF_DOUBLE
#  if NEED_PRINTF_LONG_DOUBLE || NEED_PRINTF_INFINITE_LONG_DOUBLE
                if (type == TYPE_DOUBLE)
#  endif
                  if (dp->conversion == 'f' || dp->conversion == 'F')
                    {
                      double arg = a.arg[dp->arg_index].a.a_double;
                      if (!(isnand (arg) || arg + arg == arg))
                        {
                           
                          int exponent = floorlog10 (arg < 0 ? -arg : arg);
                          if (exponent >= 0 && tmp_length < exponent + precision)
                            tmp_length = exponent + precision;
                        }
                    }
# endif
                 
                tmp_length = xsum (tmp_length, 12);

                if (tmp_length < width)
                  tmp_length = width;

                tmp_length = xsum (tmp_length, 1);  

                if (tmp_length <= sizeof (tmpbuf) / sizeof (DCHAR_T))
                  tmp = tmpbuf;
                else
                  {
                    size_t tmp_memsize = xtimes (tmp_length, sizeof (DCHAR_T));

                    if (size_overflow_p (tmp_memsize))
                       
                      goto out_of_memory;
                    tmp = (DCHAR_T *) malloc (tmp_memsize);
                    if (tmp == NULL)
                       
                      goto out_of_memory;
                  }

                pad_ptr = NULL;
                p = tmp;

# if NEED_PRINTF_LONG_DOUBLE || NEED_PRINTF_INFINITE_LONG_DOUBLE
#  if NEED_PRINTF_DOUBLE || NEED_PRINTF_INFINITE_DOUBLE
                if (type == TYPE_LONGDOUBLE)
#  endif
                  {
                    long double arg = a.arg[dp->arg_index].a.a_longdouble;

                    if (isnanl (arg))
                      {
                        if (dp->conversion >= 'A' && dp->conversion <= 'Z')
                          {
                            *p++ = 'N'; *p++ = 'A'; *p++ = 'N';
                          }
                        else
                          {
                            *p++ = 'n'; *p++ = 'a'; *p++ = 'n';
                          }
                      }
                    else
                      {
                        int sign = 0;
                        DECL_LONG_DOUBLE_ROUNDING

                        BEGIN_LONG_DOUBLE_ROUNDING ();

                        if (signbit (arg))  
                          {
                            sign = -1;
                            arg = -arg;
                          }

                        if (sign < 0)
                          *p++ = '-';
                        else if (flags & FLAG_SHOWSIGN)
                          *p++ = '+';
                        else if (flags & FLAG_SPACE)
                          *p++ = ' ';

                        if (arg > 0.0L && arg + arg == arg)
                          {
                            if (dp->conversion >= 'A' && dp->conversion <= 'Z')
                              {
                                *p++ = 'I'; *p++ = 'N'; *p++ = 'F';
                              }
                            else
                              {
                                *p++ = 'i'; *p++ = 'n'; *p++ = 'f';
                              }
                          }
                        else
                          {
#  if NEED_PRINTF_LONG_DOUBLE
                            pad_ptr = p;

                            if (dp->conversion == 'f' || dp->conversion == 'F')
                              {
                                char *digits;
                                size_t ndigits;

                                digits =
                                  scale10_round_decimal_long_double (arg, precision);
                                if (digits == NULL)
                                  {
                                    END_LONG_DOUBLE_ROUNDING ();
                                    goto out_of_memory;
                                  }
                                ndigits = strlen (digits);

                                if (ndigits > precision)
                                  do
                                    {
                                      --ndigits;
                                      *p++ = digits[ndigits];
                                    }
                                  while (ndigits > precision);
                                else
                                  *p++ = '0';
                                 
                                if ((flags & FLAG_ALT) || precision > 0)
                                  {
                                    *p++ = decimal_point_char ();
                                    for (; precision > ndigits; precision--)
                                      *p++ = '0';
                                    while (ndigits > 0)
                                      {
                                        --ndigits;
                                        *p++ = digits[ndigits];
                                      }
                                  }

                                free (digits);
                              }
                            else if (dp->conversion == 'e' || dp->conversion == 'E')
                              {
                                int exponent;

                                if (arg == 0.0L)
                                  {
                                    exponent = 0;
                                    *p++ = '0';
                                    if ((flags & FLAG_ALT) || precision > 0)
                                      {
                                        *p++ = decimal_point_char ();
                                        for (; precision > 0; precision--)
                                          *p++ = '0';
                                      }
                                  }
                                else
                                  {
                                     
                                    int adjusted;
                                    char *digits;
                                    size_t ndigits;

                                    exponent = floorlog10l (arg);
                                    adjusted = 0;
                                    for (;;)
                                      {
                                        digits =
                                          scale10_round_decimal_long_double (arg,
                                                                             (int)precision - exponent);
                                        if (digits == NULL)
                                          {
                                            END_LONG_DOUBLE_ROUNDING ();
                                            goto out_of_memory;
                                          }
                                        ndigits = strlen (digits);

                                        if (ndigits == precision + 1)
                                          break;
                                        if (ndigits < precision
                                            || ndigits > precision + 2)
                                           
                                          abort ();
                                        if (adjusted)
                                           
                                          abort ();
                                        free (digits);
                                        if (ndigits == precision)
                                          exponent -= 1;
                                        else
                                          exponent += 1;
                                        adjusted = 1;
                                      }
                                     
                                    if (is_borderline (digits, precision))
                                      {
                                         
                                        char *digits2 =
                                          scale10_round_decimal_long_double (arg,
                                                                             (int)precision - exponent + 1);
                                        if (digits2 == NULL)
                                          {
                                            free (digits);
                                            END_LONG_DOUBLE_ROUNDING ();
                                            goto out_of_memory;
                                          }
                                        if (strlen (digits2) == precision + 1)
                                          {
                                            free (digits);
                                            digits = digits2;
                                            exponent -= 1;
                                          }
                                        else
                                          free (digits2);
                                      }
                                     

                                    *p++ = digits[--ndigits];
                                    if ((flags & FLAG_ALT) || precision > 0)
                                      {
                                        *p++ = decimal_point_char ();
                                        while (ndigits > 0)
                                          {
                                            --ndigits;
                                            *p++ = digits[ndigits];
                                          }
                                      }

                                    free (digits);
                                  }

                                *p++ = dp->conversion;  
#   if WIDE_CHAR_VERSION && DCHAR_IS_TCHAR
                                {
                                  static const wchar_t decimal_format[] =
                                    { '%', '+', '.', '2', 'd', '\0' };
                                  SNPRINTF (p, 6 + 1, decimal_format, exponent);
                                }
                                while (*p != '\0')
                                  p++;
#   else
                                if (sizeof (DCHAR_T) == 1)
                                  {
                                    sprintf ((char *) p, "%+.2d", exponent);
                                    while (*p != '\0')
                                      p++;
                                  }
                                else
                                  {
                                    char expbuf[6 + 1];
                                    const char *ep;
                                    sprintf (expbuf, "%+.2d", exponent);
                                    for (ep = expbuf; (*p = *ep) != '\0'; ep++)
                                      p++;
                                  }
#   endif
                              }
                            else if (dp->conversion == 'g' || dp->conversion == 'G')
                              {
                                if (precision == 0)
                                  precision = 1;
                                 

                                if (arg == 0.0L)
                                   
                                  {
                                    size_t ndigits = precision;
                                     
                                    size_t nzeroes =
                                      (flags & FLAG_ALT ? 0 : precision - 1);

                                    --ndigits;
                                    *p++ = '0';
                                    if ((flags & FLAG_ALT) || ndigits > nzeroes)
                                      {
                                        *p++ = decimal_point_char ();
                                        while (ndigits > nzeroes)
                                          {
                                            --ndigits;
                                            *p++ = '0';
                                          }
                                      }
                                  }
                                else
                                  {
                                     
                                    int exponent;
                                    int adjusted;
                                    char *digits;
                                    size_t ndigits;
                                    size_t nzeroes;

                                    exponent = floorlog10l (arg);
                                    adjusted = 0;
                                    for (;;)
                                      {
                                        digits =
                                          scale10_round_decimal_long_double (arg,
                                                                             (int)(precision - 1) - exponent);
                                        if (digits == NULL)
                                          {
                                            END_LONG_DOUBLE_ROUNDING ();
                                            goto out_of_memory;
                                          }
                                        ndigits = strlen (digits);

                                        if (ndigits == precision)
                                          break;
                                        if (ndigits < precision - 1
                                            || ndigits > precision + 1)
                                           
                                          abort ();
                                        if (adjusted)
                                           
                                          abort ();
                                        free (digits);
                                        if (ndigits < precision)
                                          exponent -= 1;
                                        else
                                          exponent += 1;
                                        adjusted = 1;
                                      }
                                     
                                    if (is_borderline (digits, precision - 1))
                                      {
                                         
                                        char *digits2 =
                                          scale10_round_decimal_long_double (arg,
                                                                             (int)(precision - 1) - exponent + 1);
                                        if (digits2 == NULL)
                                          {
                                            free (digits);
                                            END_LONG_DOUBLE_ROUNDING ();
                                            goto out_of_memory;
                                          }
                                        if (strlen (digits2) == precision)
                                          {
                                            free (digits);
                                            digits = digits2;
                                            exponent -= 1;
                                          }
                                        else
                                          free (digits2);
                                      }
                                     

                                     
                                    nzeroes = 0;
                                    if ((flags & FLAG_ALT) == 0)
                                      while (nzeroes < ndigits
                                             && digits[nzeroes] == '0')
                                        nzeroes++;

                                     
                                    if (exponent >= -4
                                        && exponent < (long)precision)
                                      {
                                         
                                        if (exponent >= 0)
                                          {
                                            size_t ecount = exponent + 1;
                                             
                                            for (; ecount > 0; ecount--)
                                              *p++ = digits[--ndigits];
                                            if ((flags & FLAG_ALT) || ndigits > nzeroes)
                                              {
                                                *p++ = decimal_point_char ();
                                                while (ndigits > nzeroes)
                                                  {
                                                    --ndigits;
                                                    *p++ = digits[ndigits];
                                                  }
                                              }
                                          }
                                        else
                                          {
                                            size_t ecount = -exponent - 1;
                                            *p++ = '0';
                                            *p++ = decimal_point_char ();
                                            for (; ecount > 0; ecount--)
                                              *p++ = '0';
                                            while (ndigits > nzeroes)
                                              {
                                                --ndigits;
                                                *p++ = digits[ndigits];
                                              }
                                          }
                                      }
                                    else
                                      {
                                         
                                        *p++ = digits[--ndigits];
                                        if ((flags & FLAG_ALT) || ndigits > nzeroes)
                                          {
                                            *p++ = decimal_point_char ();
                                            while (ndigits > nzeroes)
                                              {
                                                --ndigits;
                                                *p++ = digits[ndigits];
                                              }
                                          }
                                        *p++ = dp->conversion - 'G' + 'E';  
#   if WIDE_CHAR_VERSION && DCHAR_IS_TCHAR
                                        {
                                          static const wchar_t decimal_format[] =
                                            { '%', '+', '.', '2', 'd', '\0' };
                                          SNPRINTF (p, 6 + 1, decimal_format, exponent);
                                        }
                                        while (*p != '\0')
                                          p++;
#   else
                                        if (sizeof (DCHAR_T) == 1)
                                          {
                                            sprintf ((char *) p, "%+.2d", exponent);
                                            while (*p != '\0')
                                              p++;
                                          }
                                        else
                                          {
                                            char expbuf[6 + 1];
                                            const char *ep;
                                            sprintf (expbuf, "%+.2d", exponent);
                                            for (ep = expbuf; (*p = *ep) != '\0'; ep++)
                                              p++;
                                          }
#   endif
                                      }

                                    free (digits);
                                  }
                              }
                            else
                              abort ();
#  else
                             
                            if (!(arg == 0.0L))
                              abort ();

                            pad_ptr = p;

                            if (dp->conversion == 'f' || dp->conversion == 'F')
                              {
                                *p++ = '0';
                                if ((flags & FLAG_ALT) || precision > 0)
                                  {
                                    *p++ = decimal_point_char ();
                                    for (; precision > 0; precision--)
                                      *p++ = '0';
                                  }
                              }
                            else if (dp->conversion == 'e' || dp->conversion == 'E')
                              {
                                *p++ = '0';
                                if ((flags & FLAG_ALT) || precision > 0)
                                  {
                                    *p++ = decimal_point_char ();
                                    for (; precision > 0; precision--)
                                      *p++ = '0';
                                  }
                                *p++ = dp->conversion;  
                                *p++ = '+';
                                *p++ = '0';
                                *p++ = '0';
                              }
                            else if (dp->conversion == 'g' || dp->conversion == 'G')
                              {
                                *p++ = '0';
                                if (flags & FLAG_ALT)
                                  {
                                    size_t ndigits =
                                      (precision > 0 ? precision - 1 : 0);
                                    *p++ = decimal_point_char ();
                                    for (; ndigits > 0; --ndigits)
                                      *p++ = '0';
                                  }
                              }
                            else if (dp->conversion == 'a' || dp->conversion == 'A')
                              {
                                *p++ = '0';
                                *p++ = dp->conversion - 'A' + 'X';
                                pad_ptr = p;
                                *p++ = '0';
                                if ((flags & FLAG_ALT) || precision > 0)
                                  {
                                    *p++ = decimal_point_char ();
                                    for (; precision > 0; precision--)
                                      *p++ = '0';
                                  }
                                *p++ = dp->conversion - 'A' + 'P';
                                *p++ = '+';
                                *p++ = '0';
                              }
                            else
                              abort ();
#  endif
                          }

                        END_LONG_DOUBLE_ROUNDING ();
                      }
                  }
#  if NEED_PRINTF_DOUBLE || NEED_PRINTF_INFINITE_DOUBLE
                else
#  endif
# endif
# if NEED_PRINTF_DOUBLE || NEED_PRINTF_INFINITE_DOUBLE
                  {
                    double arg = a.arg[dp->arg_index].a.a_double;

                    if (isnand (arg))
                      {
                        if (dp->conversion >= 'A' && dp->conversion <= 'Z')
                          {
                            *p++ = 'N'; *p++ = 'A'; *p++ = 'N';
                          }
                        else
                          {
                            *p++ = 'n'; *p++ = 'a'; *p++ = 'n';
                          }
                      }
                    else
                      {
                        int sign = 0;

                        if (signbit (arg))  
                          {
                            sign = -1;
                            arg = -arg;
                          }

                        if (sign < 0)
                          *p++ = '-';
                        else if (flags & FLAG_SHOWSIGN)
                          *p++ = '+';
                        else if (flags & FLAG_SPACE)
                          *p++ = ' ';

                        if (arg > 0.0 && arg + arg == arg)
                          {
                            if (dp->conversion >= 'A' && dp->conversion <= 'Z')
                              {
                                *p++ = 'I'; *p++ = 'N'; *p++ = 'F';
                              }
                            else
                              {
                                *p++ = 'i'; *p++ = 'n'; *p++ = 'f';
                              }
                          }
                        else
                          {
#  if NEED_PRINTF_DOUBLE
                            pad_ptr = p;

                            if (dp->conversion == 'f' || dp->conversion == 'F')
                              {
                                char *digits;
                                size_t ndigits;

                                digits =
                                  scale10_round_decimal_double (arg, precision);
                                if (digits == NULL)
                                  goto out_of_memory;
                                ndigits = strlen (digits);

                                if (ndigits > precision)
                                  do
                                    {
                                      --ndigits;
                                      *p++ = digits[ndigits];
                                    }
                                  while (ndigits > precision);
                                else
                                  *p++ = '0';
                                 
                                if ((flags & FLAG_ALT) || precision > 0)
                                  {
                                    *p++ = decimal_point_char ();
                                    for (; precision > ndigits; precision--)
                                      *p++ = '0';
                                    while (ndigits > 0)
                                      {
                                        --ndigits;
                                        *p++ = digits[ndigits];
                                      }
                                  }

                                free (digits);
                              }
                            else if (dp->conversion == 'e' || dp->conversion == 'E')
                              {
                                int exponent;

                                if (arg == 0.0)
                                  {
                                    exponent = 0;
                                    *p++ = '0';
                                    if ((flags & FLAG_ALT) || precision > 0)
                                      {
                                        *p++ = decimal_point_char ();
                                        for (; precision > 0; precision--)
                                          *p++ = '0';
                                      }
                                  }
                                else
                                  {
                                     
                                    int adjusted;
                                    char *digits;
                                    size_t ndigits;

                                    exponent = floorlog10 (arg);
                                    adjusted = 0;
                                    for (;;)
                                      {
                                        digits =
                                          scale10_round_decimal_double (arg,
                                                                        (int)precision - exponent);
                                        if (digits == NULL)
                                          goto out_of_memory;
                                        ndigits = strlen (digits);

                                        if (ndigits == precision + 1)
                                          break;
                                        if (ndigits < precision
                                            || ndigits > precision + 2)
                                           
                                          abort ();
                                        if (adjusted)
                                           
                                          abort ();
                                        free (digits);
                                        if (ndigits == precision)
                                          exponent -= 1;
                                        else
                                          exponent += 1;
                                        adjusted = 1;
                                      }
                                     
                                    if (is_borderline (digits, precision))
                                      {
                                         
                                        char *digits2 =
                                          scale10_round_decimal_double (arg,
                                                                        (int)precision - exponent + 1);
                                        if (digits2 == NULL)
                                          {
                                            free (digits);
                                            goto out_of_memory;
                                          }
                                        if (strlen (digits2) == precision + 1)
                                          {
                                            free (digits);
                                            digits = digits2;
                                            exponent -= 1;
                                          }
                                        else
                                          free (digits2);
                                      }
                                     

                                    *p++ = digits[--ndigits];
                                    if ((flags & FLAG_ALT) || precision > 0)
                                      {
                                        *p++ = decimal_point_char ();
                                        while (ndigits > 0)
                                          {
                                            --ndigits;
                                            *p++ = digits[ndigits];
                                          }
                                      }

                                    free (digits);
                                  }

                                *p++ = dp->conversion;  
#   if WIDE_CHAR_VERSION && DCHAR_IS_TCHAR
                                {
                                  static const wchar_t decimal_format[] =
                                     
#    if defined _WIN32 && ! defined __CYGWIN__
                                    { '%', '+', '.', '3', 'd', '\0' };
#    else
                                    { '%', '+', '.', '2', 'd', '\0' };
#    endif
                                  SNPRINTF (p, 6 + 1, decimal_format, exponent);
                                }
                                while (*p != '\0')
                                  p++;
#   else
                                {
                                  static const char decimal_format[] =
                                     
#    if defined _WIN32 && ! defined __CYGWIN__
                                    "%+.3d";
#    else
                                    "%+.2d";
#    endif
                                  if (sizeof (DCHAR_T) == 1)
                                    {
                                      sprintf ((char *) p, decimal_format, exponent);
                                      while (*p != '\0')
                                        p++;
                                    }
                                  else
                                    {
                                      char expbuf[6 + 1];
                                      const char *ep;
                                      sprintf (expbuf, decimal_format, exponent);
                                      for (ep = expbuf; (*p = *ep) != '\0'; ep++)
                                        p++;
                                    }
                                }
#   endif
                              }
                            else if (dp->conversion == 'g' || dp->conversion == 'G')
                              {
                                if (precision == 0)
                                  precision = 1;
                                 

                                if (arg == 0.0)
                                   
                                  {
                                    size_t ndigits = precision;
                                     
                                    size_t nzeroes =
                                      (flags & FLAG_ALT ? 0 : precision - 1);

                                    --ndigits;
                                    *p++ = '0';
                                    if ((flags & FLAG_ALT) || ndigits > nzeroes)
                                      {
                                        *p++ = decimal_point_char ();
                                        while (ndigits > nzeroes)
                                          {
                                            --ndigits;
                                            *p++ = '0';
                                          }
                                      }
                                  }
                                else
                                  {
                                     
                                    int exponent;
                                    int adjusted;
                                    char *digits;
                                    size_t ndigits;
                                    size_t nzeroes;

                                    exponent = floorlog10 (arg);
                                    adjusted = 0;
                                    for (;;)
                                      {
                                        digits =
                                          scale10_round_decimal_double (arg,
                                                                        (int)(precision - 1) - exponent);
                                        if (digits == NULL)
                                          goto out_of_memory;
                                        ndigits = strlen (digits);

                                        if (ndigits == precision)
                                          break;
                                        if (ndigits < precision - 1
                                            || ndigits > precision + 1)
                                           
                                          abort ();
                                        if (adjusted)
                                           
                                          abort ();
                                        free (digits);
                                        if (ndigits < precision)
                                          exponent -= 1;
                                        else
                                          exponent += 1;
                                        adjusted = 1;
                                      }
                                     
                                    if (is_borderline (digits, precision - 1))
                                      {
                                         
                                        char *digits2 =
                                          scale10_round_decimal_double (arg,
                                                                        (int)(precision - 1) - exponent + 1);
                                        if (digits2 == NULL)
                                          {
                                            free (digits);
                                            goto out_of_memory;
                                          }
                                        if (strlen (digits2) == precision)
                                          {
                                            free (digits);
                                            digits = digits2;
                                            exponent -= 1;
                                          }
                                        else
                                          free (digits2);
                                      }
                                     

                                     
                                    nzeroes = 0;
                                    if ((flags & FLAG_ALT) == 0)
                                      while (nzeroes < ndigits
                                             && digits[nzeroes] == '0')
                                        nzeroes++;

                                     
                                    if (exponent >= -4
                                        && exponent < (long)precision)
                                      {
                                         
                                        if (exponent >= 0)
                                          {
                                            size_t ecount = exponent + 1;
                                             
                                            for (; ecount > 0; ecount--)
                                              *p++ = digits[--ndigits];
                                            if ((flags & FLAG_ALT) || ndigits > nzeroes)
                                              {
                                                *p++ = decimal_point_char ();
                                                while (ndigits > nzeroes)
                                                  {
                                                    --ndigits;
                                                    *p++ = digits[ndigits];
                                                  }
                                              }
                                          }
                                        else
                                          {
                                            size_t ecount = -exponent - 1;
                                            *p++ = '0';
                                            *p++ = decimal_point_char ();
                                            for (; ecount > 0; ecount--)
                                              *p++ = '0';
                                            while (ndigits > nzeroes)
                                              {
                                                --ndigits;
                                                *p++ = digits[ndigits];
                                              }
                                          }
                                      }
                                    else
                                      {
                                         
                                        *p++ = digits[--ndigits];
                                        if ((flags & FLAG_ALT) || ndigits > nzeroes)
                                          {
                                            *p++ = decimal_point_char ();
                                            while (ndigits > nzeroes)
                                              {
                                                --ndigits;
                                                *p++ = digits[ndigits];
                                              }
                                          }
                                        *p++ = dp->conversion - 'G' + 'E';  
#   if WIDE_CHAR_VERSION && DCHAR_IS_TCHAR
                                        {
                                          static const wchar_t decimal_format[] =
                                             
#    if defined _WIN32 && ! defined __CYGWIN__
                                            { '%', '+', '.', '3', 'd', '\0' };
#    else
                                            { '%', '+', '.', '2', 'd', '\0' };
#    endif
                                          SNPRINTF (p, 6 + 1, decimal_format, exponent);
                                        }
                                        while (*p != '\0')
                                          p++;
#   else
                                        {
                                          static const char decimal_format[] =
                                             
#    if defined _WIN32 && ! defined __CYGWIN__
                                            "%+.3d";
#    else
                                            "%+.2d";
#    endif
                                          if (sizeof (DCHAR_T) == 1)
                                            {
                                              sprintf ((char *) p, decimal_format, exponent);
                                              while (*p != '\0')
                                                p++;
                                            }
                                          else
                                            {
                                              char expbuf[6 + 1];
                                              const char *ep;
                                              sprintf (expbuf, decimal_format, exponent);
                                              for (ep = expbuf; (*p = *ep) != '\0'; ep++)
                                                p++;
                                            }
                                        }
#   endif
                                      }

                                    free (digits);
                                  }
                              }
                            else
                              abort ();
#  else
                             
                            if (!(arg == 0.0))
                              abort ();

                            pad_ptr = p;

                            if (dp->conversion == 'f' || dp->conversion == 'F')
                              {
                                *p++ = '0';
                                if ((flags & FLAG_ALT) || precision > 0)
                                  {
                                    *p++ = decimal_point_char ();
                                    for (; precision > 0; precision--)
                                      *p++ = '0';
                                  }
                              }
                            else if (dp->conversion == 'e' || dp->conversion == 'E')
                              {
                                *p++ = '0';
                                if ((flags & FLAG_ALT) || precision > 0)
                                  {
                                    *p++ = decimal_point_char ();
                                    for (; precision > 0; precision--)
                                      *p++ = '0';
                                  }
                                *p++ = dp->conversion;  
                                *p++ = '+';
                                 
#   if defined _WIN32 && ! defined __CYGWIN__
                                *p++ = '0';
#   endif
                                *p++ = '0';
                                *p++ = '0';
                              }
                            else if (dp->conversion == 'g' || dp->conversion == 'G')
                              {
                                *p++ = '0';
                                if (flags & FLAG_ALT)
                                  {
                                    size_t ndigits =
                                      (precision > 0 ? precision - 1 : 0);
                                    *p++ = decimal_point_char ();
                                    for (; ndigits > 0; --ndigits)
                                      *p++ = '0';
                                  }
                              }
                            else
                              abort ();
#  endif
                          }
                      }
                  }
# endif

                 
                count = p - tmp;

                if (count < width)
                  {
                    size_t pad = width - count;
                    DCHAR_T *end = p + pad;

                    if (flags & FLAG_LEFT)
                      {
                         
                        for (; pad > 0; pad--)
                          *p++ = ' ';
                      }
                    else if ((flags & FLAG_ZERO) && pad_ptr != NULL)
                      {
                         
                        DCHAR_T *q = end;

                        while (p > pad_ptr)
                          *--q = *--p;
                        for (; pad > 0; pad--)
                          *p++ = '0';
                      }
                    else
                      {
                         
                        DCHAR_T *q = end;

                        while (p > tmp)
                          *--q = *--p;
                        for (; pad > 0; pad--)
                          *p++ = ' ';
                      }

                    p = end;
                  }

                count = p - tmp;

                if (count >= tmp_length)
                   
                  abort ();

                 
                if (count >= allocated - length)
                  {
                    size_t n = xsum (length, count);

                    ENSURE_ALLOCATION (n);
                  }

                 
                memcpy (result + length, tmp, count * sizeof (DCHAR_T));
                if (tmp != tmpbuf)
                  free (tmp);
                length += count;
              }
#endif
            else
              {
                arg_type type = a.arg[dp->arg_index].type;
                int flags = dp->flags;
#if (WIDE_CHAR_VERSION && MUSL_LIBC) || !DCHAR_IS_TCHAR || ENABLE_UNISTDIO || NEED_PRINTF_FLAG_LEFTADJUST || NEED_PRINTF_FLAG_ZERO || NEED_PRINTF_UNBOUNDED_PRECISION
                int has_width;
#endif
#if !USE_SNPRINTF || WIDE_CHAR_VERSION || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF || !DCHAR_IS_TCHAR || ENABLE_UNISTDIO || NEED_PRINTF_FLAG_LEFTADJUST || NEED_PRINTF_FLAG_ZERO || NEED_PRINTF_UNBOUNDED_PRECISION
                size_t width;
#endif
#if !USE_SNPRINTF || (WIDE_CHAR_VERSION && DCHAR_IS_TCHAR) || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF || (WIDE_CHAR_VERSION && MUSL_LIBC) || !DCHAR_IS_TCHAR || ENABLE_UNISTDIO || NEED_PRINTF_FLAG_LEFTADJUST || NEED_PRINTF_FLAG_ZERO || NEED_PRINTF_UNBOUNDED_PRECISION
                int has_precision;
                size_t precision;
#endif
#if NEED_PRINTF_UNBOUNDED_PRECISION
                int prec_ourselves;
#else
#               define prec_ourselves 0
#endif
#if (WIDE_CHAR_VERSION && MUSL_LIBC) || NEED_PRINTF_FLAG_LEFTADJUST
#               define pad_ourselves 1
#elif !DCHAR_IS_TCHAR || ENABLE_UNISTDIO || NEED_PRINTF_FLAG_ZERO || NEED_PRINTF_UNBOUNDED_PRECISION
                int pad_ourselves;
#else
#               define pad_ourselves 0
#endif
                TCHAR_T *fbp;
                unsigned int prefix_count;
                int prefixes[2] IF_LINT (= { 0 });
                int orig_errno;
#if !USE_SNPRINTF
                size_t tmp_length;
                TCHAR_T tmpbuf[700];
                TCHAR_T *tmp;
#endif

#if (WIDE_CHAR_VERSION && MUSL_LIBC) || !DCHAR_IS_TCHAR || ENABLE_UNISTDIO || NEED_PRINTF_FLAG_LEFTADJUST || NEED_PRINTF_FLAG_ZERO || NEED_PRINTF_UNBOUNDED_PRECISION
                has_width = 0;
#endif
#if !USE_SNPRINTF || WIDE_CHAR_VERSION || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF || !DCHAR_IS_TCHAR || ENABLE_UNISTDIO || NEED_PRINTF_FLAG_LEFTADJUST || NEED_PRINTF_FLAG_ZERO || NEED_PRINTF_UNBOUNDED_PRECISION
                width = 0;
                if (dp->width_start != dp->width_end)
                  {
                    if (dp->width_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->width_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->width_arg_index].a.a_int;
                        width = arg;
                        if (arg < 0)
                          {
                             
                            flags |= FLAG_LEFT;
                            width = -width;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->width_start;

                        do
                          width = xsum (xtimes (width, 10), *digitp++ - '0');
                        while (digitp != dp->width_end);
                      }
# if (WIDE_CHAR_VERSION && MUSL_LIBC) || !DCHAR_IS_TCHAR || ENABLE_UNISTDIO || NEED_PRINTF_FLAG_LEFTADJUST || NEED_PRINTF_FLAG_ZERO || NEED_PRINTF_UNBOUNDED_PRECISION
                    has_width = 1;
# endif
                  }
#endif

#if !USE_SNPRINTF || (WIDE_CHAR_VERSION && DCHAR_IS_TCHAR) || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF || (WIDE_CHAR_VERSION && MUSL_LIBC) || !DCHAR_IS_TCHAR || ENABLE_UNISTDIO || NEED_PRINTF_FLAG_LEFTADJUST || NEED_PRINTF_FLAG_ZERO || NEED_PRINTF_UNBOUNDED_PRECISION
                has_precision = 0;
                precision = 6;
                if (dp->precision_start != dp->precision_end)
                  {
                    if (dp->precision_arg_index != ARG_NONE)
                      {
                        int arg;

                        if (!(a.arg[dp->precision_arg_index].type == TYPE_INT))
                          abort ();
                        arg = a.arg[dp->precision_arg_index].a.a_int;
                         
                        if (arg >= 0)
                          {
                            precision = arg;
                            has_precision = 1;
                          }
                      }
                    else
                      {
                        const FCHAR_T *digitp = dp->precision_start + 1;

                        precision = 0;
                        while (digitp != dp->precision_end)
                          precision = xsum (xtimes (precision, 10), *digitp++ - '0');
                        has_precision = 1;
                      }
                  }
#endif

                 
#if NEED_PRINTF_UNBOUNDED_PRECISION
                switch (dp->conversion)
                  {
                  case 'd': case 'i': case 'u':
                  case 'b':
                  #if SUPPORT_GNU_PRINTF_DIRECTIVES \
                      || (__GLIBC__ + (__GLIBC_MINOR__ >= 35) > 2)
                  case 'B':
                  #endif
                  case 'o':
                  case 'x': case 'X': case 'p':
                    prec_ourselves = has_precision && (precision > 0);
                    break;
                  default:
                    prec_ourselves = 0;
                    break;
                  }
#endif

                 
#if !((WIDE_CHAR_VERSION && MUSL_LIBC) || NEED_PRINTF_FLAG_LEFTADJUST) && (!DCHAR_IS_TCHAR || ENABLE_UNISTDIO || NEED_PRINTF_FLAG_ZERO || NEED_PRINTF_UNBOUNDED_PRECISION)
                switch (dp->conversion)
                  {
# if !DCHAR_IS_TCHAR || ENABLE_UNISTDIO
                   
                  case 'c': case 's':
# endif
# if NEED_PRINTF_FLAG_ZERO
                  case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
                  case 'a': case 'A':
# endif
                    pad_ourselves = 1;
                    break;
                  default:
                    pad_ourselves = prec_ourselves;
                    break;
                  }
#endif

#if !USE_SNPRINTF
                 
                tmp_length =
                  MAX_ROOM_NEEDED (&a, dp->arg_index, dp->conversion, type,
                                   flags, width, has_precision, precision,
                                   pad_ourselves);

                if (tmp_length <= sizeof (tmpbuf) / sizeof (TCHAR_T))
                  tmp = tmpbuf;
                else
                  {
                    size_t tmp_memsize = xtimes (tmp_length, sizeof (TCHAR_T));

                    if (size_overflow_p (tmp_memsize))
                       
                      goto out_of_memory;
                    tmp = (TCHAR_T *) malloc (tmp_memsize);
                    if (tmp == NULL)
                       
                      goto out_of_memory;
                  }
#endif

                 
                fbp = buf;
                *fbp++ = '%';
#if NEED_PRINTF_FLAG_GROUPING
                 
#else
                if (flags & FLAG_GROUP)
                  *fbp++ = '\'';
#endif
                if (flags & FLAG_LEFT)
                  *fbp++ = '-';
                if (flags & FLAG_SHOWSIGN)
                  *fbp++ = '+';
                if (flags & FLAG_SPACE)
                  *fbp++ = ' ';
                if (flags & FLAG_ALT)
                  *fbp++ = '#';
#if __GLIBC__ >= 2 && !defined __UCLIBC__
                if (flags & FLAG_LOCALIZED)
                  *fbp++ = 'I';
#endif
                if (!pad_ourselves)
                  {
                    if (flags & FLAG_ZERO)
                      *fbp++ = '0';
                    if (dp->width_start != dp->width_end)
                      {
                        size_t n = dp->width_end - dp->width_start;
                         
                        if (sizeof (FCHAR_T) == sizeof (TCHAR_T))
                          {
                            memcpy (fbp, dp->width_start, n * sizeof (TCHAR_T));
                            fbp += n;
                          }
                        else
                          {
                            const FCHAR_T *mp = dp->width_start;
                            do
                              *fbp++ = *mp++;
                            while (--n > 0);
                          }
                      }
                  }
                if (!prec_ourselves)
                  {
                    if (dp->precision_start != dp->precision_end)
                      {
                        size_t n = dp->precision_end - dp->precision_start;
                         
                        if (sizeof (FCHAR_T) == sizeof (TCHAR_T))
                          {
                            memcpy (fbp, dp->precision_start, n * sizeof (TCHAR_T));
                            fbp += n;
                          }
                        else
                          {
                            const FCHAR_T *mp = dp->precision_start;
                            do
                              *fbp++ = *mp++;
                            while (--n > 0);
                          }
                      }
                  }

                switch (type)
                  {
                  case TYPE_LONGLONGINT:
                  case TYPE_ULONGLONGINT:
                  #if INT8_WIDTH > LONG_WIDTH
                  case TYPE_INT8_T:
                  #endif
                  #if UINT8_WIDTH > LONG_WIDTH
                  case TYPE_UINT8_T:
                  #endif
                  #if INT16_WIDTH > LONG_WIDTH
                  case TYPE_INT16_T:
                  #endif
                  #if UINT16_WIDTH > LONG_WIDTH
                  case TYPE_UINT16_T:
                  #endif
                  #if INT32_WIDTH > LONG_WIDTH
                  case TYPE_INT32_T:
                  #endif
                  #if UINT32_WIDTH > LONG_WIDTH
                  case TYPE_UINT32_T:
                  #endif
                  #if INT64_WIDTH > LONG_WIDTH
                  case TYPE_INT64_T:
                  #endif
                  #if UINT64_WIDTH > LONG_WIDTH
                  case TYPE_UINT64_T:
                  #endif
                  #if INT_FAST8_WIDTH > LONG_WIDTH
                  case TYPE_INT_FAST8_T:
                  #endif
                  #if UINT_FAST8_WIDTH > LONG_WIDTH
                  case TYPE_UINT_FAST8_T:
                  #endif
                  #if INT_FAST16_WIDTH > LONG_WIDTH
                  case TYPE_INT_FAST16_T:
                  #endif
                  #if UINT_FAST16_WIDTH > LONG_WIDTH
                  case TYPE_UINT_FAST16_T:
                  #endif
                  #if INT_FAST32_WIDTH > LONG_WIDTH
                  case TYPE_INT3_FAST2_T:
                  #endif
                  #if UINT_FAST32_WIDTH > LONG_WIDTH
                  case TYPE_UINT_FAST32_T:
                  #endif
                  #if INT_FAST64_WIDTH > LONG_WIDTH
                  case TYPE_INT_FAST64_T:
                  #endif
                  #if UINT_FAST64_WIDTH > LONG_WIDTH
                  case TYPE_UINT_FAST64_T:
                  #endif
#if defined _WIN32 && ! defined __CYGWIN__
                    *fbp++ = 'I';
                    *fbp++ = '6';
                    *fbp++ = '4';
                    break;
#else
                    *fbp++ = 'l';
#endif
                    FALLTHROUGH;
                  case TYPE_LONGINT:
                  case TYPE_ULONGINT:
                  #if INT8_WIDTH > INT_WIDTH && INT8_WIDTH <= LONG_WIDTH
                  case TYPE_INT8_T:
                  #endif
                  #if UINT8_WIDTH > INT_WIDTH && UINT8_WIDTH <= LONG_WIDTH
                  case TYPE_UINT8_T:
                  #endif
                  #if INT16_WIDTH > INT_WIDTH && INT16_WIDTH <= LONG_WIDTH
                  case TYPE_INT16_T:
                  #endif
                  #if UINT16_WIDTH > INT_WIDTH && UINT16_WIDTH <= LONG_WIDTH
                  case TYPE_UINT16_T:
                  #endif
                  #if INT32_WIDTH > INT_WIDTH && INT32_WIDTH <= LONG_WIDTH
                  case TYPE_INT32_T:
                  #endif
                  #if UINT32_WIDTH > INT_WIDTH && UINT32_WIDTH <= LONG_WIDTH
                  case TYPE_UINT32_T:
                  #endif
                  #if INT64_WIDTH > INT_WIDTH && INT64_WIDTH <= LONG_WIDTH
                  case TYPE_INT64_T:
                  #endif
                  #if UINT64_WIDTH > INT_WIDTH && UINT64_WIDTH <= LONG_WIDTH
                  case TYPE_UINT64_T:
                  #endif
                  #if INT_FAST8_WIDTH > INT_WIDTH && INT_FAST8_WIDTH <= LONG_WIDTH
                  case TYPE_INT_FAST8_T:
                  #endif
                  #if UINT_FAST8_WIDTH > INT_WIDTH && UINT_FAST8_WIDTH <= LONG_WIDTH
                  case TYPE_UINT_FAST8_T:
                  #endif
                  #if INT_FAST16_WIDTH > INT_WIDTH && INT_FAST16_WIDTH <= LONG_WIDTH
                  case TYPE_INT_FAST16_T:
                  #endif
                  #if UINT_FAST16_WIDTH > INT_WIDTH && UINT_FAST16_WIDTH <= LONG_WIDTH
                  case TYPE_UINT_FAST16_T:
                  #endif
                  #if INT_FAST32_WIDTH > INT_WIDTH && INT_FAST32_WIDTH <= LONG_WIDTH
                  case TYPE_INT_FAST32_T:
                  #endif
                  #if UINT_FAST32_WIDTH > INT_WIDTH && UINT_FAST32_WIDTH <= LONG_WIDTH
                  case TYPE_UINT_FAST32_T:
                  #endif
                  #if INT_FAST64_WIDTH > INT_WIDTH && INT_FAST64_WIDTH <= LONG_WIDTH
                  case TYPE_INT_FAST64_T:
                  #endif
                  #if UINT_FAST64_WIDTH > INT_WIDTH && UINT_FAST64_WIDTH <= LONG_WIDTH
                  case TYPE_UINT_FAST64_T:
                  #endif
                  #if HAVE_WINT_T
                  case TYPE_WIDE_CHAR:
                  #endif
                  #if HAVE_WCHAR_T
                  case TYPE_WIDE_STRING:
                  #endif
                    *fbp++ = 'l';
                    break;
                  case TYPE_LONGDOUBLE:
                    *fbp++ = 'L';
                    break;
                  default:
                    break;
                  }
#if NEED_PRINTF_DIRECTIVE_F
                if (dp->conversion == 'F')
                  *fbp = 'f';
                else
#endif
                  *fbp = dp->conversion;
#if USE_SNPRINTF
                 
# if (((!WIDE_CHAR_VERSION || !DCHAR_IS_TCHAR)                              \
       && (HAVE_SNPRINTF_RETVAL_C99 && HAVE_SNPRINTF_TRUNCATION_C99))       \
      || ((__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3))       \
          && !defined __UCLIBC__)                                           \
      || (defined __APPLE__ && defined __MACH__)                            \
      || defined __OpenBSD__                                                \
      || defined __ANDROID__                                                \
      || (defined _WIN32 && ! defined __CYGWIN__))                          \
      || (WIDE_CHAR_VERSION && MUSL_LIBC)
                 
                 
                 
                 
                fbp[1] = '\0';
# else            
                fbp[1] = '%';
                fbp[2] = 'n';
                fbp[3] = '\0';
# endif
#else
                fbp[1] = '\0';
#endif

                 
                prefix_count = 0;
                if (!pad_ourselves && dp->width_arg_index != ARG_NONE)
                  {
                    if (!(a.arg[dp->width_arg_index].type == TYPE_INT))
                      abort ();
                    prefixes[prefix_count++] = a.arg[dp->width_arg_index].a.a_int;
                  }
                if (!prec_ourselves && dp->precision_arg_index != ARG_NONE)
                  {
                    if (!(a.arg[dp->precision_arg_index].type == TYPE_INT))
                      abort ();
                    prefixes[prefix_count++] = a.arg[dp->precision_arg_index].a.a_int;
                  }

#if USE_SNPRINTF
                 
# define TCHARS_PER_DCHAR (sizeof (DCHAR_T) / sizeof (TCHAR_T))
                 
                ENSURE_ALLOCATION (xsum (length,
                                         (2 + TCHARS_PER_DCHAR - 1)
                                         / TCHARS_PER_DCHAR));
                 
                *(TCHAR_T *) (result + length) = '\0';
#endif

                orig_errno = errno;

                for (;;)
                  {
                    int count = -1;

#if USE_SNPRINTF
                    int retcount = 0;
                    size_t maxlen = allocated - length;
                     
                    if (maxlen > INT_MAX / TCHARS_PER_DCHAR)
                      maxlen = INT_MAX / TCHARS_PER_DCHAR;
                    maxlen = maxlen * TCHARS_PER_DCHAR;
# define SNPRINTF_BUF(arg) \
                    switch (prefix_count)                                   \
                      {                                                     \
                      case 0:                                               \
                        retcount = SNPRINTF ((TCHAR_T *) (result + length), \
                                             maxlen, buf,                   \
                                             arg, &count);                  \
                        break;                                              \
                      case 1:                                               \
                        retcount = SNPRINTF ((TCHAR_T *) (result + length), \
                                             maxlen, buf,                   \
                                             prefixes[0], arg, &count);     \
                        break;                                              \
                      case 2:                                               \
                        retcount = SNPRINTF ((TCHAR_T *) (result + length), \
                                             maxlen, buf,                   \
                                             prefixes[0], prefixes[1], arg, \
                                             &count);                       \
                        break;                                              \
                      default:                                              \
                        abort ();                                           \
                      }
#else
# define SNPRINTF_BUF(arg) \
                    switch (prefix_count)                                   \
                      {                                                     \
                      case 0:                                               \
                        count = sprintf (tmp, buf, arg);                    \
                        break;                                              \
                      case 1:                                               \
                        count = sprintf (tmp, buf, prefixes[0], arg);       \
                        break;                                              \
                      case 2:                                               \
                        count = sprintf (tmp, buf, prefixes[0], prefixes[1],\
                                         arg);                              \
                        break;                                              \
                      default:                                              \
                        abort ();                                           \
                      }
#endif

                    errno = 0;
                    switch (type)
                      {
                      case TYPE_SCHAR:
                        {
                          int arg = a.arg[dp->arg_index].a.a_schar;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_UCHAR:
                        {
                          unsigned int arg = a.arg[dp->arg_index].a.a_uchar;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_SHORT:
                        {
                          int arg = a.arg[dp->arg_index].a.a_short;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_USHORT:
                        {
                          unsigned int arg = a.arg[dp->arg_index].a.a_ushort;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_INT:
                        {
                          int arg = a.arg[dp->arg_index].a.a_int;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_UINT:
                        {
                          unsigned int arg = a.arg[dp->arg_index].a.a_uint;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_LONGINT:
                        {
                          long int arg = a.arg[dp->arg_index].a.a_longint;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_ULONGINT:
                        {
                          unsigned long int arg = a.arg[dp->arg_index].a.a_ulongint;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_LONGLONGINT:
                        {
                          long long int arg = a.arg[dp->arg_index].a.a_longlongint;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_ULONGLONGINT:
                        {
                          unsigned long long int arg = a.arg[dp->arg_index].a.a_ulonglongint;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_INT8_T:
                        {
                          int8_t arg = a.arg[dp->arg_index].a.a_int8_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_UINT8_T:
                        {
                          uint8_t arg = a.arg[dp->arg_index].a.a_uint8_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_INT16_T:
                        {
                          int16_t arg = a.arg[dp->arg_index].a.a_int16_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_UINT16_T:
                        {
                          uint16_t arg = a.arg[dp->arg_index].a.a_uint16_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_INT32_T:
                        {
                          int32_t arg = a.arg[dp->arg_index].a.a_int32_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_UINT32_T:
                        {
                          uint32_t arg = a.arg[dp->arg_index].a.a_uint32_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_INT64_T:
                        {
                          int64_t arg = a.arg[dp->arg_index].a.a_int64_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_UINT64_T:
                        {
                          uint64_t arg = a.arg[dp->arg_index].a.a_uint64_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_INT_FAST8_T:
                        {
                          int_fast8_t arg = a.arg[dp->arg_index].a.a_int_fast8_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_UINT_FAST8_T:
                        {
                          uint_fast8_t arg = a.arg[dp->arg_index].a.a_uint_fast8_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_INT_FAST16_T:
                        {
                          int_fast16_t arg = a.arg[dp->arg_index].a.a_int_fast16_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_UINT_FAST16_T:
                        {
                          uint_fast16_t arg = a.arg[dp->arg_index].a.a_uint_fast16_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_INT_FAST32_T:
                        {
                          int_fast32_t arg = a.arg[dp->arg_index].a.a_int_fast32_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_UINT_FAST32_T:
                        {
                          uint_fast32_t arg = a.arg[dp->arg_index].a.a_uint_fast32_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_INT_FAST64_T:
                        {
                          int_fast64_t arg = a.arg[dp->arg_index].a.a_int_fast64_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_UINT_FAST64_T:
                        {
                          uint_fast64_t arg = a.arg[dp->arg_index].a.a_uint_fast64_t;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_DOUBLE:
                        {
                          double arg = a.arg[dp->arg_index].a.a_double;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_LONGDOUBLE:
                        {
                          long double arg = a.arg[dp->arg_index].a.a_longdouble;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      case TYPE_CHAR:
                        {
                          int arg = a.arg[dp->arg_index].a.a_char;
                          SNPRINTF_BUF (arg);
                        }
                        break;
#if HAVE_WINT_T
                      case TYPE_WIDE_CHAR:
                        {
                          wint_t arg = a.arg[dp->arg_index].a.a_wide_char;
                          SNPRINTF_BUF (arg);
                        }
                        break;
#endif
                      case TYPE_STRING:
                        {
                          const char *arg = a.arg[dp->arg_index].a.a_string;
                          SNPRINTF_BUF (arg);
                        }
                        break;
#if HAVE_WCHAR_T
                      case TYPE_WIDE_STRING:
                        {
                          const wchar_t *arg = a.arg[dp->arg_index].a.a_wide_string;
                          SNPRINTF_BUF (arg);
                        }
                        break;
#endif
                      case TYPE_POINTER:
                        {
                          void *arg = a.arg[dp->arg_index].a.a_pointer;
                          SNPRINTF_BUF (arg);
                        }
                        break;
                      default:
                        abort ();
                      }

#if USE_SNPRINTF
                     
                    if (count >= 0)
                      {
                         
                        if ((unsigned int) count < maxlen
                            && ((TCHAR_T *) (result + length)) [count] != '\0')
                          abort ();
                         
                        if (retcount > count)
                          count = retcount;
                      }
                    else
                      {
                         
                        if (fbp[1] != '\0')
                          {
                             
                            fbp[1] = '\0';
                            continue;
                          }
                        else
                          {
                             
                            if (retcount < 0)
                              {
# if (WIDE_CHAR_VERSION && DCHAR_IS_TCHAR) || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF
                                 
                                size_t tmp_length =
                                  MAX_ROOM_NEEDED (&a, dp->arg_index,
                                                   dp->conversion, type, flags,
                                                   width,
                                                   has_precision,
                                                   precision, pad_ourselves);

                                if (maxlen < tmp_length)
                                  {
                                     
                                    size_t bigger_need =
                                      xsum (length,
                                            xsum (tmp_length,
                                                  TCHARS_PER_DCHAR - 1)
                                            / TCHARS_PER_DCHAR);
                                     
                                    size_t bigger_need2 =
                                      xsum (xtimes (allocated, 2), 12);
                                    if (bigger_need < bigger_need2)
                                      bigger_need = bigger_need2;
                                    ENSURE_ALLOCATION (bigger_need);
                                    continue;
                                  }
# endif
                              }
                            else
                              {
                                count = retcount;
# if WIDE_CHAR_VERSION && defined __MINGW32__
                                if (count == 0 && dp->conversion == 'c')
                                   
                                  count = 1;
# endif
                              }
                          }
                      }
#endif

                     
                    if (count < 0)
                      {
                         
                        if (errno == 0)
                          {
                            if (dp->conversion == 'c' || dp->conversion == 's')
                              errno = EILSEQ;
                            else
                              errno = EINVAL;
                          }

                        goto fail_with_errno;
                      }

#if USE_SNPRINTF
                     
                    if ((unsigned int) count + 1 >= maxlen)
                      {
                         
                        if (maxlen == INT_MAX / TCHARS_PER_DCHAR)
                          goto overflow;
                        else
                          {
                             
                            size_t n =
                              xmax (xsum (length,
                                          ((unsigned int) count + 2
                                           + TCHARS_PER_DCHAR - 1)
                                          / TCHARS_PER_DCHAR),
                                    xtimes (allocated, 2));

                            ENSURE_ALLOCATION (n);
                            continue;
                          }
                      }
#endif

#if NEED_PRINTF_UNBOUNDED_PRECISION
                    if (prec_ourselves)
                      {
                         
                        TCHAR_T *prec_ptr =
# if USE_SNPRINTF
                          (TCHAR_T *) (result + length);
# else
                          tmp;
# endif
                        size_t prefix_count;
                        size_t move;

                        prefix_count = 0;
                         
                        if (count >= 1
                            && (*prec_ptr == '-' || *prec_ptr == '+'
                                || *prec_ptr == ' '))
                          prefix_count = 1;
                         
                        else if (count >= 2
                                 && prec_ptr[0] == '0'
                                 && (prec_ptr[1] == 'x' || prec_ptr[1] == 'X'))
                          prefix_count = 2;

                        move = count - prefix_count;
                        if (precision > move)
                          {
                             
                            size_t insert = precision - move;
                            TCHAR_T *prec_end;

# if USE_SNPRINTF
                            size_t n =
                              xsum (length,
                                    (count + insert + TCHARS_PER_DCHAR - 1)
                                    / TCHARS_PER_DCHAR);
                            length += (count + TCHARS_PER_DCHAR - 1) / TCHARS_PER_DCHAR;
                            ENSURE_ALLOCATION (n);
                            length -= (count + TCHARS_PER_DCHAR - 1) / TCHARS_PER_DCHAR;
                            prec_ptr = (TCHAR_T *) (result + length);
# endif

                            prec_end = prec_ptr + count;
                            prec_ptr += prefix_count;

                            while (prec_end > prec_ptr)
                              {
                                prec_end--;
                                prec_end[insert] = prec_end[0];
                              }

                            prec_end += insert;
                            do
                              *--prec_end = '0';
                            while (prec_end > prec_ptr);

                            count += insert;
                          }
                      }
#endif

#if !USE_SNPRINTF
                    if (count >= tmp_length)
                       
                      abort ();
#endif

#if !DCHAR_IS_TCHAR
                     
                    if (dp->conversion == 'c' || dp->conversion == 's'
# if __GLIBC__ >= 2 && !defined __UCLIBC__
                        || (flags & FLAG_LOCALIZED)
# endif
                       )
                      {
                         
                        const TCHAR_T *tmpsrc;
                        DCHAR_T *tmpdst;
                        size_t tmpdst_len;
                         
                        static_assert (sizeof (TCHAR_T) == 1);
# if USE_SNPRINTF
                        tmpsrc = (TCHAR_T *) (result + length);
# else
                        tmpsrc = tmp;
# endif
# if WIDE_CHAR_VERSION
                         
                        mbstate_t state;

                        mbszero (&state);
                        tmpdst_len = 0;
                        {
                          const TCHAR_T *src = tmpsrc;
                          size_t srclen = count;

                          for (; srclen > 0; tmpdst_len++)
                            {
                               
                              size_t ret = mbrtowc (NULL, src, srclen, &state);
                              if (ret == (size_t)(-2) || ret == (size_t)(-1))
                                goto fail_with_EILSEQ;
                              if (ret == 0)
                                ret = 1;
                              src += ret;
                              srclen -= ret;
                            }
                        }

                        tmpdst =
                          (wchar_t *) malloc ((tmpdst_len + 1) * sizeof (wchar_t));
                        if (tmpdst == NULL)
                          goto out_of_memory;

                        mbszero (&state);
                        {
                          DCHAR_T *destptr = tmpdst;
                          const TCHAR_T *src = tmpsrc;
                          size_t srclen = count;

                          for (; srclen > 0; destptr++)
                            {
                               
                              size_t ret = mbrtowc (destptr, src, srclen, &state);
                              if (ret == (size_t)(-2) || ret == (size_t)(-1))
                                 
                                abort ();
                              if (ret == 0)
                                ret = 1;
                              src += ret;
                              srclen -= ret;
                            }
                        }
# else
                        tmpdst =
                          DCHAR_CONV_FROM_ENCODING (locale_charset (),
                                                    iconveh_question_mark,
                                                    tmpsrc, count,
                                                    NULL,
                                                    NULL, &tmpdst_len);
                        if (tmpdst == NULL)
                          goto fail_with_errno;
# endif
                        ENSURE_ALLOCATION_ELSE (xsum (length, tmpdst_len),
                                                { free (tmpdst); goto out_of_memory; });
                        DCHAR_CPY (result + length, tmpdst, tmpdst_len);
                        free (tmpdst);
                        count = tmpdst_len;
                      }
                    else
                      {
                         
# if USE_SNPRINTF
                         
                        if (sizeof (DCHAR_T) != sizeof (TCHAR_T))
# endif
                          {
                            const TCHAR_T *tmpsrc;
                            DCHAR_T *tmpdst;
                            size_t n;

# if USE_SNPRINTF
                            if (result == resultbuf)
                              {
                                tmpsrc = (TCHAR_T *) (result + length);
                                 
                                ENSURE_ALLOCATION (xsum (length, count));
                              }
                            else
                              {
                                 
                                ENSURE_ALLOCATION (xsum (length, count));
                                tmpsrc = (TCHAR_T *) (result + length);
                              }
# else
                            tmpsrc = tmp;
                            ENSURE_ALLOCATION (xsum (length, count));
# endif
                            tmpdst = result + length;
                             
                            tmpsrc += count;
                            tmpdst += count;
                            for (n = count; n > 0; n--)
                              *--tmpdst = *--tmpsrc;
                          }
                      }
#endif

#if DCHAR_IS_TCHAR && !USE_SNPRINTF
                     
                    if (count > allocated - length)
                      {
                         
                        size_t n =
                          xmax (xsum (length, count), xtimes (allocated, 2));

                        ENSURE_ALLOCATION (n);
                      }
#endif

                     

                     
#if (WIDE_CHAR_VERSION && MUSL_LIBC) || !DCHAR_IS_TCHAR || ENABLE_UNISTDIO || NEED_PRINTF_FLAG_LEFTADJUST || NEED_PRINTF_FLAG_ZERO || NEED_PRINTF_UNBOUNDED_PRECISION
                    if (pad_ourselves && has_width)
                      {
                        size_t w;
# if ENABLE_UNISTDIO
                         
                        w = DCHAR_MBSNLEN (result + length, count);
# else
                         
                        w = count;
# endif
                        if (w < width)
                          {
                            size_t pad = width - w;

                             
                            if (xsum (count, pad) > allocated - length)
                              {
                                 
                                size_t n =
                                  xmax (xsum3 (length, count, pad),
                                        xtimes (allocated, 2));

# if USE_SNPRINTF
                                length += count;
                                ENSURE_ALLOCATION (n);
                                length -= count;
# else
                                ENSURE_ALLOCATION (n);
# endif
                              }
                             

                            {
# if !DCHAR_IS_TCHAR || USE_SNPRINTF
                              DCHAR_T * const rp = result + length;
# else
                              DCHAR_T * const rp = tmp;
# endif
                              DCHAR_T *p = rp + count;
                              DCHAR_T *end = p + pad;
                              DCHAR_T *pad_ptr;
# if !DCHAR_IS_TCHAR || ENABLE_UNISTDIO
                              if (dp->conversion == 'c'
                                  || dp->conversion == 's')
                                 
                                pad_ptr = NULL;
                              else
# endif
                                {
                                  pad_ptr = (*rp == '-' ? rp + 1 : rp);
                                   
                                  if ((*pad_ptr >= 'A' && *pad_ptr <= 'Z')
                                      || (*pad_ptr >= 'a' && *pad_ptr <= 'z'))
                                    pad_ptr = NULL;
                                  else
                                     
                                    if (p - rp >= 2
                                        && *rp == '0'
                                        && (((dp->conversion == 'a'
                                              || dp->conversion == 'x')
                                             && rp[1] == 'x')
                                            || ((dp->conversion == 'A'
                                                 || dp->conversion == 'X')
                                                && rp[1] == 'X')
                                            || (dp->conversion == 'b'
                                                && rp[1] == 'b')
                                            || (dp->conversion == 'B'
                                                && rp[1] == 'B')))
                                      pad_ptr += 2;
                                }
                               

                              count = count + pad;  

                              if (flags & FLAG_LEFT)
                                {
                                   
                                  for (; pad > 0; pad--)
                                    *p++ = ' ';
                                }
                              else if ((flags & FLAG_ZERO) && pad_ptr != NULL
                                        
                                       && !(has_precision
                                            && (dp->conversion == 'd'
                                                || dp->conversion == 'i'
                                                || dp->conversion == 'o'
                                                || dp->conversion == 'u'
                                                || dp->conversion == 'x'
                                                || dp->conversion == 'X'
                                                 
                                                || dp->conversion == 'b'
                                                || dp->conversion == 'B')))
                                {
                                   
                                  DCHAR_T *q = end;

                                  while (p > pad_ptr)
                                    *--q = *--p;
                                  for (; pad > 0; pad--)
                                    *p++ = '0';
                                }
                              else
                                {
                                   
                                  DCHAR_T *q = end;

                                  while (p > rp)
                                    *--q = *--p;
                                  for (; pad > 0; pad--)
                                    *p++ = ' ';
                                }
                            }
                          }
                      }
#endif

                     

#if !DCHAR_IS_TCHAR || USE_SNPRINTF
                     
#else
                     
                    memcpy (result + length, tmp, count * sizeof (DCHAR_T));
#endif
#if !USE_SNPRINTF
                    if (tmp != tmpbuf)
                      free (tmp);
#endif

#if NEED_PRINTF_DIRECTIVE_F
                    if (dp->conversion == 'F')
                      {
                         
                        DCHAR_T *rp = result + length;
                        size_t rc;
                        for (rc = count; rc > 0; rc--, rp++)
                          if (*rp >= 'a' && *rp <= 'z')
                            *rp = *rp - 'a' + 'A';
                      }
#endif

                    length += count;
                    break;
                  }
                errno = orig_errno;
#undef pad_ourselves
#undef prec_ourselves
              }
          }
      }

     
    ENSURE_ALLOCATION (xsum (length, 1));
    result[length] = '\0';

    if (result != resultbuf && length + 1 < allocated)
      {
         
        DCHAR_T *memory;

        memory = (DCHAR_T *) realloc (result, (length + 1) * sizeof (DCHAR_T));
        if (memory != NULL)
          result = memory;
      }

    if (buf_malloced != NULL)
      free (buf_malloced);
    CLEANUP ();
    *lengthp = length;
     
    return result;

#if USE_SNPRINTF
  overflow:
    errno = EOVERFLOW;
    goto fail_with_errno;
#endif

  out_of_memory:
    errno = ENOMEM;
    goto fail_with_errno;

#if ENABLE_UNISTDIO || ((!USE_SNPRINTF || WIDE_CHAR_VERSION || !HAVE_SNPRINTF_RETVAL_C99 || USE_MSVC__SNPRINTF || NEED_PRINTF_DIRECTIVE_LS || ENABLE_WCHAR_FALLBACK) && HAVE_WCHAR_T) || ((NEED_PRINTF_DIRECTIVE_LC || ENABLE_WCHAR_FALLBACK) && HAVE_WINT_T && !WIDE_CHAR_VERSION) || (NEED_WPRINTF_DIRECTIVE_C && WIDE_CHAR_VERSION)
  fail_with_EILSEQ:
    errno = EILSEQ;
    goto fail_with_errno;
#endif

  fail_with_errno:
    if (result != resultbuf)
      free (result);
    if (buf_malloced != NULL)
      free (buf_malloced);
    CLEANUP ();
    return NULL;
  }

 out_of_memory_1:
  errno = ENOMEM;
  goto fail_1_with_errno;

 fail_1_with_EINVAL:
  errno = EINVAL;
  goto fail_1_with_errno;

 fail_1_with_errno:
  CLEANUP ();
  return NULL;
}

#undef MAX_ROOM_NEEDED
#undef TCHARS_PER_DCHAR
#undef SNPRINTF
#undef USE_SNPRINTF
#undef DCHAR_SET
#undef DCHAR_CPY
#undef PRINTF_PARSE
#undef DIRECTIVES
#undef DIRECTIVE
#undef DCHAR_IS_TCHAR
#undef TCHAR_T
#undef DCHAR_T
#undef FCHAR_T
#undef VASNPRINTF
