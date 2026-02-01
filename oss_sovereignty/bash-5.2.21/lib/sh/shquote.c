 

 

#include <config.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include <stdio.h>
#include <stdc.h>

#include "syntax.h"
#include <xmalloc.h>

#include "shmbchar.h"
#include "shmbutil.h"

extern char *ansic_quote PARAMS((char *, int, int *));
extern int ansic_shouldquote PARAMS((const char *));

 
static const char bstab[256] =
  {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 0, 0, 0, 0, 0,	 
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    1, 1, 1, 0, 1, 0, 1, 1,	 
    1, 1, 1, 0, 1, 0, 0, 0,	 
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 0, 1, 1,	 

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 0,	 

    1, 0, 0, 0, 0, 0, 0, 0,	 
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0,	 

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
  };

 
 
 
 
 

 
char *
sh_single_quote (string)
     const char *string;
{
  register int c;
  char *result, *r;
  const char *s;

  result = (char *)xmalloc (3 + (4 * strlen (string)));
  r = result;

  if (string[0] == '\'' && string[1] == 0)
    {
      *r++ = '\\';
      *r++ = '\'';
      *r++ = 0;
      return result;
    }

  *r++ = '\'';

  for (s = string; s && (c = *s); s++)
    {
      *r++ = c;

      if (c == '\'')
	{
	  *r++ = '\\';	 
	  *r++ = '\'';
	  *r++ = '\'';	 
	}
    }

  *r++ = '\'';
  *r = '\0';

  return (result);
}

 
char *
sh_double_quote (string)
     const char *string;
{
  register unsigned char c;
  int mb_cur_max;
  char *result, *r;
  size_t slen;
  const char *s, *send;
  DECLARE_MBSTATE;

  slen = strlen (string);
  send = string + slen;
  mb_cur_max = MB_CUR_MAX;

  result = (char *)xmalloc (3 + (2 * strlen (string)));
  r = result;
  *r++ = '"';

  for (s = string; s && (c = *s); s++)
    {
       
      if ((sh_syntaxtab[c] & CBSDQUOTE) && c != '\n')
	*r++ = '\\';

#if defined (HANDLE_MULTIBYTE)
      if ((locale_utf8locale && (c & 0x80)) ||
	  (locale_utf8locale == 0 && mb_cur_max > 1 && is_basic (c) == 0))
	{
	  COPY_CHAR_P (r, s, send);
	  s--;		 
	  continue;
	}
#endif

       
      *r++ = c;
    }

  *r++ = '"';
  *r = '\0';

  return (result);
}

 
char *
sh_mkdoublequoted (s, slen, flags)
     const char *s;
     int slen, flags;
{
  char *r, *ret;
  const char *send;
  int rlen, mb_cur_max;
  DECLARE_MBSTATE;

  send = s + slen;
  mb_cur_max = flags ? MB_CUR_MAX : 1;
  rlen = (flags == 0) ? slen + 3 : (2 * slen) + 1;
  ret = r = (char *)xmalloc (rlen);

  *r++ = '"';
  while (*s)
    {
      if (flags && *s == '"')
	*r++ = '\\';

#if defined (HANDLE_MULTIBYTE)
      if  (flags && ((locale_utf8locale && (*s & 0x80)) ||
		     (locale_utf8locale == 0 && mb_cur_max > 1 && is_basic (*s) == 0)))
	{
	  COPY_CHAR_P (r, s, send);
	  continue;
	}
#endif
      *r++ = *s++;
    }
  *r++ = '"';
  *r = '\0';

  return ret;
}

 
char *
sh_un_double_quote (string)
     char *string;
{
  register int c, pass_next;
  char *result, *r, *s;

  r = result = (char *)xmalloc (strlen (string) + 1);

  for (pass_next = 0, s = string; s && (c = *s); s++)
    {
      if (pass_next)
	{
	  *r++ = c;
	  pass_next = 0;
	  continue;
	}
      if (c == '\\' && (sh_syntaxtab[(unsigned char) s[1]] & CBSDQUOTE))
	{
	  pass_next = 1;
	  continue;
	}
      *r++ = c;
    }

  *r = '\0';
  return result;
}

 
   
char *
sh_backslash_quote (string, table, flags)
     char *string;
     char *table;
     int flags;
{
  int c, mb_cur_max;
  size_t slen;
  char *result, *r, *s, *backslash_table, *send;
  DECLARE_MBSTATE;

  slen = strlen (string);
  send = string + slen;
  result = (char *)xmalloc (2 * slen + 1);

  backslash_table = table ? table : (char *)bstab;
  mb_cur_max = MB_CUR_MAX;

  for (r = result, s = string; s && (c = *s); s++)
    {
#if defined (HANDLE_MULTIBYTE)
       
      if (c >= 0 && c <= 127 && backslash_table[(unsigned char)c] == 1)
	{
	  *r++ = '\\';
	  *r++ = c;
	  continue;
	}
      if ((locale_utf8locale && (c & 0x80)) ||
	  (locale_utf8locale == 0 && mb_cur_max > 1 && is_basic (c) == 0))
	{
	  COPY_CHAR_P (r, s, send);
	  s--;		 
	  continue;
	}
#endif
      if (backslash_table[(unsigned char)c] == 1)
	*r++ = '\\';
      else if (c == '#' && s == string)			 
	*r++ = '\\';
      else if ((flags&1) && c == '~' && (s == string || s[-1] == ':' || s[-1] == '='))
         
	*r++ = '\\';
      else if ((flags&2) && shellblank((unsigned char)c))
	*r++ = '\\';
      *r++ = c;
    }

  *r = '\0';
  return (result);
}

#if defined (PROMPT_STRING_DECODE) || defined (TRANSLATABLE_STRINGS)
 
char *
sh_backslash_quote_for_double_quotes (string, flags)
     char *string;
     int flags;
{
  unsigned char c;
  char *result, *r, *s, *send;
  size_t slen;
  int mb_cur_max;
  DECLARE_MBSTATE;
 
  slen = strlen (string);
  send = string + slen;
  mb_cur_max = MB_CUR_MAX;
  result = (char *)xmalloc (2 * slen + 1);

  for (r = result, s = string; s && (c = *s); s++)
    {
       
      if ((sh_syntaxtab[c] & CBSDQUOTE) && c != '\n')
	*r++ = '\\';
       
      else if (c == CTLESC || c == CTLNUL)
	*r++ = CTLESC;		 

#if defined (HANDLE_MULTIBYTE)
      if ((locale_utf8locale && (c & 0x80)) ||
	  (locale_utf8locale == 0 && mb_cur_max > 1 && is_basic (c) == 0))
	{
	  COPY_CHAR_P (r, s, send);
	  s--;		 
	  continue;
	}
#endif

      *r++ = c;
    }

  *r = '\0';
  return (result);
}
#endif  

char *
sh_quote_reusable (s, flags)
     char *s;
     int flags;
{
  char *ret;

  if (s == 0)
    return s;
  else if (*s == 0)
    {
      ret = (char *)xmalloc (3);
      ret[0] = ret[1] = '\'';
      ret[2] = '\0';
    }
  else if (ansic_shouldquote (s))
    ret = ansic_quote (s, 0, (int *)0);
  else if (flags)
    ret = sh_backslash_quote (s, 0, 1);
  else
    ret = sh_single_quote (s);

  return ret;
}

int
sh_contains_shell_metas (string)
     const char *string;
{
  const char *s;

  for (s = string; s && *s; s++)
    {
      switch (*s)
	{
	case ' ': case '\t': case '\n':		 
	case '\'': case '"': case '\\':		 
	case '|': case '&': case ';':		 
	case '(': case ')': case '<': case '>':
	case '!': case '{': case '}':		 
	case '*': case '[': case '?': case ']':	 
	case '^':
	case '$': case '`':			 
	  return (1);
	case '~':				 
	  if (s == string || s[-1] == '=' || s[-1] == ':')
	    return (1);
	  break;
	case '#':
	  if (s == string)			 
	    return (1);
	   
	default:
	  break;
	}
    }

  return (0);
}

int
sh_contains_quotes (string)
     const char *string;
{
  const char *s;

  for (s = string; s && *s; s++)
    {
      if (*s == '\'' || *s == '"' || *s == '\\')
	return 1;
    }
  return 0;
}
