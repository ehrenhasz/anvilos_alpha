 

 

 
#include <config.h>

#include <unistd.h>

#include "bashansi.h"
#include <stdio.h>

#include "chartypes.h"
#include "bashintl.h"
#include "shmbutil.h"

#define SAFECHAR(c)   ((c) == ' ' || (c) == '\t')

#ifndef RUBOUT
#define RUBOUT 0x7f
#endif

#ifndef CTRL_CHAR
#define CTRL_CHAR(c)	((c) < 0x20)
#endif

#ifndef META_CHAR
#define META_CHAR(c)	((c) > 0x7f && (c) <= UCHAR_MAX)
#endif

#ifndef UNCTRL
#define UNCTRL(c)	(TOUPPER ((c) | 0x40))
#endif

#ifndef UNMETA
#define UNMETA(c)	((c) & 0x7f)
#endif

int
sh_charvis (s, sindp, slen, ret, rindp)
     const char *s;
     size_t *sindp;
     size_t slen;
     char *ret;
     size_t *rindp;
{
  unsigned char c;
  size_t si, ri;
  const char *send;
  DECLARE_MBSTATE;

  si = *sindp;
  ri = *rindp;
  c = s[*sindp];

#if defined (HANDLE_MULTIBYTE)
  send = (locale_mb_cur_max > 1) ? s + slen : 0;
#else
  send = 0;
#endif

  if (SAFECHAR (c))
    {
      ret[ri++] = c;
      si++;
    }
  else if (c == RUBOUT)
    {
      ret[ri++] = '^';
      ret[ri++] = '?';
      si++;
    }
  else if (CTRL_CHAR (c))
    {
      ret[ri++] = '^';
      ret[ri++] = UNCTRL (c);
      si++;
    }
#if defined (HANDLE_MULTIBYTE)
  else if (locale_utf8locale && (c & 0x80))
    COPY_CHAR_I (ret, ri, s, send, si);
  else if (locale_mb_cur_max > 1 && is_basic (c) == 0)
    COPY_CHAR_I (ret, ri, s, send, si);
#endif
  else if (META_CHAR (c))
    {
      ret[ri++] = 'M';
      ret[ri++] = '-';
      ret[ri++] = UNMETA (c);
      si++;
    }
  else
    ret[ri++] = s[si++];
  
  *sindp = si;
  *rindp = ri;

  return si;    
}

 
char *
sh_strvis (string)
     const char *string;
{
  size_t slen, sind;
  char *ret;
  size_t retind, retsize;
  unsigned char c;
  DECLARE_MBSTATE;

  if (string == 0)
    return 0;
  if (*string == '\0')
    {
      if ((ret = (char *)malloc (1)) == 0)
	return 0;
      ret[0] = '\0';
      return ret;
    }

  slen = strlen (string);
  retsize = 3 * slen + 1;

  ret = (char *)malloc (retsize);
  if (ret == 0)
    return 0;

  retind = 0;
  sind = 0;

  while (string[sind])
    sind = sh_charvis (string, &sind, slen, ret, &retind);

  ret[retind] = '\0';
  return ret;
}
