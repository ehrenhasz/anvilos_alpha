 

 

#include <config.h>

#include "stdc.h"
#include "strmatch.h"

extern int xstrmatch PARAMS((char *, char *, int));
#if defined (HANDLE_MULTIBYTE)
extern int internal_wstrmatch PARAMS((wchar_t *, wchar_t *, int));
#endif

int
strmatch (pattern, string, flags)
     char *pattern;
     char *string;
     int flags;
{
  if (string == 0 || pattern == 0)
    return FNM_NOMATCH;

  return (xstrmatch (pattern, string, flags));
}

#if defined (HANDLE_MULTIBYTE)
int
wcsmatch (wpattern, wstring, flags)
     wchar_t *wpattern;
     wchar_t *wstring;
     int flags;
{
  if (wstring == 0 || wpattern == 0)
    return (FNM_NOMATCH);

  return (internal_wstrmatch (wpattern, wstring, flags));
}
#endif

#ifdef TEST
main (c, v)
     int c;
     char **v;
{
  char *string, *pat;

  string = v[1];
  pat = v[2];

  if (strmatch (pat, string, 0) == 0)
    {
      printf ("%s matches %s\n", string, pat);
      exit (0);
    }
  else
    {
      printf ("%s does not match %s\n", string, pat);
      exit (1);
    }
}
#endif
