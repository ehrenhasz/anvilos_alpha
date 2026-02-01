 

 

#include "config.h"

#include "bashtypes.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "bashansi.h"
#include <stdio.h>
#include "chartypes.h"

#include "shell.h"
#include "pathexp.h"

#include <glob/glob.h>

#if defined (EXTENDED_GLOB)
#  include <glob/strmatch.h>
#endif

 
 
 
 
 

 
int
find_string_in_alist (string, alist, flags)
     char *string;
     STRING_INT_ALIST *alist;
     int flags;
{
  register int i;
  int r;

  for (i = r = 0; alist[i].word; i++)
    {
#if defined (EXTENDED_GLOB)
      if (flags)
	r = strmatch (alist[i].word, string, FNM_EXTMATCH) != FNM_NOMATCH;
      else
#endif
	r = STREQ (string, alist[i].word);

      if (r)
	return (alist[i].token);
    }
  return -1;
}

 
char *
find_token_in_alist (token, alist, flags)
     int token;
     STRING_INT_ALIST *alist;
     int flags;
{
  register int i;

  for (i = 0; alist[i].word; i++)
    {
      if (alist[i].token == token)
        return (savestring (alist[i].word));
    }
  return ((char *)NULL);
}

int
find_index_in_alist (string, alist, flags)
     char *string;
     STRING_INT_ALIST *alist;
     int flags;
{
  register int i;
  int r;

  for (i = r = 0; alist[i].word; i++)
    {
#if defined (EXTENDED_GLOB)
      if (flags)
	r = strmatch (alist[i].word, string, FNM_EXTMATCH) != FNM_NOMATCH;
      else
#endif
	r = STREQ (string, alist[i].word);

      if (r)
	return (i);
    }

  return -1;
}

 
 
 
 
 

 
char *
substring (string, start, end)
     const char *string;
     int start, end;
{
  register int len;
  register char *result;

  len = end - start;
  result = (char *)xmalloc (len + 1);
  memcpy (result, string + start, len);
  result[len] = '\0';
  return (result);
}

 
char *
strsub (string, pat, rep, global)
     char *string, *pat, *rep;
     int global;
{
  size_t patlen, replen, templen, tempsize, i;
  int repl;
  char *temp, *r;

  patlen = strlen (pat);
  replen = strlen (rep);
  for (temp = (char *)NULL, i = templen = tempsize = 0, repl = 1; string[i]; )
    {
      if (repl && STREQN (string + i, pat, patlen))
	{
	  if (replen)
	    RESIZE_MALLOCED_BUFFER (temp, templen, replen, tempsize, (replen * 2));

	  for (r = rep; *r; )	 
	    temp[templen++] = *r++;

	  i += patlen ? patlen : 1;	 
	  repl = global != 0;
	}
      else
	{
	  RESIZE_MALLOCED_BUFFER (temp, templen, 1, tempsize, 16);
	  temp[templen++] = string[i++];
	}
    }
  if (temp)
    temp[templen] = 0;
  else
    temp = savestring (string);
  return (temp);
}

 
char *
strcreplace (string, c, text, flags)
     char *string;
     int c;
     const char *text;
     int flags;
{
  char *ret, *p, *r, *t;
  size_t len, rlen, ind, tlen;
  int do_glob, escape_backslash;

  do_glob = flags & 1;
  escape_backslash = flags & 2;

  len = STRLEN (text);
  rlen = len + strlen (string) + 2;
  ret = (char *)xmalloc (rlen);

  for (p = string, r = ret; p && *p; )
    {
      if (*p == c)
	{
	  if (len)
	    {
	      ind = r - ret;
	      if (do_glob && (glob_pattern_p (text) || strchr (text, '\\')))
		{
		  t = quote_globbing_chars (text);
		  tlen = strlen (t);
		  RESIZE_MALLOCED_BUFFER (ret, ind, tlen, rlen, rlen);
		  r = ret + ind;	 
		  strcpy (r, t);
		  r += tlen;
		  free (t);
		}
	      else
		{
		  RESIZE_MALLOCED_BUFFER (ret, ind, len, rlen, rlen);
		  r = ret + ind;	 
		  strcpy (r, text);
		  r += len;
		}
	    }
	  p++;
	  continue;
	}

      if (*p == '\\' && p[1] == c)
	p++;
      else if (escape_backslash && *p == '\\' && p[1] == '\\')
	p++;

      ind = r - ret;
      RESIZE_MALLOCED_BUFFER (ret, ind, 2, rlen, rlen);
      r = ret + ind;			 
      *r++ = *p++;
    }
  *r = '\0';

  return ret;
}

#ifdef INCLUDE_UNUSED
 
void
strip_leading (string)
     char *string;
{
  char *start = string;

  while (*string && (whitespace (*string) || *string == '\n'))
    string++;

  if (string != start)
    {
      int len = strlen (string);
      FASTCOPY (string, start, len);
      start[len] = '\0';
    }
}
#endif

 
void
strip_trailing (string, len, newlines_only)
     char *string;
     int len;
     int newlines_only;
{
  while (len >= 0)
    {
      if ((newlines_only && string[len] == '\n') ||
	  (!newlines_only && whitespace (string[len])))
	len--;
      else
	break;
    }
  string[len + 1] = '\0';
}

 
void
xbcopy (s, d, n)
     char *s, *d;
     int n;
{
  FASTCOPY (s, d, n);
}
