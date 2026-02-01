 
#include <langinfo.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#if defined _WIN32 && ! defined __CYGWIN__
# define WIN32_LEAN_AND_MEAN   
# include <windows.h>
# include <stdio.h>
#endif

#if REPLACE_NL_LANGINFO && !NL_LANGINFO_MTSAFE
# if defined _WIN32 && !defined __CYGWIN__

#  define WIN32_LEAN_AND_MEAN   
#  include <windows.h>

# elif HAVE_PTHREAD_API

#  include <pthread.h>
#  if HAVE_THREADS_H && HAVE_WEAK_SYMBOLS
#   include <threads.h>
#   pragma weak thrd_exit
#   define c11_threads_in_use() (thrd_exit != NULL)
#  else
#   define c11_threads_in_use() 0
#  endif

# elif HAVE_THREADS_H

#  include <threads.h>

# endif
#endif

 

#if !REPLACE_NL_LANGINFO || GNULIB_defined_CODESET
 
static char *
ctype_codeset (void)
{
   
  static char result[2 + 10 + 1];
  char buf[2 + 10 + 1];
  char locale[SETLOCALE_NULL_MAX];
  char *codeset;
  size_t codesetlen;

  if (setlocale_null_r (LC_CTYPE, locale, sizeof (locale)))
    locale[0] = '\0';

  codeset = buf;
  codeset[0] = '\0';

  if (locale[0])
    {
       
      char *dot = strchr (locale, '.');

      if (dot)
        {
           
          char *codeset_start = dot + 1;
          char const *modifier = strchr (codeset_start, '@');

          if (! modifier)
            codeset = codeset_start;
          else
            {
              codesetlen = modifier - codeset_start;
              if (codesetlen < sizeof buf)
                {
                  codeset = memcpy (buf, codeset_start, codesetlen);
                  codeset[codesetlen] = '\0';
                }
            }
        }
    }

# if defined _WIN32 && ! defined __CYGWIN__
   
  codesetlen = strlen (codeset);
  if (0 < codesetlen && codesetlen < sizeof buf - 2)
    memmove (buf + 2, codeset, codesetlen + 1);
  else
    sprintf (buf + 2, "%u", GetACP ());
   
  if (strcmp (buf + 2, "65001") == 0 || strcmp (buf + 2, "utf8") == 0)
    return (char *) "UTF-8";
  else
    {
      memcpy (buf, "CP", 2);
      strcpy (result, buf);
      return result;
    }
# else
  strcpy (result, codeset);
  return result;
#endif
}
#endif


#if REPLACE_NL_LANGINFO

 

# undef nl_langinfo

 

# if !NL_LANGINFO_MTSAFE  

#  define ITEMS (MAXSTRMSG + 1)
#  define MAX_RESULT_LEN 80

static char *
nl_langinfo_unlocked (nl_item item)
{
  static char result[ITEMS][MAX_RESULT_LEN];

   
  char *tmp = nl_langinfo (item);
  if (item >= 0 && item < ITEMS && tmp != NULL)
    {
      size_t tmp_len = strlen (tmp);
      if (tmp_len < MAX_RESULT_LEN)
        strcpy (result[item], tmp);
      else
        {
           
          result[item][MAX_RESULT_LEN - 1] = '\0';
          memcpy (result[item], tmp, MAX_RESULT_LEN - 1);
        }
      return result[item];
    }
  else
    return tmp;
}

 

 
#  undef gl_get_nl_langinfo_lock

#  if defined _WIN32 && !defined __CYGWIN__

extern __declspec(dllimport) CRITICAL_SECTION *gl_get_nl_langinfo_lock (void);

static char *
nl_langinfo_with_lock (nl_item item)
{
  CRITICAL_SECTION *lock = gl_get_nl_langinfo_lock ();
  char *ret;

  EnterCriticalSection (lock);
  ret = nl_langinfo_unlocked (item);
  LeaveCriticalSection (lock);

  return ret;
}

#  elif HAVE_PTHREAD_API

extern
#   if defined _WIN32 || defined __CYGWIN__
  __declspec(dllimport)
#   endif
  pthread_mutex_t *gl_get_nl_langinfo_lock (void);

#   if HAVE_WEAK_SYMBOLS  

      
#    pragma weak pthread_mutex_lock
#    pragma weak pthread_mutex_unlock

      
#    pragma weak pthread_mutexattr_gettype
      
#    define pthread_in_use() \
       (pthread_mutexattr_gettype != NULL || c11_threads_in_use ())

#   else
#    define pthread_in_use() 1
#   endif

static char *
nl_langinfo_with_lock (nl_item item)
{
  if (pthread_in_use())
    {
      pthread_mutex_t *lock = gl_get_nl_langinfo_lock ();
      char *ret;

      if (pthread_mutex_lock (lock))
        abort ();
      ret = nl_langinfo_unlocked (item);
      if (pthread_mutex_unlock (lock))
        abort ();

      return ret;
    }
  else
    return nl_langinfo_unlocked (item);
}

#  elif HAVE_THREADS_H

extern mtx_t *gl_get_nl_langinfo_lock (void);

static char *
nl_langinfo_with_lock (nl_item item)
{
  mtx_t *lock = gl_get_nl_langinfo_lock ();
  char *ret;

  if (mtx_lock (lock) != thrd_success)
    abort ();
  ret = nl_langinfo_unlocked (item);
  if (mtx_unlock (lock) != thrd_success)
    abort ();

  return ret;
}

#  endif

# else

 
#  define nl_langinfo_with_lock nl_langinfo

# endif

char *
rpl_nl_langinfo (nl_item item)
{
  switch (item)
    {
# if GNULIB_defined_CODESET
    case CODESET:
      return ctype_codeset ();
# endif
# if GNULIB_defined_T_FMT_AMPM
    case T_FMT_AMPM:
      return (char *) "%I:%M:%S %p";
# endif
# if GNULIB_defined_ALTMON
    case ALTMON_1:
    case ALTMON_2:
    case ALTMON_3:
    case ALTMON_4:
    case ALTMON_5:
    case ALTMON_6:
    case ALTMON_7:
    case ALTMON_8:
    case ALTMON_9:
    case ALTMON_10:
    case ALTMON_11:
    case ALTMON_12:
       
      item = item - ALTMON_1 + MON_1;
      break;
# endif
# if GNULIB_defined_ERA
    case ERA:
       
      return (char *) "";
    case ERA_D_FMT:
       
      item = D_FMT;
      break;
    case ERA_D_T_FMT:
       
      item = D_T_FMT;
      break;
    case ERA_T_FMT:
       
      item = T_FMT;
      break;
    case ALT_DIGITS:
       
      return (char *) "\0\0\0\0\0\0\0\0\0\0";
# endif
# if GNULIB_defined_YESEXPR || !FUNC_NL_LANGINFO_YESEXPR_WORKS
    case YESEXPR:
      return (char *) "^[yY]";
    case NOEXPR:
      return (char *) "^[nN]";
# endif
    default:
      break;
    }
  return nl_langinfo_with_lock (item);
}

#else

 

# include <time.h>

char *
nl_langinfo (nl_item item)
{
  char buf[100];
  struct tm tmm = { 0 };

  switch (item)
    {
     
    case CODESET:
      {
        char *codeset = ctype_codeset ();
        if (*codeset)
          return codeset;
      }
# ifdef __BEOS__
      return (char *) "UTF-8";
# else
      return (char *) "ISO-8859-1";
# endif
     
    case RADIXCHAR:
      return localeconv () ->decimal_point;
    case THOUSEP:
      return localeconv () ->thousands_sep;
# ifdef GROUPING
    case GROUPING:
      return localeconv () ->grouping;
# endif
     
    case D_T_FMT:
    case ERA_D_T_FMT:
      return (char *) "%a %b %e %H:%M:%S %Y";
    case D_FMT:
    case ERA_D_FMT:
      return (char *) "%m/%d/%y";
    case T_FMT:
    case ERA_T_FMT:
      return (char *) "%H:%M:%S";
    case T_FMT_AMPM:
      return (char *) "%I:%M:%S %p";
    case AM_STR:
      {
        static char result[80];
        if (!strftime (buf, sizeof result, "%p", &tmm))
          return (char *) "AM";
        strcpy (result, buf);
        return result;
      }
    case PM_STR:
      {
        static char result[80];
        tmm.tm_hour = 12;
        if (!strftime (buf, sizeof result, "%p", &tmm))
          return (char *) "PM";
        strcpy (result, buf);
        return result;
      }
    case DAY_1:
    case DAY_2:
    case DAY_3:
    case DAY_4:
    case DAY_5:
    case DAY_6:
    case DAY_7:
      {
        static char result[7][50];
        static char const days[][sizeof "Wednesday"] = {
          "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
          "Friday", "Saturday"
        };
        tmm.tm_wday = item - DAY_1;
        if (!strftime (buf, sizeof result[0], "%A", &tmm))
          return (char *) days[item - DAY_1];
        strcpy (result[item - DAY_1], buf);
        return result[item - DAY_1];
      }
    case ABDAY_1:
    case ABDAY_2:
    case ABDAY_3:
    case ABDAY_4:
    case ABDAY_5:
    case ABDAY_6:
    case ABDAY_7:
      {
        static char result[7][30];
        static char const abdays[][sizeof "Sun"] = {
          "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
        };
        tmm.tm_wday = item - ABDAY_1;
        if (!strftime (buf, sizeof result[0], "%a", &tmm))
          return (char *) abdays[item - ABDAY_1];
        strcpy (result[item - ABDAY_1], buf);
        return result[item - ABDAY_1];
      }
    {
      static char const months[][sizeof "September"] = {
        "January", "February", "March", "April", "May", "June", "July",
        "September", "October", "November", "December"
      };
      case MON_1:
      case MON_2:
      case MON_3:
      case MON_4:
      case MON_5:
      case MON_6:
      case MON_7:
      case MON_8:
      case MON_9:
      case MON_10:
      case MON_11:
      case MON_12:
        {
          static char result[12][50];
          tmm.tm_mon = item - MON_1;
          if (!strftime (buf, sizeof result[0], "%B", &tmm))
            return (char *) months[item - MON_1];
          strcpy (result[item - MON_1], buf);
          return result[item - MON_1];
        }
      case ALTMON_1:
      case ALTMON_2:
      case ALTMON_3:
      case ALTMON_4:
      case ALTMON_5:
      case ALTMON_6:
      case ALTMON_7:
      case ALTMON_8:
      case ALTMON_9:
      case ALTMON_10:
      case ALTMON_11:
      case ALTMON_12:
        {
          static char result[12][50];
          tmm.tm_mon = item - ALTMON_1;
           
          #if 0
          if (!strftime (buf, sizeof result[0], "%OB", &tmm))
          #endif
            if (!strftime (buf, sizeof result[0], "%B", &tmm))
              return (char *) months[item - ALTMON_1];
          strcpy (result[item - ALTMON_1], buf);
          return result[item - ALTMON_1];
        }
    }
    case ABMON_1:
    case ABMON_2:
    case ABMON_3:
    case ABMON_4:
    case ABMON_5:
    case ABMON_6:
    case ABMON_7:
    case ABMON_8:
    case ABMON_9:
    case ABMON_10:
    case ABMON_11:
    case ABMON_12:
      {
        static char result[12][30];
        static char const abmonths[][sizeof "Jan"] = {
          "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
          "Sep", "Oct", "Nov", "Dec"
        };
        tmm.tm_mon = item - ABMON_1;
        if (!strftime (buf, sizeof result[0], "%b", &tmm))
          return (char *) abmonths[item - ABMON_1];
        strcpy (result[item - ABMON_1], buf);
        return result[item - ABMON_1];
      }
    case ERA:
      return (char *) "";
    case ALT_DIGITS:
      return (char *) "\0\0\0\0\0\0\0\0\0\0";
     
    case CRNCYSTR:
      return localeconv () ->currency_symbol;
# ifdef INT_CURR_SYMBOL
    case INT_CURR_SYMBOL:
      return localeconv () ->int_curr_symbol;
    case MON_DECIMAL_POINT:
      return localeconv () ->mon_decimal_point;
    case MON_THOUSANDS_SEP:
      return localeconv () ->mon_thousands_sep;
    case MON_GROUPING:
      return localeconv () ->mon_grouping;
    case POSITIVE_SIGN:
      return localeconv () ->positive_sign;
    case NEGATIVE_SIGN:
      return localeconv () ->negative_sign;
    case FRAC_DIGITS:
      return & localeconv () ->frac_digits;
    case INT_FRAC_DIGITS:
      return & localeconv () ->int_frac_digits;
    case P_CS_PRECEDES:
      return & localeconv () ->p_cs_precedes;
    case N_CS_PRECEDES:
      return & localeconv () ->n_cs_precedes;
    case P_SEP_BY_SPACE:
      return & localeconv () ->p_sep_by_space;
    case N_SEP_BY_SPACE:
      return & localeconv () ->n_sep_by_space;
    case P_SIGN_POSN:
      return & localeconv () ->p_sign_posn;
    case N_SIGN_POSN:
      return & localeconv () ->n_sign_posn;
# endif
     
    case YESEXPR:
      return (char *) "^[yY]";
    case NOEXPR:
      return (char *) "^[nN]";
    default:
      return (char *) "";
    }
}

#endif
