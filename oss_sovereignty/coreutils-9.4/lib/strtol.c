 
#ifndef UNSIGNED
# define UNSIGNED 0
# define INT LONG int
#else
# define INT unsigned LONG int
#endif

 
#ifdef USE_IN_EXTENDED_LOCALE_MODEL
# undef strtol
# if UNSIGNED
#  ifdef USE_WIDE_CHAR
#   ifdef QUAD
#    define strtol __wcstoull_l
#   else
#    define strtol __wcstoul_l
#   endif
#  else
#   ifdef QUAD
#    define strtol __strtoull_l
#   else
#    define strtol __strtoul_l
#   endif
#  endif
# else
#  ifdef USE_WIDE_CHAR
#   ifdef QUAD
#    define strtol __wcstoll_l
#   else
#    define strtol __wcstol_l
#   endif
#  else
#   ifdef QUAD
#    define strtol __strtoll_l
#   else
#    define strtol __strtol_l
#   endif
#  endif
# endif
#else
# if UNSIGNED
#  undef strtol
#  ifdef USE_WIDE_CHAR
#   ifdef QUAD
#    define strtol wcstoull
#   else
#    define strtol wcstoul
#   endif
#  else
#   ifdef QUAD
#    define strtol strtoull
#   else
#    define strtol strtoul
#   endif
#  endif
# else
#  ifdef USE_WIDE_CHAR
#   undef strtol
#   ifdef QUAD
#    define strtol wcstoll
#   else
#    define strtol wcstol
#   endif
#  else
#   ifdef QUAD
#    undef strtol
#    define strtol strtoll
#   endif
#  endif
# endif
#endif

 
#ifdef QUAD
# define LONG long long
# define STRTOL_LONG_MIN LLONG_MIN
# define STRTOL_LONG_MAX LLONG_MAX
# define STRTOL_ULONG_MAX ULLONG_MAX
# if __GNUC__ == 2 && __GNUC_MINOR__ < 7
    
   static const unsigned long long int maxquad = ULLONG_MAX;
#  undef STRTOL_ULONG_MAX
#  define STRTOL_ULONG_MAX maxquad
# endif
#else
# define LONG long
# define STRTOL_LONG_MIN LONG_MIN
# define STRTOL_LONG_MAX LONG_MAX
# define STRTOL_ULONG_MAX ULONG_MAX
#endif


#ifdef USE_NUMBER_GROUPING
# define GROUP_PARAM_PROTO , int group
#else
# define GROUP_PARAM_PROTO
#endif

 
#ifdef USE_IN_EXTENDED_LOCALE_MODEL
# undef _NL_CURRENT
# define _NL_CURRENT(category, item) \
  (current->values[_NL_ITEM_INDEX (item)].string)
# define LOCALE_PARAM , loc
# define LOCALE_PARAM_PROTO , __locale_t loc
#else
# define LOCALE_PARAM
# define LOCALE_PARAM_PROTO
#endif

#ifdef USE_WIDE_CHAR
# include <wchar.h>
# include <wctype.h>
# define L_(Ch) L##Ch
# define UCHAR_TYPE wint_t
# define STRING_TYPE wchar_t
# ifdef USE_IN_EXTENDED_LOCALE_MODEL
#  define ISSPACE(Ch) __iswspace_l ((Ch), loc)
#  define ISALPHA(Ch) __iswalpha_l ((Ch), loc)
#  define TOUPPER(Ch) __towupper_l ((Ch), loc)
# else
#  define ISSPACE(Ch) iswspace (Ch)
#  define ISALPHA(Ch) iswalpha (Ch)
#  define TOUPPER(Ch) towupper (Ch)
# endif
#else
# define L_(Ch) Ch
# define UCHAR_TYPE unsigned char
# define STRING_TYPE char
# ifdef USE_IN_EXTENDED_LOCALE_MODEL
#  define ISSPACE(Ch) __isspace_l ((unsigned char) (Ch), loc)
#  define ISALPHA(Ch) __isalpha_l ((unsigned char) (Ch), loc)
#  define TOUPPER(Ch) __toupper_l ((unsigned char) (Ch), loc)
# else
#  define ISSPACE(Ch) isspace ((unsigned char) (Ch))
#  define ISALPHA(Ch) isalpha ((unsigned char) (Ch))
#  define TOUPPER(Ch) toupper ((unsigned char) (Ch))
# endif
#endif

#ifdef USE_NUMBER_GROUPING
# define INTERNAL(X) INTERNAL1(X)
# define INTERNAL1(X) __##X##_internal
# define WEAKNAME(X) WEAKNAME1(X)
#else
# define INTERNAL(X) X
#endif

#ifdef USE_NUMBER_GROUPING
 
# include "grouping.h"
#endif



 

INT
INTERNAL (strtol) (const STRING_TYPE *nptr, STRING_TYPE **endptr,
                   int base GROUP_PARAM_PROTO LOCALE_PARAM_PROTO)
{
  int negative;
  register unsigned LONG int cutoff;
  register unsigned int cutlim;
  register unsigned LONG int i;
  register const STRING_TYPE *s;
  register UCHAR_TYPE c;
  const STRING_TYPE *save, *end;
  int overflow;

#ifdef USE_NUMBER_GROUPING
# ifdef USE_IN_EXTENDED_LOCALE_MODEL
  struct locale_data *current = loc->__locales[LC_NUMERIC];
# endif
   
  wchar_t thousands = L'\0';
   
  const char *grouping;

  if (group)
    {
      grouping = _NL_CURRENT (LC_NUMERIC, GROUPING);
      if (*grouping <= 0 || *grouping == CHAR_MAX)
        grouping = NULL;
      else
        {
           
# if defined _LIBC || defined _HAVE_BTOWC
          thousands = __btowc (*_NL_CURRENT (LC_NUMERIC, THOUSANDS_SEP));
          if (thousands == WEOF)
            thousands = L'\0';
# endif
          if (thousands == L'\0')
            grouping = NULL;
        }
    }
  else
    grouping = NULL;
#endif

  if (base < 0 || base == 1 || base > 36)
    {
      __set_errno (EINVAL);
      return 0;
    }

  save = s = nptr;

   
  while (ISSPACE (*s))
    ++s;
  if (*s == L_('\0'))
    goto noconv;

   
  if (*s == L_('-'))
    {
      negative = 1;
      ++s;
    }
  else if (*s == L_('+'))
    {
      negative = 0;
      ++s;
    }
  else
    negative = 0;

   
  if (*s == L_('0'))
    {
      if ((base == 0 || base == 16) && TOUPPER (s[1]) == L_('X'))
        {
          s += 2;
          base = 16;
        }
      else if ((base == 0 || base == 2) && TOUPPER (s[1]) == L_('B'))
        {
          s += 2;
          base = 2;
        }
      else if (base == 0)
        base = 8;
    }
  else if (base == 0)
    base = 10;

   
  save = s;

#ifdef USE_NUMBER_GROUPING
  if (group)
    {
       
      end = s;
      for (c = *end; c != L_('\0'); c = *++end)
        if ((wchar_t) c != thousands
            && ((wchar_t) c < L_('0') || (wchar_t) c > L_('9'))
            && (!ISALPHA (c) || (int) (TOUPPER (c) - L_('A') + 10) >= base))
          break;
      if (*s == thousands)
        end = s;
      else
        end = correctly_grouped_prefix (s, end, thousands, grouping);
    }
  else
#endif
    end = NULL;

  cutoff = STRTOL_ULONG_MAX / (unsigned LONG int) base;
  cutlim = STRTOL_ULONG_MAX % (unsigned LONG int) base;

  overflow = 0;
  i = 0;
  for (c = *s; c != L_('\0'); c = *++s)
    {
      if (s == end)
        break;
      if (c >= L_('0') && c <= L_('9'))
        c -= L_('0');
      else if (ISALPHA (c))
        c = TOUPPER (c) - L_('A') + 10;
      else
        break;
      if ((int) c >= base)
        break;
       
      if (i > cutoff || (i == cutoff && c > cutlim))
        overflow = 1;
      else
        {
          i *= (unsigned LONG int) base;
          i += c;
        }
    }

   
  if (s == save)
    goto noconv;

   
  if (endptr != NULL)
    *endptr = (STRING_TYPE *) s;

#if !UNSIGNED
   
  if (overflow == 0
      && i > (negative
              ? -((unsigned LONG int) (STRTOL_LONG_MIN + 1)) + 1
              : (unsigned LONG int) STRTOL_LONG_MAX))
    overflow = 1;
#endif

  if (overflow)
    {
      __set_errno (ERANGE);
#if UNSIGNED
      return STRTOL_ULONG_MAX;
#else
      return negative ? STRTOL_LONG_MIN : STRTOL_LONG_MAX;
#endif
    }

   
  return negative ? -i : i;

noconv:
   
  if (endptr != NULL)
    {
      if (save - nptr >= 2
          && (TOUPPER (save[-1]) == L_('X') || TOUPPER (save[-1]) == L_('B'))
          && save[-2] == L_('0'))
        *endptr = (STRING_TYPE *) &save[-1];
      else
         
        *endptr = (STRING_TYPE *) nptr;
    }

  return 0L;
}

#ifdef USE_NUMBER_GROUPING
 

INT
# ifdef weak_function
weak_function
# endif
strtol (const STRING_TYPE *nptr, STRING_TYPE **endptr,
        int base LOCALE_PARAM_PROTO)
{
  return INTERNAL (strtol) (nptr, endptr, base, 0 LOCALE_PARAM);
}
#endif
