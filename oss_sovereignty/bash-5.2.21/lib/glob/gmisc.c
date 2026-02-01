 

#include <config.h>

#include "bashtypes.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "bashansi.h"
#include "shmbutil.h"
#include "chartypes.h"

#include "stdc.h"

#ifndef FNM_CASEFOLD
#  include "strmatch.h"
#endif
#include "glob.h"

 
extern char *glob_patscan PARAMS((char *, char *, int));

 
#define CHAR	char
#define INT	int
#define L(CS)	CS
#define EXTGLOB_PATTERN_P extglob_pattern_p
#define MATCH_PATTERN_CHAR match_pattern_char
#define MATCHLEN umatchlen
#define FOLD(c) ((flags & FNM_CASEFOLD) \
	? TOLOWER ((unsigned char)c) \
	: ((unsigned char)c))
#ifndef LPAREN
#define LPAREN '('
#define RPAREN ')'
#endif
#include "gm_loop.c"

 
#if HANDLE_MULTIBYTE

#define CHAR	wchar_t
#define INT	wint_t
#define L(CS)	L##CS
#define EXTGLOB_PATTERN_P wextglob_pattern_p
#define MATCH_PATTERN_CHAR match_pattern_wchar
#define MATCHLEN wmatchlen

#define FOLD(c) ((flags & FNM_CASEFOLD) && iswupper (c) ? towlower (c) : (c))
#define LPAREN L'('
#define RPAREN L')'
#include "gm_loop.c"

#endif  


#if defined (EXTENDED_GLOB)
 
char *
glob_dirscan (pat, dirsep)
     char *pat;
     int dirsep;
{
  char *p, *d, *pe, *se;

  d = pe = se = 0;
  for (p = pat; p && *p; p++)
    {
      if (extglob_pattern_p (p))
	{
	  if (se == 0)
	    se = p + strlen (p) - 1;
	  pe = glob_patscan (p + 2, se, 0);
	  if (pe == 0)
	    continue;
	  else if (*pe == 0)
	    break;
	  p = pe - 1;	 
	  continue;
	}
      if (*p ==  dirsep)
	d = p;
    }
  return d;
}
#endif  
