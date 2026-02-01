 
#include <wchar.h>

#include <errno.h>
#include <stdlib.h>


size_t
wcrtomb (char *s, wchar_t wc, mbstate_t *ps)
#undef wcrtomb
{
   
  if (ps != NULL && !mbsinit (ps))
    {
      errno = EINVAL;
      return (size_t)(-1);
    }

#if !HAVE_WCRTOMB                         \
    || WCRTOMB_RETVAL_BUG                 \
    || WCRTOMB_C_LOCALE_BUG              
  if (s == NULL)
     
    return 1;
  else
#endif
    {
#if HAVE_WCRTOMB
# if WCRTOMB_C_LOCALE_BUG                
       
      if (wc >= 0 && wc <= 0xff)
        {
          *s = (unsigned char) wc;
          return 1;
        }
      else
        {
          errno = EILSEQ;
          return (size_t)(-1);
        }
# else
      return wcrtomb (s, wc, ps);
# endif
#else                                    
       
      int ret = wctomb (s, wc);

      if (ret >= 0)
        return ret;
      else
        {
          errno = EILSEQ;
          return (size_t)(-1);
        }
#endif
    }
}
