 
#include <wchar.h>

#include <stdio.h>
#include <stdlib.h>

int
wctob (wint_t wc)
{
  char buf[64];

  if (!(MB_CUR_MAX <= sizeof (buf)))
    abort ();
   
  if (wc == (wchar_t)wc)
    if (wctomb (buf, (wchar_t)wc) == 1)
      return (unsigned char) buf[0];
  return EOF;
}
