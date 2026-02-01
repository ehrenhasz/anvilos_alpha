 
#include <wchar.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

wint_t
btowc (int c)
{
  if (c != EOF)
    {
      char buf[1];
      wchar_t wc;

      buf[0] = c;
#if HAVE_MBRTOWC
      mbstate_t state;
      mbszero (&state);
      size_t ret = mbrtowc (&wc, buf, 1, &state);
      if (!(ret == (size_t)(-1) || ret == (size_t)(-2)))
#else
      if (mbtowc (&wc, buf, 1) >= 0)
#endif
        return wc;
    }
  return WEOF;
}
