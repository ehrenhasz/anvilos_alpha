 

#include <config.h>

 
#include <uchar.h>

#include "attribute.h"

#include <errno.h>
#include <stdlib.h>

#if GL_CHAR32_T_IS_UNICODE
# include "lc-charset-unicode.h"
#endif

#if GNULIB_defined_mbstate_t  
 

 

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

# include "lc-charset-dispatch.h"
# include "mbtowc-lock.h"

static_assert (sizeof (mbstate_t) >= 4);
static char internal_state[4];

size_t
mbrtoc32 (char32_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
# define FITS_IN_CHAR_TYPE(wc)  1
# include "mbrtowc-impl.h"
}

#else  

 

# include <wchar.h>

# include "localcharset.h"
# include "streq.h"

# if MBRTOC32_IN_C_LOCALE_MAYBE_EILSEQ
#  include "hard-locale.h"
#  include <locale.h>
# endif

static mbstate_t internal_state;

size_t
mbrtoc32 (char32_t *pwc, const char *s, size_t n, mbstate_t *ps)
# undef mbrtoc32
{
   
  if (s == NULL)
    {
      pwc = NULL;
      s = "";
      n = 1;
    }

# if MBRTOC32_EMPTY_INPUT_BUG || _GL_SMALL_WCHAR_T
  if (n == 0)
    return (size_t) -2;
# endif

  if (ps == NULL)
    ps = &internal_state;

# if HAVE_WORKING_MBRTOC32
   

#  if defined _WIN32 && !defined __CYGWIN__
  char32_t wc;
  size_t ret = mbrtoc32 (&wc, s, n, ps);
  if (ret < (size_t) -2 && pwc != NULL)
    *pwc = wc;
#  else
  size_t ret = mbrtoc32 (pwc, s, n, ps);
#  endif

#  if GNULIB_MBRTOC32_REGULAR
   
  if (ret < (size_t) -3 && ! mbsinit (ps))
     
    mbszero (ps);
  if (ret == (size_t) -3)
    abort ();
#  endif

#  if MBRTOC32_IN_C_LOCALE_MAYBE_EILSEQ
  if ((size_t) -2 <= ret && n != 0 && ! hard_locale (LC_CTYPE))
    {
      if (pwc != NULL)
        *pwc = (unsigned char) *s;
      return 1;
    }
#  endif

  return ret;

# elif _GL_SMALL_WCHAR_T

   
  const char *encoding = locale_charset ();
  if (STREQ_OPT (encoding, "UTF-8", 'U', 'T', 'F', '-', '8', 0, 0, 0, 0))
    {
       
       
      char *pstate = (char *)ps;
      size_t nstate = pstate[0];
      char buf[4];
      const char *p;
      size_t m;
      int res;

      switch (nstate)
        {
        case 0:
          p = s;
          m = n;
          break;
        case 3:
          buf[2] = pstate[3];
          FALLTHROUGH;
        case 2:
          buf[1] = pstate[2];
          FALLTHROUGH;
        case 1:
          buf[0] = pstate[1];
          p = buf;
          m = nstate;
          buf[m++] = s[0];
          if (n >= 2 && m < 4)
            {
              buf[m++] = s[1];
              if (n >= 3 && m < 4)
                buf[m++] = s[2];
            }
          break;
        default:
          errno = EINVAL;
          return (size_t)(-1);
        }

       

      {
#  define FITS_IN_CHAR_TYPE(wc)  1
#  include "mbrtowc-impl-utf8.h"
      }

     success:
      if (nstate >= (res > 0 ? res : 1))
        abort ();
      res -= nstate;
       
#  if defined _WIN32 && !defined __CYGWIN__
       
       
      *(unsigned int *)pstate = 0;
#  elif defined __CYGWIN__
       
      ps->__count = 0;
#  else
      pstate[0] = 0;
#  endif
      return res;

     incomplete:
      {
        size_t k = nstate;
         
        pstate[++k] = s[0];
        if (k < m)
          {
            pstate[++k] = s[1];
            if (k < m)
              pstate[++k] = s[2];
          }
        if (k != m)
          abort ();
      }
      pstate[0] = m;
      return (size_t)(-2);

     invalid:
      errno = EILSEQ;
       
      return (size_t)(-1);
    }
  else
    {
      wchar_t wc;
      size_t ret = mbrtowc (&wc, s, n, ps);
      if (ret < (size_t) -2 && pwc != NULL)
        *pwc = wc;
      return ret;
    }

# else

   
  wchar_t wc;
  size_t ret = mbrtowc (&wc, s, n, ps);

#  if GNULIB_MBRTOC32_REGULAR
   
  if (ret < (size_t) -2 && ! mbsinit (ps))
     
    mbszero (ps);
#  endif

#  if GL_CHAR32_T_IS_UNICODE && GL_CHAR32_T_VS_WCHAR_T_NEEDS_CONVERSION
  if (ret < (size_t) -2 && wc != 0)
    {
      wc = locale_encoding_to_unicode (wc);
      if (wc == 0)
        {
          ret = (size_t) -1;
          errno = EILSEQ;
        }
    }
#  endif
  if (ret < (size_t) -2 && pwc != NULL)
    *pwc = wc;
  return ret;

# endif
}

#endif
