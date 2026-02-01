 

 

#include "config.h"

#include "bashtypes.h"
#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "bashansi.h"

#include "shell.h"
#include "pathexp.h"
#include "flags.h"

#include "shmbutil.h"
#include "bashintl.h"

#include <glob/strmatch.h>

static int glob_name_is_acceptable PARAMS((const char *));
static void ignore_globbed_names PARAMS((char **, sh_ignore_func_t *));
static char *split_ignorespec PARAMS((char *, int *));
	       
#include <glob/glob.h>

 
int glob_dot_filenames;

 
int extended_glob = EXTGLOB_DEFAULT;

 
int glob_star = 0;

 
int
unquoted_glob_pattern_p (string)
     register char *string;
{
  register int c;
  char *send;
  int open, bsquote;

  DECLARE_MBSTATE;

  open = bsquote = 0;
  send = string + strlen (string);

  while (c = *string++)
    {
      switch (c)
	{
	case '?':
	case '*':
	  return (1);

	case '[':
	  open++;
	  continue;

	case ']':
	  if (open)		 
	    return (1);
	  continue;

	case '/':
	  if (open)
	    open = 0;

	case '+':
	case '@':
	case '!':
	  if (*string == '(')	 
	    return (1);
	  continue;

	 
	case '\\':
	  if (*string != '\0' && *string != '/')
	    {
	      bsquote = 1;
	      string++;
	      continue;
	    }
	  else if (open && *string == '/')
	    {
	      string++;		 
	      continue;
	    }
	  else if (*string == 0)
	    return (0);
	 	  
	case CTLESC:
	  if (*string++ == '\0')
	    return (0);
	}

       
#ifdef HANDLE_MULTIBYTE
      string--;
      ADVANCE_CHAR_P (string, send - string);
      string++;
#else
      ADVANCE_CHAR_P (string, send - string);
#endif
    }

#if 0
  return (bsquote ? 2 : 0);
#else
  return (0);
#endif
}

 
static inline int
ere_char (c)
     int c;
{
  switch (c)
    {
    case '.':
    case '[':
    case '\\':
    case '(':
    case ')':
    case '*':
    case '+':
    case '?':
    case '{':
    case '|':
    case '^':
    case '$':
      return 1;
    default: 
      return 0;
    }
  return (0);
}

 
int
glob_char_p (s)
     const char *s;
{
  switch (*s)
    {
    case '*':
    case '[':
    case ']':
    case '?':
    case '\\':
      return 1;
    case '+':
    case '@':
    case '!':
      if (s[1] == '(')	 
	return 1;
      break;
    }
  return 0;
}

 
char *
quote_string_for_globbing (pathname, qflags)
     const char *pathname;
     int qflags;
{
  char *temp;
  register int i, j;
  int cclass, collsym, equiv, c, last_was_backslash;
  int savei, savej;

  temp = (char *)xmalloc (2 * strlen (pathname) + 1);

  if ((qflags & QGLOB_CVTNULL) && QUOTED_NULL (pathname))
    {
      temp[0] = '\0';
      return temp;
    }

  cclass = collsym = equiv = last_was_backslash = 0;
  for (i = j = 0; pathname[i]; i++)
    {
       
      if (pathname[i] == CTLESC && pathname[i+1] == '\0')
	{
	  temp[j++] = pathname[i++];
	  break;
	}
       
      else if ((qflags & (QGLOB_REGEXP|QGLOB_CTLESC)) && pathname[i] == CTLESC && (pathname[i+1] == CTLESC || pathname[i+1] == CTLNUL))
	{
	  i++;
	  temp[j++] = pathname[i];
	  continue;
	}
      else if (pathname[i] == CTLESC)
	{
convert_to_backslash:
	  if ((qflags & QGLOB_FILENAME) && pathname[i+1] == '/')
	    continue;
	   
	  if (pathname[i+1] != CTLESC && (qflags & QGLOB_REGEXP) && ere_char (pathname[i+1]) == 0)
	    continue;
	  temp[j++] = '\\';
	  i++;
	  if (pathname[i] == '\0')
	    break;
	}
      else if ((qflags & QGLOB_REGEXP) && (i == 0 || pathname[i-1] != CTLESC) && pathname[i] == '[')	 
	{
	  temp[j++] = pathname[i++];	 
	  savej = j;
	  savei = i;
	  c = pathname[i++];	 
	  if (c == '^')		 
	    {
	      temp[j++] = c;
	      c = pathname[i++];
	    }
	  if (c == ']')		 
	    {
	      temp[j++] = c;
	      c = pathname[i++];
	    }
	  do
	    {
	      if (c == 0)
		goto endpat;
	      else if (c == CTLESC)
		{
		   
		   
		  if (pathname[i] == 0)
		    goto endpat;
		  temp[j++] = pathname[i++];
		}
	      else if (c == '[' && pathname[i] == ':')
		{
		  temp[j++] = c;
		  temp[j++] = pathname[i++];
		  cclass = 1;
		}
	      else if (cclass && c == ':' && pathname[i] == ']')
		{
		  temp[j++] = c;
		  temp[j++] = pathname[i++];
		  cclass = 0;
		}
	      else if (c == '[' && pathname[i] == '=')
		{
		  temp[j++] = c;
		  temp[j++] = pathname[i++];
		  if (pathname[i] == ']')
		    temp[j++] = pathname[i++];		 
		  equiv = 1;
		}
	      else if (equiv && c == '=' && pathname[i] == ']')
		{
		  temp[j++] = c;
		  temp[j++] = pathname[i++];
		  equiv = 0;
		}
	      else if (c == '[' && pathname[i] == '.')
		{
		  temp[j++] = c;
		  temp[j++] = pathname[i++];
		  if (pathname[i] == ']')
		    temp[j++] = pathname[i++];		 
		  collsym = 1;
		}
	      else if (collsym && c == '.' && pathname[i] == ']')
		{
		  temp[j++] = c;
		  temp[j++] = pathname[i++];
		  collsym = 0;
		}
	      else
		temp[j++] = c;
	    }
	  while (((c = pathname[i++]) != ']') && c != 0);

	   
	  if (c == 0)
	    {
	      i = savei - 1;	 
	      j = savej;
	      continue;
	    }

	  temp[j++] = c;	 
	  i--;			 
	  continue;		 
	}
      else if (pathname[i] == '\\' && (qflags & QGLOB_REGEXP) == 0)
	{
	   

	   
	  temp[j++] = '\\';

	  i++;
	  if (pathname[i] == '\0')
	    break;
	   
	  if ((qflags & QGLOB_CTLESC) && pathname[i] == CTLESC && (pathname[i+1] == CTLESC || pathname[i+1] == CTLNUL))
	    i++;	 
	  else if ((qflags & QGLOB_CTLESC) && pathname[i] == CTLESC)
	     
	    goto convert_to_backslash;
	}
      else if (pathname[i] == '\\' && (qflags & QGLOB_REGEXP))
        last_was_backslash = 1;
      temp[j++] = pathname[i];
    }
endpat:
  temp[j] = '\0';

  return (temp);
}

char *
quote_globbing_chars (string)
     const char *string;
{
  size_t slen;
  char *temp, *t;
  const char *s, *send;
  DECLARE_MBSTATE;

  slen = strlen (string);
  send = string + slen;

  temp = (char *)xmalloc (slen * 2 + 1);
  for (t = temp, s = string; *s; )
    {
      if (glob_char_p (s))
	*t++ = '\\';

       
      COPY_CHAR_P (t, s, send);
    }
  *t = '\0';
  return temp;
}

 
char **
shell_glob_filename (pathname, qflags)
     const char *pathname;
     int qflags;
{
  char *temp, **results;
  int gflags, quoted_pattern;

  noglob_dot_filenames = glob_dot_filenames == 0;

  temp = quote_string_for_globbing (pathname, QGLOB_FILENAME|qflags);
  gflags = glob_star ? GX_GLOBSTAR : 0;
  results = glob_filename (temp, gflags);
  free (temp);

  if (results && ((GLOB_FAILED (results)) == 0))
    {
      if (should_ignore_glob_matches ())
	ignore_glob_matches (results);
      if (results && results[0])
	strvec_sort (results, 1);		 
      else
	{
	  FREE (results);
	  results = (char **)&glob_error_return;
	}
    }

  return (results);
}

 

static struct ignorevar globignore =
{
  "GLOBIGNORE",
  (struct ign *)0,
  0,
  (char *)0,
  (sh_iv_item_func_t *)0,
};

 
void
setup_glob_ignore (name)
     char *name;
{
  char *v;

  v = get_string_value (name);
  setup_ignore_patterns (&globignore);

  if (globignore.num_ignores)
    glob_dot_filenames = 1;
  else if (v == 0)
    glob_dot_filenames = 0;
}

int
should_ignore_glob_matches ()
{
  return globignore.num_ignores;
}

 
static int
glob_name_is_acceptable (name)
     const char *name;
{
  struct ign *p;
  char *n;
  int flags;

   
  n = strrchr (name, '/');
  if (n == 0 || n[1] == 0)
    n = (char *)name;
  else
    n++;

  if (n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0')))
    return (0);

  flags = FNM_PATHNAME | FNMATCH_EXTFLAG | FNMATCH_NOCASEGLOB;
  for (p = globignore.ignores; p->val; p++)
    {
      if (strmatch (p->val, (char *)name, flags) != FNM_NOMATCH)
	return (0);
    }
  return (1);
}

 

static void
ignore_globbed_names (names, name_func)
     char **names;
     sh_ignore_func_t *name_func;
{
  char **newnames;
  int n, i;

  for (i = 0; names[i]; i++)
    ;
  newnames = strvec_create (i + 1);

  for (n = i = 0; names[i]; i++)
    {
      if ((*name_func) (names[i]))
	newnames[n++] = names[i];
      else
	free (names[i]);
    }

  newnames[n] = (char *)NULL;

  if (n == 0)
    {
      names[0] = (char *)NULL;
      free (newnames);
      return;
    }

   
  for (n = 0; newnames[n]; n++)
    names[n] = newnames[n];
  names[n] = (char *)NULL;
  free (newnames);
}

void
ignore_glob_matches (names)
     char **names;
{
  if (globignore.num_ignores == 0)
    return;

  ignore_globbed_names (names, glob_name_is_acceptable);
}

static char *
split_ignorespec (s, ip)
     char *s;
     int *ip;
{
  char *t;
  int n, i;

  if (s == 0)
    return 0;

  i = *ip;
  if (s[i] == 0)
    return 0;

  n = skip_to_delim (s, i, ":", SD_NOJMP|SD_EXTGLOB|SD_GLOB);
  t = substring (s, i, n);

  if (s[n] == ':')
    n++;  
  *ip = n;  
  return t;
}
  
void
setup_ignore_patterns (ivp)
     struct ignorevar *ivp;
{
  int numitems, maxitems, ptr;
  char *colon_bit, *this_ignoreval;
  struct ign *p;

  this_ignoreval = get_string_value (ivp->varname);

   
  if ((this_ignoreval && ivp->last_ignoreval && STREQ (this_ignoreval, ivp->last_ignoreval)) ||
      (!this_ignoreval && !ivp->last_ignoreval))
    return;

   
  ivp->num_ignores = 0;

  if (ivp->ignores)
    {
      for (p = ivp->ignores; p->val; p++)
	free(p->val);
      free (ivp->ignores);
      ivp->ignores = (struct ign *)NULL;
    }

  if (ivp->last_ignoreval)
    {
      free (ivp->last_ignoreval);
      ivp->last_ignoreval = (char *)NULL;
    }

  if (this_ignoreval == 0 || *this_ignoreval == '\0')
    return;

  ivp->last_ignoreval = savestring (this_ignoreval);

  numitems = maxitems = ptr = 0;

#if 0
  while (colon_bit = extract_colon_unit (this_ignoreval, &ptr))
#else
  while (colon_bit = split_ignorespec (this_ignoreval, &ptr))
#endif
    {
      if (numitems + 1 >= maxitems)
	{
	  maxitems += 10;
	  ivp->ignores = (struct ign *)xrealloc (ivp->ignores, maxitems * sizeof (struct ign));
	}
      ivp->ignores[numitems].val = colon_bit;
      ivp->ignores[numitems].len = strlen (colon_bit);
      ivp->ignores[numitems].flags = 0;
      if (ivp->item_func)
	(*ivp->item_func) (&ivp->ignores[numitems]);
      numitems++;
    }
  ivp->ignores[numitems].val = (char *)NULL;
  ivp->num_ignores = numitems;
}
