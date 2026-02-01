 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <stdio.h>
#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#if defined (HAVE_FNMATCH)
#  include <fnmatch.h>
#endif

#include "history.h"
#include "histlib.h"
#include "xmalloc.h"

 
char *history_search_delimiter_chars = (char *)NULL;

static int history_search_internal (const char *, int, int);

 

static int
history_search_internal (const char *string, int direction, int flags)
{
  register int i, reverse;
  register char *line;
  register int line_index;
  int string_len, anchored, patsearch;
  HIST_ENTRY **the_history; 	 

  i = history_offset;
  reverse = (direction < 0);
  anchored = (flags & ANCHORED_SEARCH);
#if defined (HAVE_FNMATCH)
  patsearch = (flags & PATTERN_SEARCH);
#else
  patsearch = 0;
#endif

   
  if (string == 0 || *string == '\0')
    return (-1);

  if (!history_length || ((i >= history_length) && !reverse))
    return (-1);

  if (reverse && (i >= history_length))
    i = history_length - 1;

#define NEXT_LINE() do { if (reverse) i--; else i++; } while (0)

  the_history = history_list ();
  string_len = strlen (string);
  while (1)
    {
       

       
      if ((reverse && i < 0) || (!reverse && i == history_length))
	return (-1);

      line = the_history[i]->line;
      line_index = strlen (line);

       
      if (patsearch == 0 && (string_len > line_index))
	{
	  NEXT_LINE ();
	  continue;
	}

       
      if (anchored == ANCHORED_SEARCH)
	{
#if defined (HAVE_FNMATCH)
	  if (patsearch)
	    {
	      if (fnmatch (string, line, 0) == 0)
		{
		  history_offset = i;
		  return (0);
		}
	    }
	  else
#endif
	  if (STREQN (string, line, string_len))
	    {
	      history_offset = i;
	      return (0);
	    }

	  NEXT_LINE ();
	  continue;
	}

       
      if (reverse)
	{
	  line_index -= (patsearch == 0) ? string_len : 1;

	  while (line_index >= 0)
	    {
#if defined (HAVE_FNMATCH)
	      if (patsearch)
		{
		  if (fnmatch (string, line + line_index, 0) == 0)
		    {
		      history_offset = i;
		      return (line_index);
		    }
		}
	      else
#endif
	      if (STREQN (string, line + line_index, string_len))
		{
		  history_offset = i;
		  return (line_index);
		}
	      line_index--;
	    }
	}
      else
	{
	  register int limit;

	  limit = line_index - string_len + 1;
	  line_index = 0;

	  while (line_index < limit)
	    {
#if defined (HAVE_FNMATCH)
	      if (patsearch)
		{
		  if (fnmatch (string, line + line_index, 0) == 0)
		    {
		      history_offset = i;
		      return (line_index);
		    }
		}
	      else
#endif
	      if (STREQN (string, line + line_index, string_len))
		{
		  history_offset = i;
		  return (line_index);
		}
	      line_index++;
	    }
	}
      NEXT_LINE ();
    }
}

int
_hs_history_patsearch (const char *string, int direction, int flags)
{
  char *pat;
  size_t len, start;
  int ret, unescaped_backslash;

#if defined (HAVE_FNMATCH)
   
  len = strlen (string);
  ret = len - 1;
   
  if (unescaped_backslash = (string[ret] == '\\'))
    {
      while (ret > 0 && string[--ret] == '\\')
	unescaped_backslash = 1 - unescaped_backslash;
    }
  if (unescaped_backslash)
    return -1;
  pat = (char *)xmalloc (len + 3);
   
  if ((flags & ANCHORED_SEARCH) == 0 && string[0] != '*')
    {
      pat[0] = '*';
      start = 1;
      len++;
    }
  else
    {
      start = 0;
    }

   
  strcpy (pat + start, string);
  if (pat[len - 1] != '*')
    {
      pat[len] = '*';		 
      pat[len+1] = '\0';
    }
#else
  pat = string;
#endif

  ret = history_search_internal (pat, direction, flags|PATTERN_SEARCH);

  if (pat != string)
    xfree (pat);
  return ret;
}
	
 
int
history_search (const char *string, int direction)
{
  return (history_search_internal (string, direction, NON_ANCHORED_SEARCH));
}

 
int
history_search_prefix (const char *string, int direction)
{
  return (history_search_internal (string, direction, ANCHORED_SEARCH));
}

 
int
history_search_pos (const char *string, int dir, int pos)
{
  int ret, old;

  old = where_history ();
  history_set_pos (pos);
  if (history_search (string, dir) == -1)
    {
      history_set_pos (old);
      return (-1);
    }
  ret = where_history ();
  history_set_pos (old);
  return ret;
}
