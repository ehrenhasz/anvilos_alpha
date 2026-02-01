 
#if !(defined __osf__ && 0)
# define MULTIBYTE_IS_FORMAT_SAFE 1
#endif
#define DO_MULTIBYTE (! MULTIBYTE_IS_FORMAT_SAFE)

#if DO_MULTIBYTE
# include <wchar.h>
  static const mbstate_t mbstate_zero;
#endif

#include <limits.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "attribute.h"
#include <intprops.h>

#ifdef COMPILE_WIDE
# include <endian.h>
# define CHAR_T wchar_t
# define UCHAR_T unsigned int
# define L_(Str) L##Str
# define NLW(Sym) _NL_W##Sym

# define MEMCPY(d, s, n) __wmemcpy (d, s, n)
# define STRLEN(s) __wcslen (s)

#else
# define CHAR_T char
# define UCHAR_T unsigned char
# define L_(Str) Str
# define NLW(Sym) Sym
# define ABALTMON_1 _NL_ABALTMON_1

# define MEMCPY(d, s, n) memcpy (d, s, n)
# define STRLEN(s) strlen (s)

#endif

 
#define SHR(a, b)       \
  (-1 >> 1 == -1        \
   ? (a) >> (b)         \
   : ((a) + ((a) < 0)) / (1 << (b)) - ((a) < 0))

#define TM_YEAR_BASE 1900

#ifndef __isleap
 
# define __isleap(year) \
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#endif


#ifdef _LIBC
# define mktime_z(tz, tm) mktime (tm)
# define tzname __tzname
# define tzset __tzset
#endif

#ifndef FPRINTFTIME
# define FPRINTFTIME 0
#endif

#if FPRINTFTIME
# define STREAM_OR_CHAR_T FILE
# define STRFTIME_ARG(x)  
#else
# define STREAM_OR_CHAR_T CHAR_T
# define STRFTIME_ARG(x) x,
#endif

#if FPRINTFTIME
# define memset_byte(P, Len, Byte) \
  do { size_t _i; for (_i = 0; _i < Len; _i++) fputc (Byte, P); } while (0)
# define memset_space(P, Len) memset_byte (P, Len, ' ')
# define memset_zero(P, Len) memset_byte (P, Len, '0')
#elif defined COMPILE_WIDE
# define memset_space(P, Len) (wmemset (P, L' ', Len), (P) += (Len))
# define memset_zero(P, Len) (wmemset (P, L'0', Len), (P) += (Len))
#else
# define memset_space(P, Len) (memset (P, ' ', Len), (P) += (Len))
# define memset_zero(P, Len) (memset (P, '0', Len), (P) += (Len))
#endif

#if FPRINTFTIME
# define advance(P, N)
#else
# define advance(P, N) ((P) += (N))
#endif

#define add(n, f) width_add (width, n, f)
#define width_add(width, n, f)                                                \
  do                                                                          \
    {                                                                         \
      size_t _n = (n);                                                        \
      size_t _w = pad == L_('-') || width < 0 ? 0 : width;                    \
      size_t _incr = _n < _w ? _w : _n;                                       \
      if (_incr >= maxsize - i)                                               \
        {                                                                     \
          errno = ERANGE;                                                     \
          return 0;                                                           \
        }                                                                     \
      if (p)                                                                  \
        {                                                                     \
          if (_n < _w)                                                        \
            {                                                                 \
              size_t _delta = _w - _n;                                        \
              if (pad == L_('0') || pad == L_('+'))                           \
                memset_zero (p, _delta);                                      \
              else                                                            \
                memset_space (p, _delta);                                     \
            }                                                                 \
          f;                                                                  \
          advance (p, _n);                                                    \
        }                                                                     \
      i += _incr;                                                             \
    } while (0)

#define add1(c) width_add1 (width, c)
#if FPRINTFTIME
# define width_add1(width, c) width_add (width, 1, fputc (c, p))
#else
# define width_add1(width, c) width_add (width, 1, *p = c)
#endif

#define cpy(n, s) width_cpy (width, n, s)
#if FPRINTFTIME
# define width_cpy(width, n, s)                                               \
    width_add (width, n,                                                      \
     do                                                                       \
       {                                                                      \
         if (to_lowcase)                                                      \
           fwrite_lowcase (p, (s), _n);                                       \
         else if (to_uppcase)                                                 \
           fwrite_uppcase (p, (s), _n);                                       \
         else                                                                 \
           {                                                                  \
                                                       \
             fwrite (s, _n, 1, p);                                            \
           }                                                                  \
       }                                                                      \
     while (0)                                                                \
    )
#else
# define width_cpy(width, n, s)                                               \
    width_add (width, n,                                                      \
         if (to_lowcase)                                                      \
           memcpy_lowcase (p, (s), _n LOCALE_ARG);                            \
         else if (to_uppcase)                                                 \
           memcpy_uppcase (p, (s), _n LOCALE_ARG);                            \
         else                                                                 \
           MEMCPY ((void *) p, (void const *) (s), _n))
#endif

#ifdef COMPILE_WIDE
# ifndef USE_IN_EXTENDED_LOCALE_MODEL
#  undef __mbsrtowcs_l
#  define __mbsrtowcs_l(d, s, l, st, loc) __mbsrtowcs (d, s, l, st)
# endif
#endif


#if defined _LIBC && defined USE_IN_EXTENDED_LOCALE_MODEL
 
# define strftime               __strftime_l
# define wcsftime               __wcsftime_l
# undef _NL_CURRENT
# define _NL_CURRENT(category, item) \
  (current->values[_NL_ITEM_INDEX (item)].string)
# define LOCALE_PARAM , locale_t loc
# define LOCALE_ARG , loc
# define HELPER_LOCALE_ARG  , current
#else
# define LOCALE_PARAM
# define LOCALE_ARG
# ifdef _LIBC
#  define HELPER_LOCALE_ARG , _NL_CURRENT_DATA (LC_TIME)
# else
#  define HELPER_LOCALE_ARG
# endif
#endif

#ifdef COMPILE_WIDE
# ifdef USE_IN_EXTENDED_LOCALE_MODEL
#  define TOUPPER(Ch, L) __towupper_l (Ch, L)
#  define TOLOWER(Ch, L) __towlower_l (Ch, L)
# else
#  define TOUPPER(Ch, L) towupper (Ch)
#  define TOLOWER(Ch, L) towlower (Ch)
# endif
#else
# ifdef USE_IN_EXTENDED_LOCALE_MODEL
#  define TOUPPER(Ch, L) __toupper_l (Ch, L)
#  define TOLOWER(Ch, L) __tolower_l (Ch, L)
# else
#  define TOUPPER(Ch, L) toupper (Ch)
#  define TOLOWER(Ch, L) tolower (Ch)
# endif
#endif
 
#define ISDIGIT(Ch) ((unsigned int) (Ch) - L_('0') <= 9)

 
#if __GNUC__ >= 7 && !__OPTIMIZE__
# pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

#if FPRINTFTIME
static void
fwrite_lowcase (FILE *fp, const CHAR_T *src, size_t len)
{
  while (len-- > 0)
    {
      fputc (TOLOWER ((UCHAR_T) *src, loc), fp);
      ++src;
    }
}

static void
fwrite_uppcase (FILE *fp, const CHAR_T *src, size_t len)
{
  while (len-- > 0)
    {
      fputc (TOUPPER ((UCHAR_T) *src, loc), fp);
      ++src;
    }
}
#else
static CHAR_T *memcpy_lowcase (CHAR_T *dest, const CHAR_T *src,
                               size_t len LOCALE_PARAM);

static CHAR_T *
memcpy_lowcase (CHAR_T *dest, const CHAR_T *src, size_t len LOCALE_PARAM)
{
  while (len-- > 0)
    dest[len] = TOLOWER ((UCHAR_T) src[len], loc);
  return dest;
}

static CHAR_T *memcpy_uppcase (CHAR_T *dest, const CHAR_T *src,
                               size_t len LOCALE_PARAM);

static CHAR_T *
memcpy_uppcase (CHAR_T *dest, const CHAR_T *src, size_t len LOCALE_PARAM)
{
  while (len-- > 0)
    dest[len] = TOUPPER ((UCHAR_T) src[len], loc);
  return dest;
}
#endif


#if ! HAVE_TM_GMTOFF
 
# define tm_diff ftime_tm_diff
static int tm_diff (const struct tm *, const struct tm *);
static int
tm_diff (const struct tm *a, const struct tm *b)
{
   
  int a4 = SHR (a->tm_year, 2) + SHR (TM_YEAR_BASE, 2) - ! (a->tm_year & 3);
  int b4 = SHR (b->tm_year, 2) + SHR (TM_YEAR_BASE, 2) - ! (b->tm_year & 3);
  int a100 = (a4 + (a4 < 0)) / 25 - (a4 < 0);
  int b100 = (b4 + (b4 < 0)) / 25 - (b4 < 0);
  int a400 = SHR (a100, 2);
  int b400 = SHR (b100, 2);
  int intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400);
  int years = a->tm_year - b->tm_year;
  int days = (365 * years + intervening_leap_days
              + (a->tm_yday - b->tm_yday));
  return (60 * (60 * (24 * days + (a->tm_hour - b->tm_hour))
                + (a->tm_min - b->tm_min))
          + (a->tm_sec - b->tm_sec));
}
#endif  



 
#define ISO_WEEK_START_WDAY 1  
#define ISO_WEEK1_WDAY 4  
#define YDAY_MINIMUM (-366)
static int iso_week_days (int, int);
static __inline int
iso_week_days (int yday, int wday)
{
   
  int big_enough_multiple_of_7 = (-YDAY_MINIMUM / 7 + 2) * 7;
  return (yday
          - (yday - wday + ISO_WEEK1_WDAY + big_enough_multiple_of_7) % 7
          + ISO_WEEK1_WDAY - ISO_WEEK_START_WDAY);
}


 

#if FPRINTFTIME
# undef my_strftime
# define my_strftime fprintftime
#endif

#ifdef my_strftime
# define extra_args , tz, ns
# define extra_args_spec , timezone_t tz, int ns
#else
# if defined COMPILE_WIDE
#  define my_strftime wcsftime
#  define nl_get_alt_digit _nl_get_walt_digit
# else
#  define my_strftime strftime
#  define nl_get_alt_digit _nl_get_alt_digit
# endif
# define extra_args
# define extra_args_spec
 
# define tz 1
# define ns 0
#endif

static size_t __strftime_internal (STREAM_OR_CHAR_T *, STRFTIME_ARG (size_t)
                                   const CHAR_T *, const struct tm *,
                                   bool, int, int, bool *
                                   extra_args_spec LOCALE_PARAM);

 
size_t
my_strftime (STREAM_OR_CHAR_T *s, STRFTIME_ARG (size_t maxsize)
             const CHAR_T *format,
             const struct tm *tp extra_args_spec LOCALE_PARAM)
{
  bool tzset_called = false;
  return __strftime_internal (s, STRFTIME_ARG (maxsize) format, tp, false,
                              0, -1, &tzset_called extra_args LOCALE_ARG);
}
libc_hidden_def (my_strftime)

 
static size_t
__strftime_internal (STREAM_OR_CHAR_T *s, STRFTIME_ARG (size_t maxsize)
                     const CHAR_T *format,
                     const struct tm *tp, bool upcase,
                     int yr_spec, int width, bool *tzset_called
                     extra_args_spec LOCALE_PARAM)
{
#if defined _LIBC && defined USE_IN_EXTENDED_LOCALE_MODEL
  struct __locale_data *const current = loc->__locales[LC_TIME];
#endif
#if FPRINTFTIME
  size_t maxsize = (size_t) -1;
#endif

  int saved_errno = errno;
  int hour12 = tp->tm_hour;
#ifdef _NL_CURRENT
   
# define a_wkday \
  ((const CHAR_T *) (tp->tm_wday < 0 || tp->tm_wday > 6                      \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(ABDAY_1) + tp->tm_wday)))
# define f_wkday \
  ((const CHAR_T *) (tp->tm_wday < 0 || tp->tm_wday > 6                      \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(DAY_1) + tp->tm_wday)))
# define a_month \
  ((const CHAR_T *) (tp->tm_mon < 0 || tp->tm_mon > 11                       \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(ABMON_1) + tp->tm_mon)))
# define f_month \
  ((const CHAR_T *) (tp->tm_mon < 0 || tp->tm_mon > 11                       \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(MON_1) + tp->tm_mon)))
# define a_altmonth \
  ((const CHAR_T *) (tp->tm_mon < 0 || tp->tm_mon > 11                       \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(ABALTMON_1) + tp->tm_mon)))
# define f_altmonth \
  ((const CHAR_T *) (tp->tm_mon < 0 || tp->tm_mon > 11                       \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(ALTMON_1) + tp->tm_mon)))
# define ampm \
  ((const CHAR_T *) _NL_CURRENT (LC_TIME, tp->tm_hour > 11                    \
                                 ? NLW(PM_STR) : NLW(AM_STR)))

# define aw_len STRLEN (a_wkday)
# define am_len STRLEN (a_month)
# define aam_len STRLEN (a_altmonth)
# define ap_len STRLEN (ampm)
#endif
#if HAVE_TZNAME
  char **tzname_vec = tzname;
#endif
  const char *zone;
  size_t i = 0;
  STREAM_OR_CHAR_T *p = s;
  const CHAR_T *f;
#if DO_MULTIBYTE && !defined COMPILE_WIDE
  const char *format_end = NULL;
#endif

  zone = NULL;
#if HAVE_STRUCT_TM_TM_ZONE
   
  zone = (const char *) tp->tm_zone;
#endif
#if HAVE_TZNAME
  if (!tz)
    {
      if (! (zone && *zone))
        zone = "GMT";
    }
  else
    {
# if !HAVE_STRUCT_TM_TM_ZONE
       
      tzname_vec = tz->tzname_copy;
# endif
    }
   
  if (!(zone && *zone) && tp->tm_isdst >= 0)
    {
       
# ifndef my_strftime
      if (!*tzset_called)
        {
          tzset ();
          *tzset_called = true;
        }
# endif
      zone = tzname_vec[tp->tm_isdst != 0];
    }
#endif
  if (! zone)
    zone = "";

  if (hour12 > 12)
    hour12 -= 12;
  else
    if (hour12 == 0)
      hour12 = 12;

  for (f = format; *f != '\0'; width = -1, f++)
    {
      int pad = 0;   
      int modifier;              
      int digits = 0;            
      int number_value;          
      unsigned int u_number_value;  
      bool negative_number;      
      bool always_output_a_sign;  
      int tz_colon_mask;         
      const CHAR_T *subfmt;
      CHAR_T *bufp;
      CHAR_T buf[1
                 + 2  
                 + (sizeof (int) < sizeof (time_t)
                    ? INT_STRLEN_BOUND (time_t)
                    : INT_STRLEN_BOUND (int))];
      bool to_lowcase = false;
      bool to_uppcase = upcase;
      size_t colons;
      bool change_case = false;
      int format_char;
      int subwidth;

#if DO_MULTIBYTE && !defined COMPILE_WIDE
      switch (*f)
        {
        case L_('%'):
          break;

        case L_('\b'): case L_('\t'): case L_('\n'):
        case L_('\v'): case L_('\f'): case L_('\r'):
        case L_(' '): case L_('!'): case L_('"'): case L_('#'): case L_('&'):
        case L_('\''): case L_('('): case L_(')'): case L_('*'): case L_('+'):
        case L_(','): case L_('-'): case L_('.'): case L_('/'): case L_('0'):
        case L_('1'): case L_('2'): case L_('3'): case L_('4'): case L_('5'):
        case L_('6'): case L_('7'): case L_('8'): case L_('9'): case L_(':'):
        case L_(';'): case L_('<'): case L_('='): case L_('>'): case L_('?'):
        case L_('A'): case L_('B'): case L_('C'): case L_('D'): case L_('E'):
        case L_('F'): case L_('G'): case L_('H'): case L_('I'): case L_('J'):
        case L_('K'): case L_('L'): case L_('M'): case L_('N'): case L_('O'):
        case L_('P'): case L_('Q'): case L_('R'): case L_('S'): case L_('T'):
        case L_('U'): case L_('V'): case L_('W'): case L_('X'): case L_('Y'):
        case L_('Z'): case L_('['): case L_('\\'): case L_(']'): case L_('^'):
        case L_('_'): case L_('a'): case L_('b'): case L_('c'): case L_('d'):
        case L_('e'): case L_('f'): case L_('g'): case L_('h'): case L_('i'):
        case L_('j'): case L_('k'): case L_('l'): case L_('m'): case L_('n'):
        case L_('o'): case L_('p'): case L_('q'): case L_('r'): case L_('s'):
        case L_('t'): case L_('u'): case L_('v'): case L_('w'): case L_('x'):
        case L_('y'): case L_('z'): case L_('{'): case L_('|'): case L_('}'):
        case L_('~'):
           
          add1 (*f);
          continue;

        default:
           
          {
            mbstate_t mbstate = mbstate_zero;
            size_t len = 0;
            size_t fsize;

            if (! format_end)
              format_end = f + strlen (f) + 1;
            fsize = format_end - f;

            do
              {
                size_t bytes = mbrlen (f + len, fsize - len, &mbstate);

                if (bytes == 0)
                  break;

                if (bytes == (size_t) -2)
                  {
                    len += strlen (f + len);
                    break;
                  }

                if (bytes == (size_t) -1)
                  {
                    len++;
                    break;
                  }

                len += bytes;
              }
            while (! mbsinit (&mbstate));

            cpy (len, f);
            f += len - 1;
            continue;
          }
        }

#else  

       
      if (*f != L_('%'))
        {
          add1 (*f);
          continue;
        }

#endif  

      char const *percent = f;

       
      while (1)
        {
          switch (*++f)
            {
               
            case L_('_'):
            case L_('-'):
            case L_('+'):
            case L_('0'):
              pad = *f;
              continue;

               
            case L_('^'):
              to_uppcase = true;
              continue;
            case L_('#'):
              change_case = true;
              continue;

            default:
              break;
            }
          break;
        }

      if (ISDIGIT (*f))
        {
          width = 0;
          do
            {
              if (ckd_mul (&width, width, 10)
                  || ckd_add (&width, width, *f - L_('0')))
                width = INT_MAX;
              ++f;
            }
          while (ISDIGIT (*f));
        }

       
      switch (*f)
        {
        case L_('E'):
        case L_('O'):
          modifier = *f++;
          break;

        default:
          modifier = 0;
          break;
        }

       
      format_char = *f;
      switch (format_char)
        {
#define DO_NUMBER(d, v) \
          do                                                                  \
            {                                                                 \
              digits = d;                                                     \
              number_value = v;                                               \
              goto do_number;                                                 \
            }                                                                 \
          while (0)
#define DO_SIGNED_NUMBER(d, negative, v) \
          DO_MAYBE_SIGNED_NUMBER (d, negative, v, do_signed_number)
#define DO_YEARISH(d, negative, v) \
          DO_MAYBE_SIGNED_NUMBER (d, negative, v, do_yearish)
#define DO_MAYBE_SIGNED_NUMBER(d, negative, v, label) \
          do                                                                  \
            {                                                                 \
              digits = d;                                                     \
              negative_number = negative;                                     \
              u_number_value = v;                                             \
              goto label;                                                     \
            }                                                                 \
          while (0)

           
#define DO_TZ_OFFSET(d, mask, v) \
          do                                                                  \
            {                                                                 \
              digits = d;                                                     \
              tz_colon_mask = mask;                                           \
              u_number_value = v;                                             \
              goto do_tz_offset;                                              \
            }                                                                 \
          while (0)
#define DO_NUMBER_SPACEPAD(d, v) \
          do                                                                  \
            {                                                                 \
              digits = d;                                                     \
              number_value = v;                                               \
              goto do_number_spacepad;                                        \
            }                                                                 \
          while (0)

        case L_('%'):
          if (f - 1 != percent)
            goto bad_percent;
          add1 (*f);
          break;

        case L_('a'):
          if (modifier != 0)
            goto bad_format;
          if (change_case)
            {
              to_uppcase = true;
              to_lowcase = false;
            }
#ifdef _NL_CURRENT
          cpy (aw_len, a_wkday);
          break;
#else
          goto underlying_strftime;
#endif

        case 'A':
          if (modifier != 0)
            goto bad_format;
          if (change_case)
            {
              to_uppcase = true;
              to_lowcase = false;
            }
#ifdef _NL_CURRENT
          cpy (STRLEN (f_wkday), f_wkday);
          break;
#else
          goto underlying_strftime;
#endif

        case L_('b'):
        case L_('h'):
          if (change_case)
            {
              to_uppcase = true;
              to_lowcase = false;
            }
          if (modifier == L_('E'))
            goto bad_format;
#ifdef _NL_CURRENT
          if (modifier == L_('O'))
            cpy (aam_len, a_altmonth);
          else
            cpy (am_len, a_month);
          break;
#else
          goto underlying_strftime;
#endif

        case L_('B'):
          if (modifier == L_('E'))
            goto bad_format;
          if (change_case)
            {
              to_uppcase = true;
              to_lowcase = false;
            }
#ifdef _NL_CURRENT
          if (modifier == L_('O'))
            cpy (STRLEN (f_altmonth), f_altmonth);
          else
            cpy (STRLEN (f_month), f_month);
          break;
#else
          goto underlying_strftime;
#endif

        case L_('c'):
          if (modifier == L_('O'))
            goto bad_format;
#ifdef _NL_CURRENT
          if (! (modifier == L_('E')
                 && (*(subfmt =
                       (const CHAR_T *) _NL_CURRENT (LC_TIME,
                                                     NLW(ERA_D_T_FMT)))
                     != '\0')))
            subfmt = (const CHAR_T *) _NL_CURRENT (LC_TIME, NLW(D_T_FMT));
#else
          goto underlying_strftime;
#endif

        subformat:
          subwidth = -1;
        subformat_width:
          {
            size_t len = __strftime_internal (NULL, STRFTIME_ARG ((size_t) -1)
                                              subfmt, tp, to_uppcase,
                                              pad, subwidth, tzset_called
                                              extra_args LOCALE_ARG);
            add (len, __strftime_internal (p,
                                           STRFTIME_ARG (maxsize - i)
                                           subfmt, tp, to_uppcase,
                                           pad, subwidth, tzset_called
                                           extra_args LOCALE_ARG));
          }
          break;

#if !(defined _NL_CURRENT && HAVE_STRUCT_ERA_ENTRY)
        underlying_strftime:
          {
             
            char ufmt[5];
            char *u = ufmt;
            char ubuf[1024];  
            size_t len;
             
# ifdef strftime
#  undef strftime
            size_t strftime ();
# endif

             
            *u++ = ' ';
            *u++ = '%';
            if (modifier != 0)
              *u++ = modifier;
            *u++ = format_char;
            *u = '\0';
            len = strftime (ubuf, sizeof ubuf, ufmt, tp);
            if (len != 0)
              cpy (len - 1, ubuf + 1);
          }
          break;
#endif

        case L_('C'):
          if (modifier == L_('E'))
            {
#if HAVE_STRUCT_ERA_ENTRY
              struct era_entry *era = _nl_get_era_entry (tp HELPER_LOCALE_ARG);
              if (era)
                {
# ifdef COMPILE_WIDE
                  size_t len = __wcslen (era->era_wname);
                  cpy (len, era->era_wname);
# else
                  size_t len = strlen (era->era_name);
                  cpy (len, era->era_name);
# endif
                  break;
                }
#else
              goto underlying_strftime;
#endif
            }

          {
            bool negative_year = tp->tm_year < - TM_YEAR_BASE;
            bool zero_thru_1899 = !negative_year & (tp->tm_year < 0);
            int century = ((tp->tm_year - 99 * zero_thru_1899) / 100
                           + TM_YEAR_BASE / 100);
            DO_YEARISH (2, negative_year, century);
          }

        case L_('x'):
          if (modifier == L_('O'))
            goto bad_format;
#ifdef _NL_CURRENT
          if (! (modifier == L_('E')
                 && (*(subfmt =
                       (const CHAR_T *) _NL_CURRENT (LC_TIME, NLW(ERA_D_FMT)))
                     != L_('\0'))))
            subfmt = (const CHAR_T *) _NL_CURRENT (LC_TIME, NLW(D_FMT));
          goto subformat;
#else
          goto underlying_strftime;
#endif
        case L_('D'):
          if (modifier != 0)
            goto bad_format;
          subfmt = L_("%m/%d/%y");
          goto subformat;

        case L_('d'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, tp->tm_mday);

        case L_('e'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER_SPACEPAD (2, tp->tm_mday);

           

        do_tz_offset:
          always_output_a_sign = true;
          goto do_number_body;

        do_yearish:
          if (pad == 0)
            pad = yr_spec;
          always_output_a_sign
            = (pad == L_('+')
               && ((digits == 2 ? 99 : 9999) < u_number_value
                   || digits < width));
          goto do_maybe_signed_number;

        do_number_spacepad:
          if (pad == 0)
            pad = L_('_');

        do_number:
           
          negative_number = number_value < 0;
          u_number_value = number_value;

        do_signed_number:
          always_output_a_sign = false;

        do_maybe_signed_number:
          tz_colon_mask = 0;

        do_number_body:
           
          if (modifier == L_('O') && !negative_number)
            {
#ifdef _NL_CURRENT
               
              const CHAR_T *cp = nl_get_alt_digit (u_number_value
                                                   HELPER_LOCALE_ARG);

              if (cp != NULL)
                {
                  size_t digitlen = STRLEN (cp);
                  if (digitlen != 0)
                    {
                      cpy (digitlen, cp);
                      break;
                    }
                }
#else
              goto underlying_strftime;
#endif
            }

          bufp = buf + sizeof (buf) / sizeof (buf[0]);

          if (negative_number)
            u_number_value = - u_number_value;

          do
            {
              if (tz_colon_mask & 1)
                *--bufp = ':';
              tz_colon_mask >>= 1;
              *--bufp = u_number_value % 10 + L_('0');
              u_number_value /= 10;
            }
          while (u_number_value != 0 || tz_colon_mask != 0);

        do_number_sign_and_padding:
          if (pad == 0)
            pad = L_('0');
          if (width < 0)
            width = digits;

          {
            CHAR_T sign_char = (negative_number ? L_('-')
                                : always_output_a_sign ? L_('+')
                                : 0);
            int numlen = buf + sizeof buf / sizeof buf[0] - bufp;
            int shortage = width - !!sign_char - numlen;
            int padding = pad == L_('-') || shortage <= 0 ? 0 : shortage;

            if (sign_char)
              {
                if (pad == L_('_'))
                  {
                    if (p)
                      memset_space (p, padding);
                    i += padding;
                    width -= padding;
                  }
                width_add1 (0, sign_char);
                width--;
              }

            cpy (numlen, bufp);
          }
          break;

        case L_('F'):
          if (modifier != 0)
            goto bad_format;
          if (pad == 0 && width < 0)
            {
              pad = L_('+');
              subwidth = 4;
            }
          else
            {
              subwidth = width - 6;
              if (subwidth < 0)
                subwidth = 0;
            }
          subfmt = L_("%Y-%m-%d");
          goto subformat_width;

        case L_('H'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, tp->tm_hour);

        case L_('I'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, hour12);

        case L_('k'):            
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER_SPACEPAD (2, tp->tm_hour);

        case L_('l'):            
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER_SPACEPAD (2, hour12);

        case L_('j'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_SIGNED_NUMBER (3, tp->tm_yday < -1, tp->tm_yday + 1U);

        case L_('M'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, tp->tm_min);

        case L_('m'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_SIGNED_NUMBER (2, tp->tm_mon < -1, tp->tm_mon + 1U);

#ifndef _LIBC
        case L_('N'):            
          if (modifier == L_('E'))
            goto bad_format;
          {
            int n = ns, ns_digits = 9;
            if (width <= 0)
              width = ns_digits;
            int ndigs = ns_digits;
            while (width < ndigs || (1 < ndigs && n % 10 == 0))
              ndigs--, n /= 10;
            for (int j = ndigs; 0 < j; j--)
              buf[j - 1] = n % 10 + L_('0'), n /= 10;
            if (!pad)
              pad = L_('0');
            width_cpy (0, ndigs, buf);
            width_add (width - ndigs, 0, (void) 0);
          }
          break;
#endif

        case L_('n'):
          add1 (L_('\n'));
          break;

        case L_('P'):
          to_lowcase = true;
#ifndef _NL_CURRENT
          format_char = L_('p');
#endif
          FALLTHROUGH;
        case L_('p'):
          if (change_case)
            {
              to_uppcase = false;
              to_lowcase = true;
            }
#ifdef _NL_CURRENT
          cpy (ap_len, ampm);
          break;
#else
          goto underlying_strftime;
#endif

        case L_('q'):            
          DO_SIGNED_NUMBER (1, false, ((tp->tm_mon * 11) >> 5) + 1);

        case L_('R'):
          subfmt = L_("%H:%M");
          goto subformat;

        case L_('r'):
#ifdef _NL_CURRENT
          if (*(subfmt = (const CHAR_T *) _NL_CURRENT (LC_TIME,
                                                       NLW(T_FMT_AMPM)))
              == L_('\0'))
            subfmt = L_("%I:%M:%S %p");
          goto subformat;
#else
          goto underlying_strftime;
#endif

        case L_('S'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, tp->tm_sec);

        case L_('s'):            
          {
            struct tm ltm;
            time_t t;

            ltm = *tp;
            ltm.tm_yday = -1;
            t = mktime_z (tz, &ltm);
            if (ltm.tm_yday < 0)
              {
                errno = EOVERFLOW;
                return 0;
              }

             

            bufp = buf + sizeof (buf) / sizeof (buf[0]);
            negative_number = t < 0;

            do
              {
                int d = t % 10;
                t /= 10;
                *--bufp = (negative_number ? -d : d) + L_('0');
              }
            while (t != 0);

            digits = 1;
            always_output_a_sign = false;
            goto do_number_sign_and_padding;
          }

        case L_('X'):
          if (modifier == L_('O'))
            goto bad_format;
#ifdef _NL_CURRENT
          if (! (modifier == L_('E')
                 && (*(subfmt =
                       (const CHAR_T *) _NL_CURRENT (LC_TIME, NLW(ERA_T_FMT)))
                     != L_('\0'))))
            subfmt = (const CHAR_T *) _NL_CURRENT (LC_TIME, NLW(T_FMT));
          goto subformat;
#else
          goto underlying_strftime;
#endif
        case L_('T'):
          subfmt = L_("%H:%M:%S");
          goto subformat;

        case L_('t'):
          add1 (L_('\t'));
          break;

        case L_('u'):
          DO_NUMBER (1, (tp->tm_wday - 1 + 7) % 7 + 1);

        case L_('U'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, (tp->tm_yday - tp->tm_wday + 7) / 7);

        case L_('V'):
        case L_('g'):
        case L_('G'):
          if (modifier == L_('E'))
            goto bad_format;
          {
             
            int year = (tp->tm_year
                        + (tp->tm_year < 0
                           ? TM_YEAR_BASE % 400
                           : TM_YEAR_BASE % 400 - 400));
            int year_adjust = 0;
            int days = iso_week_days (tp->tm_yday, tp->tm_wday);

            if (days < 0)
              {
                 
                year_adjust = -1;
                days = iso_week_days (tp->tm_yday + (365 + __isleap (year - 1)),
                                      tp->tm_wday);
              }
            else
              {
                int d = iso_week_days (tp->tm_yday - (365 + __isleap (year)),
                                       tp->tm_wday);
                if (0 <= d)
                  {
                     
                    year_adjust = 1;
                    days = d;
                  }
              }

            switch (*f)
              {
              case L_('g'):
                {
                  int yy = (tp->tm_year % 100 + year_adjust) % 100;
                  DO_YEARISH (2, false,
                              (0 <= yy
                               ? yy
                               : tp->tm_year < -TM_YEAR_BASE - year_adjust
                               ? -yy
                               : yy + 100));
                }

              case L_('G'):
                DO_YEARISH (4, tp->tm_year < -TM_YEAR_BASE - year_adjust,
                            (tp->tm_year + (unsigned int) TM_YEAR_BASE
                             + year_adjust));

              default:
                DO_NUMBER (2, days / 7 + 1);
              }
          }

        case L_('W'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, (tp->tm_yday - (tp->tm_wday - 1 + 7) % 7 + 7) / 7);

        case L_('w'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (1, tp->tm_wday);

        case L_('Y'):
          if (modifier == L_('E'))
            {
#if HAVE_STRUCT_ERA_ENTRY
              struct era_entry *era = _nl_get_era_entry (tp HELPER_LOCALE_ARG);
              if (era)
                {
# ifdef COMPILE_WIDE
                  subfmt = era->era_wformat;
# else
                  subfmt = era->era_format;
# endif
                  if (pad == 0)
                    pad = yr_spec;
                  goto subformat;
                }
#else
              goto underlying_strftime;
#endif
            }
          if (modifier == L_('O'))
            goto bad_format;

          DO_YEARISH (4, tp->tm_year < -TM_YEAR_BASE,
                      tp->tm_year + (unsigned int) TM_YEAR_BASE);

        case L_('y'):
          if (modifier == L_('E'))
            {
#if HAVE_STRUCT_ERA_ENTRY
              struct era_entry *era = _nl_get_era_entry (tp HELPER_LOCALE_ARG);
              if (era)
                {
                  int delta = tp->tm_year - era->start_date[0];
                  if (pad == 0)
                    pad = yr_spec;
                  DO_NUMBER (2, (era->offset
                                 + delta * era->absolute_direction));
                }
#else
              goto underlying_strftime;
#endif
            }

          {
            int yy = tp->tm_year % 100;
            if (yy < 0)
              yy = tp->tm_year < - TM_YEAR_BASE ? -yy : yy + 100;
            DO_YEARISH (2, false, yy);
          }

        case L_('Z'):
          if (change_case)
            {
              to_uppcase = false;
              to_lowcase = true;
            }

#ifdef COMPILE_WIDE
          {
             
            size_t w = pad == L_('-') || width < 0 ? 0 : width;
            char const *z = zone;
            mbstate_t st = {0};
            size_t len = __mbsrtowcs_l (p, &z, maxsize - i, &st, loc);
            if (len == (size_t) -1)
              return 0;
            size_t incr = len < w ? w : len;
            if (incr >= maxsize - i)
              {
                errno = ERANGE;
                return 0;
              }
            if (p)
              {
                if (len < w)
                  {
                    size_t delta = w - len;
                    __wmemmove (p + delta, p, len);
                    wchar_t wc = pad == L_('0') || pad == L_('+') ? L'0' : L' ';
                    wmemset (p, wc, delta);
                  }
                p += incr;
              }
            i += incr;
          }
#else
          cpy (strlen (zone), zone);
#endif
          break;

        case L_(':'):
           
          for (colons = 1; f[colons] == L_(':'); colons++)
            continue;
          if (f[colons] != L_('z'))
            goto bad_format;
          f += colons;
          goto do_z_conversion;

        case L_('z'):
          colons = 0;

        do_z_conversion:
          if (tp->tm_isdst < 0)
            break;

          {
            int diff;
            int hour_diff;
            int min_diff;
            int sec_diff;
#if HAVE_TM_GMTOFF
            diff = tp->tm_gmtoff;
#else
            if (!tz)
              diff = 0;
            else
              {
                struct tm gtm;
                struct tm ltm;
                time_t lt;

                 
# ifndef my_strftime
                if (!*tzset_called)
                  {
                    tzset ();
                    *tzset_called = true;
                  }
# endif

                ltm = *tp;
                ltm.tm_wday = -1;
                lt = mktime_z (tz, &ltm);
                if (ltm.tm_wday < 0 || ! localtime_rz (0, &lt, &gtm))
                  break;
                diff = tm_diff (&ltm, &gtm);
              }
#endif

            negative_number = diff < 0 || (diff == 0 && *zone == '-');
            hour_diff = diff / 60 / 60;
            min_diff = diff / 60 % 60;
            sec_diff = diff % 60;

            switch (colons)
              {
              case 0:  
                DO_TZ_OFFSET (5, 0, hour_diff * 100 + min_diff);

              case 1: tz_hh_mm:  
                DO_TZ_OFFSET (6, 04, hour_diff * 100 + min_diff);

              case 2: tz_hh_mm_ss:  
                DO_TZ_OFFSET (9, 024,
                              hour_diff * 10000 + min_diff * 100 + sec_diff);

              case 3:  
                if (sec_diff != 0)
                  goto tz_hh_mm_ss;
                if (min_diff != 0)
                  goto tz_hh_mm;
                DO_TZ_OFFSET (3, 0, hour_diff);

              default:
                goto bad_format;
              }
          }

        case L_('\0'):           
        bad_percent:
            --f;
            FALLTHROUGH;
        default:
           
        bad_format:
          cpy (f - percent + 1, percent);
          break;
        }
    }

#if ! FPRINTFTIME
  if (p && maxsize != 0)
    *p = L_('\0');
#endif

  errno = saved_errno;
  return i;
}
