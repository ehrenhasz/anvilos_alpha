 

 

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
#  ifndef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "rlmbutil.h"

#include "history.h"
#include "histlib.h"
#include "chardefs.h"

#include "rlshell.h"
#include "xmalloc.h"

#define HISTORY_WORD_DELIMITERS		" \t\n;&()|<>"
#define HISTORY_QUOTE_CHARACTERS	"\"'`"
#define HISTORY_EVENT_DELIMITERS	"^$*%-"

#define slashify_in_quotes "\\`\"$"

#define fielddelim(c)	(whitespace(c) || (c) == '\n')

typedef int _hist_search_func_t (const char *, int);

static char error_pointer;

static char *subst_lhs;
static char *subst_rhs;
static int subst_lhs_len;
static int subst_rhs_len;

 
static char *history_event_delimiter_chars = HISTORY_EVENT_DELIMITERS;

static char *get_history_word_specifier (char *, char *, int *);
static int history_tokenize_word (const char *, int);
static char **history_tokenize_internal (const char *, int, int *);
static char *history_substring (const char *, int, int);
static void freewords (char **, int);
static char *history_find_word (char *, int);

static char *quote_breaks (char *);

 
 
char history_expansion_char = '!';

 
char history_subst_char = '^';

 
char history_comment_char = '\0';

 
char *history_no_expand_chars = " \t\n\r=";

 
int history_quotes_inhibit_expansion = 0;

 
char *history_word_delimiters = HISTORY_WORD_DELIMITERS;

 
rl_linebuf_func_t *history_inhibit_expansion_function;

int history_quoting_state = 0;

 
 
 
 
 

 

 
static char *search_string;
 
static char *search_match;

 
char *
get_history_event (const char *string, int *caller_index, int delimiting_quote)
{
  register int i;
  register char c;
  HIST_ENTRY *entry;
  int which, sign, local_index, substring_okay;
  _hist_search_func_t *search_func;
  char *temp;

   

  i = *caller_index;

  if (string[i] != history_expansion_char)
    return ((char *)NULL);

   
  i++;

  sign = 1;
  substring_okay = 0;

#define RETURN_ENTRY(e, w) \
	return ((e = history_get (w)) ? e->line : (char *)NULL)

   
  if (string[i] == history_expansion_char)
    {
      i++;
      which = history_base + (history_length - 1);
      *caller_index = i;
      RETURN_ENTRY (entry, which);
    }

   
  if (string[i] == '-' && _rl_digit_p (string[i+1]))
    {
      sign = -1;
      i++;
    }

  if (_rl_digit_p (string[i]))
    {
       
      for (which = 0; _rl_digit_p (string[i]); i++)
	which = (which * 10) + _rl_digit_value (string[i]);

      *caller_index = i;

      if (sign < 0)
	which = (history_length + history_base) - which;

      RETURN_ENTRY (entry, which);
    }

   
  if (string[i] == '?')
    {
      substring_okay++;
      i++;
    }

   
  for (local_index = i; c = string[i]; i++)
    {
#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	{
	  int v;
	  mbstate_t ps;

	  memset (&ps, 0, sizeof (mbstate_t));
	   
	  _rl_adjust_point ((char *)string, i, &ps);
	  if ((v = _rl_get_char_len ((char *)string + i, &ps)) > 1)
	    {
	      i += v - 1;
	      continue;
	    }
        }

#endif  
      if ((!substring_okay &&
	    (whitespace (c) || c == ':' ||
	    (i > local_index && history_event_delimiter_chars && c == '-') ||
	    (c != '-' && history_event_delimiter_chars && member (c, history_event_delimiter_chars)) ||
	    (history_search_delimiter_chars && member (c, history_search_delimiter_chars)) ||
	    string[i] == delimiting_quote)) ||
	  string[i] == '\n' ||
	  (substring_okay && string[i] == '?'))
	break;
    }

  which = i - local_index;
  temp = (char *)xmalloc (1 + which);
  if (which)
    strncpy (temp, string + local_index, which);
  temp[which] = '\0';

  if (substring_okay && string[i] == '?')
    i++;

  *caller_index = i;

#define FAIL_SEARCH() \
  do { \
    history_offset = history_length; xfree (temp) ; return (char *)NULL; \
  } while (0)

   
  if (*temp == '\0' && substring_okay)
    {
      if (search_string)
        {
          xfree (temp);
          temp = savestring (search_string);
        }
      else
        FAIL_SEARCH ();
    }

  search_func = substring_okay ? history_search : history_search_prefix;
  while (1)
    {
      local_index = (*search_func) (temp, -1);

      if (local_index < 0)
	FAIL_SEARCH ();

      if (local_index == 0 || substring_okay)
	{
	  entry = current_history ();
	  if (entry == 0)
	    FAIL_SEARCH ();
	  history_offset = history_length;
	
	   
	  if (substring_okay)
	    {
	      FREE (search_string);
	      search_string = temp;

	      FREE (search_match);
	      search_match = history_find_word (entry->line, local_index);
	    }
	  else
	    xfree (temp);

	  return (entry->line);
	}

      if (history_offset)
	history_offset--;
      else
	FAIL_SEARCH ();
    }
#undef FAIL_SEARCH
#undef RETURN_ENTRY
}

 

 
static void
hist_string_extract_single_quoted (char *string, int *sindex, int flags)
{
  register int i;

  for (i = *sindex; string[i] && string[i] != '\''; i++)
    {
      if ((flags & 1) && string[i] == '\\' && string[i+1])
        i++;
    }

  *sindex = i;
}

static char *
quote_breaks (char *s)
{
  register char *p, *r;
  char *ret;
  int len = 3;

  for (p = s; p && *p; p++, len++)
    {
      if (*p == '\'')
	len += 3;
      else if (whitespace (*p) || *p == '\n')
	len += 2;
    }

  r = ret = (char *)xmalloc (len);
  *r++ = '\'';
  for (p = s; p && *p; )
    {
      if (*p == '\'')
	{
	  *r++ = '\'';
	  *r++ = '\\';
	  *r++ = '\'';
	  *r++ = '\'';
	  p++;
	}
      else if (whitespace (*p) || *p == '\n')
	{
	  *r++ = '\'';
	  *r++ = *p++;
	  *r++ = '\'';
	}
      else
	*r++ = *p++;
    }
  *r++ = '\'';
  *r = '\0';
  return ret;
}

static char *
hist_error(char *s, int start, int current, int errtype)
{
  char *temp;
  const char *emsg;
  int ll, elen;

  ll = current - start;

  switch (errtype)
    {
    case EVENT_NOT_FOUND:
      emsg = "event not found";
      elen = 15;
      break;
    case BAD_WORD_SPEC:
      emsg = "bad word specifier";
      elen = 18;
      break;
    case SUBST_FAILED:
      emsg = "substitution failed";
      elen = 19;
      break;
    case BAD_MODIFIER:
      emsg = "unrecognized history modifier";
      elen = 29;
      break;
    case NO_PREV_SUBST:
      emsg = "no previous substitution";
      elen = 24;
      break;
    default:
      emsg = "unknown expansion error";
      elen = 23;
      break;
    }

  temp = (char *)xmalloc (ll + elen + 3);
  if (s[start])
    strncpy (temp, s + start, ll);
  else
    ll = 0;
  temp[ll] = ':';
  temp[ll + 1] = ' ';
  strcpy (temp + ll + 2, emsg);
  return (temp);
}

 

static char *
get_subst_pattern (char *str, int *iptr, int delimiter, int is_rhs, int *lenptr)
{
  register int si, i, j, k;
  char *s;
#if defined (HANDLE_MULTIBYTE)
  mbstate_t ps;
#endif

  s = (char *)NULL;
  i = *iptr;

#if defined (HANDLE_MULTIBYTE)
  memset (&ps, 0, sizeof (mbstate_t));
  _rl_adjust_point (str, i, &ps);
#endif

  for (si = i; str[si] && str[si] != delimiter; si++)
#if defined (HANDLE_MULTIBYTE)
    if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
      {
	int v;
	if ((v = _rl_get_char_len (str + si, &ps)) > 1)
	  si += v - 1;
	else if (str[si] == '\\' && str[si + 1] == delimiter)
	  si++;
      }
    else
#endif  
      if (str[si] == '\\' && str[si + 1] == delimiter)
	si++;

  if (si > i || is_rhs)
    {
      s = (char *)xmalloc (si - i + 1);
      for (j = 0, k = i; k < si; j++, k++)
	{
	   
	  if (str[k] == '\\' && str[k + 1] == delimiter)
	    k++;
	  s[j] = str[k];
	}
      s[j] = '\0';
      if (lenptr)
	*lenptr = j;
    }

  i = si;
  if (str[i])
    i++;
  *iptr = i;

  return s;
}

static void
postproc_subst_rhs (void)
{
  char *new;
  int i, j, new_size;

  new = (char *)xmalloc (new_size = subst_rhs_len + subst_lhs_len);
  for (i = j = 0; i < subst_rhs_len; i++)
    {
      if (subst_rhs[i] == '&')
	{
	  if (j + subst_lhs_len >= new_size)
	    new = (char *)xrealloc (new, (new_size = new_size * 2 + subst_lhs_len));
	  strcpy (new + j, subst_lhs);
	  j += subst_lhs_len;
	}
      else
	{
	   
	  if (subst_rhs[i] == '\\' && subst_rhs[i + 1] == '&')
	    i++;
	  if (j >= new_size)
	    new = (char *)xrealloc (new, new_size *= 2);
	  new[j++] = subst_rhs[i];
	}
    }
  new[j] = '\0';
  xfree (subst_rhs);
  subst_rhs = new;
  subst_rhs_len = j;
}

 
 
static int
history_expand_internal (char *string, int start, int qc, int *end_index_ptr, char **ret_string, char *current_line)
{
  int i, n, starting_index;
  int substitute_globally, subst_bywords, want_quotes, print_only;
  char *event, *temp, *result, *tstr, *t, c, *word_spec;
  int result_len;
#if defined (HANDLE_MULTIBYTE)
  mbstate_t ps;

  memset (&ps, 0, sizeof (mbstate_t));
#endif

  result = (char *)xmalloc (result_len = 128);

  i = start;

   

  if (member (string[i + 1], ":$*%^"))
    {
      char fake_s[3];
      int fake_i = 0;
      i++;
      fake_s[0] = fake_s[1] = history_expansion_char;
      fake_s[2] = '\0';
      event = get_history_event (fake_s, &fake_i, 0);
    }
  else if (string[i + 1] == '#')
    {
      i += 2;
      event = current_line;
    }
  else
    event = get_history_event (string, &i, qc);
	  
  if (event == 0)
    {
      *ret_string = hist_error (string, start, i, EVENT_NOT_FOUND);
      xfree (result);
      return (-1);
    }

   
  starting_index = i;
  word_spec = get_history_word_specifier (string, event, &i);

   
  if (word_spec == (char *)&error_pointer)
    {
      *ret_string = hist_error (string, starting_index, i, BAD_WORD_SPEC);
      xfree (result);
      return (-1);
    }

   
  temp = word_spec ? savestring (word_spec) : savestring (event);
  FREE (word_spec);

   
  want_quotes = substitute_globally = subst_bywords = print_only = 0;
  starting_index = i;

  while (string[i] == ':')
    {
      c = string[i + 1];

      if (c == 'g' || c == 'a')
	{
	  substitute_globally = 1;
	  i++;
	  c = string[i + 1];
	}
      else if (c == 'G')
	{
	  subst_bywords = 1;
	  i++;
	  c = string[i + 1];
	}

      switch (c)
	{
	default:
	  *ret_string = hist_error (string, i+1, i+2, BAD_MODIFIER);
	  xfree (result);
	  xfree (temp);
	  return -1;

	case 'q':
	  want_quotes = 'q';
	  break;

	case 'x':
	  want_quotes = 'x';
	  break;

	   
	case 'p':
	  print_only = 1;
	  break;

	   
	case 't':
	  tstr = strrchr (temp, '/');
	  if (tstr)
	    {
	      tstr++;
	      t = savestring (tstr);
	      xfree (temp);
	      temp = t;
	    }
	  break;

	   
	case 'h':
	  tstr = strrchr (temp, '/');
	  if (tstr)
	    *tstr = '\0';
	  break;

	   
	case 'r':
	  tstr = strrchr (temp, '.');
	  if (tstr)
	    *tstr = '\0';
	  break;

	   
	case 'e':
	  tstr = strrchr (temp, '.');
	  if (tstr)
	    {
	      t = savestring (tstr);
	      xfree (temp);
	      temp = t;
	    }
	  break;

	 

	case '&':
	case 's':
	  {
	    char *new_event;
	    int delimiter, failed, si, l_temp, ws, we;

	    if (c == 's')
	      {
		if (i + 2 < (int)strlen (string))
		  {
#if defined (HANDLE_MULTIBYTE)
		    if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
		      {
			_rl_adjust_point (string, i + 2, &ps);
			if (_rl_get_char_len (string + i + 2, &ps) > 1)
			  delimiter = 0;
			else
			  delimiter = string[i + 2];
		      }
		    else
#endif  
		      delimiter = string[i + 2];
		  }
		else
		  break;	 

		i += 3;

		t = get_subst_pattern (string, &i, delimiter, 0, &subst_lhs_len);
		 
		if (t)
		  {
		    FREE (subst_lhs);
		    subst_lhs = t;
		  }
		else if (!subst_lhs)
		  {
		    if (search_string && *search_string)
		      {
			subst_lhs = savestring (search_string);
			subst_lhs_len = strlen (subst_lhs);
		      }
		    else
		      {
			subst_lhs = (char *) NULL;
			subst_lhs_len = 0;
		      }
		  }

		FREE (subst_rhs);
		subst_rhs = get_subst_pattern (string, &i, delimiter, 1, &subst_rhs_len);

		 
		if (member ('&', subst_rhs))
		  postproc_subst_rhs ();
	      }
	    else
	      i += 2;

	     
	    if (subst_lhs_len == 0)
	      {
		*ret_string = hist_error (string, starting_index, i, NO_PREV_SUBST);
		xfree (result);
		xfree (temp);
		return -1;
	      }

	    l_temp = strlen (temp);
	     
	    if (subst_lhs_len > l_temp)
	      {
		*ret_string = hist_error (string, starting_index, i, SUBST_FAILED);
		xfree (result);
		xfree (temp);
		return (-1);
	      }

	     
	     

	    si = we = 0;
	    for (failed = 1; (si + subst_lhs_len) <= l_temp; si++)
	      {
		 
		if (subst_bywords && si > we)
		  {
		    for (; temp[si] && fielddelim (temp[si]); si++)
		      ;
		    ws = si;
		    we = history_tokenize_word (temp, si);
		  }

		if (STREQN (temp+si, subst_lhs, subst_lhs_len))
		  {
		    int len = subst_rhs_len - subst_lhs_len + l_temp;
		    new_event = (char *)xmalloc (1 + len);
		    strncpy (new_event, temp, si);
		    strncpy (new_event + si, subst_rhs, subst_rhs_len);
		    strncpy (new_event + si + subst_rhs_len,
			     temp + si + subst_lhs_len,
			     l_temp - (si + subst_lhs_len));
		    new_event[len] = '\0';
		    xfree (temp);
		    temp = new_event;

		    failed = 0;

		    if (substitute_globally)
		      {
			 
			si += subst_rhs_len - 1;
			l_temp = strlen (temp);
			substitute_globally++;
			continue;
		      }
		    else if (subst_bywords)
		      {
			si = we;
			l_temp = strlen (temp);
			continue;
		      }
		    else
		      break;
		  }
	      }

	    if (substitute_globally > 1)
	      {
		substitute_globally = 0;
		continue;	 
	      }

	    if (failed == 0)
	      continue;		 

	    *ret_string = hist_error (string, starting_index, i, SUBST_FAILED);
	    xfree (result);
	    xfree (temp);
	    return (-1);
	  }
	}
      i += 2;
    }
   
   
  --i;

  if (want_quotes)
    {
      char *x;

      if (want_quotes == 'q')
	x = sh_single_quote (temp);
      else if (want_quotes == 'x')
	x = quote_breaks (temp);
      else
	x = savestring (temp);

      xfree (temp);
      temp = x;
    }

  n = strlen (temp);
  if (n >= result_len)
    result = (char *)xrealloc (result, n + 2);
  strcpy (result, temp);
  xfree (temp);

  *end_index_ptr = i;
  *ret_string = result;
  return (print_only);
}

 

#define ADD_STRING(s) \
	do \
	  { \
	    int sl = strlen (s); \
	    j += sl; \
	    if (j >= result_len) \
	      { \
		while (j >= result_len) \
		  result_len += 128; \
		result = (char *)xrealloc (result, result_len); \
	      } \
	    strcpy (result + j - sl, s); \
	  } \
	while (0)

#define ADD_CHAR(c) \
	do \
	  { \
	    if (j >= result_len - 1) \
	      result = (char *)xrealloc (result, result_len += 64); \
	    result[j++] = c; \
	    result[j] = '\0'; \
	  } \
	while (0)

int
history_expand (char *hstring, char **output)
{
  register int j;
  int i, r, l, passc, cc, modified, eindex, only_printing, dquote, squote, flag;
  char *string;

   
  int result_len;
  char *result;

#if defined (HANDLE_MULTIBYTE)
  char mb[MB_LEN_MAX];
  mbstate_t ps;
#endif

   
  char *temp;

  if (output == 0)
    return 0;

   
  if (history_expansion_char == 0)
    {
      *output = savestring (hstring);
      return (0);
    }
    
   
  result = (char *)xmalloc (result_len = 256);
  result[0] = '\0';

  only_printing = modified = 0;
  l = strlen (hstring);

   

   

   
  if (hstring[0] == history_subst_char)
    {
      string = (char *)xmalloc (l + 5);

      string[0] = string[1] = history_expansion_char;
      string[2] = ':';
      string[3] = 's';
      strcpy (string + 4, hstring);
      l += 4;
    }
  else
    {
#if defined (HANDLE_MULTIBYTE)
      memset (&ps, 0, sizeof (mbstate_t));
#endif

      string = hstring;
       

       
      dquote = history_quoting_state == '"';
      squote = history_quoting_state == '\'';

       
      i = 0;
      if (squote && history_quotes_inhibit_expansion)
	{
	  hist_string_extract_single_quoted (string, &i, 0);
	  squote = 0;
	  if (string[i])
	    i++;
	}

      for ( ; string[i]; i++)
	{
#if defined (HANDLE_MULTIBYTE)
	  if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	    {
	      int v;
	      v = _rl_get_char_len (string + i, &ps);
	      if (v > 1)
		{
		  i += v - 1;
		  continue;
		}
	    }
#endif  

	  cc = string[i + 1];
	   
	  if (history_comment_char && string[i] == history_comment_char &&
	      dquote == 0 &&
	      (i == 0 || member (string[i - 1], history_word_delimiters)))
	    {
	      while (string[i])
		i++;
	      break;
	    }
	  else if (string[i] == history_expansion_char)
	    {
	      if (cc == 0 || member (cc, history_no_expand_chars))
		continue;
	       
	      else if (dquote && cc == '"')
		continue;
	       
	      else if (history_inhibit_expansion_function &&
			(*history_inhibit_expansion_function) (string, i))
		continue;
	      else
		break;
	    }
	   
	  else if (dquote && string[i] == '\\' && cc == '"')
	    i++;
	   
	  else if (history_quotes_inhibit_expansion && string[i] == '"')
	    {
	      dquote = 1 - dquote;
	    }
	  else if (dquote == 0 && history_quotes_inhibit_expansion && string[i] == '\'')
	    {
	       
	      flag = (i > 0 && string[i - 1] == '$');
	      i++;
	      hist_string_extract_single_quoted (string, &i, flag);
	    }
	  else if (history_quotes_inhibit_expansion && string[i] == '\\')
	    {
	       
	      if (cc == '\'' || cc == history_expansion_char)
		i++;
	    }
	  
	}
	  
      if (string[i] != history_expansion_char)
	{
	  xfree (result);
	  *output = savestring (string);
	  return (0);
	}
    }

   
  dquote = history_quoting_state == '"';
  squote = history_quoting_state == '\'';

   
  i = j = 0;
  if (squote && history_quotes_inhibit_expansion)
    {
      int c;

      hist_string_extract_single_quoted (string, &i, 0);
      squote = 0;
      for (c = 0; c < i; c++)
	ADD_CHAR (string[c]);      
      if (string[i])
	{
	  ADD_CHAR (string[i]);
	  i++;
	}
    }

  for (passc = 0; i < l; i++)
    {
      int qc, tchar = string[i];

      if (passc)
	{
	  passc = 0;
	  ADD_CHAR (tchar);
	  continue;
	}

#if defined (HANDLE_MULTIBYTE)
      if (MB_CUR_MAX > 1 && rl_byte_oriented == 0)
	{
	  int k, c;

	  c = tchar;
	  memset (mb, 0, sizeof (mb));
	  for (k = 0; k < MB_LEN_MAX; k++)
	    {
	      mb[k] = (char)c;
	      memset (&ps, 0, sizeof (mbstate_t));
	      if (_rl_get_char_len (mb, &ps) == -2)
		c = string[++i];
	      else
		break;
	    }
	  if (strlen (mb) > 1)
	    {
	      ADD_STRING (mb);
	      continue;
	    }
	}
#endif  

      if (tchar == history_expansion_char)
	tchar = -3;
      else if (tchar == history_comment_char)
	tchar = -2;

      switch (tchar)
	{
	default:
	  ADD_CHAR (string[i]);
	  break;

	case '\\':
	  passc++;
	  ADD_CHAR (tchar);
	  break;

	case '"':
	  dquote = 1 - dquote;
	  ADD_CHAR (tchar);
	  break;
	  
	case '\'':
	  {
	     
	    if (squote)
	      {
	        squote = 0;
	        ADD_CHAR (tchar);
	      }
	    else if (dquote == 0 && history_quotes_inhibit_expansion)
	      {
		int quote, slen;

		flag = (i > 0 && string[i - 1] == '$');
		quote = i++;
		hist_string_extract_single_quoted (string, &i, flag);

		slen = i - quote + 2;
		temp = (char *)xmalloc (slen);
		strncpy (temp, string + quote, slen);
		temp[slen - 1] = '\0';
		ADD_STRING (temp);
		xfree (temp);
	      }
	    else if (dquote == 0 && squote == 0 && history_quotes_inhibit_expansion == 0)
	      {
	        squote = 1;
	        ADD_CHAR (string[i]);
	      }
	    else
	      ADD_CHAR (string[i]);
	    break;
	  }

	case -2:		 
	  if ((dquote == 0 || history_quotes_inhibit_expansion == 0) &&
	      (i == 0 || member (string[i - 1], history_word_delimiters)))
	    {
	      temp = (char *)xmalloc (l - i + 1);
	      strcpy (temp, string + i);
	      ADD_STRING (temp);
	      xfree (temp);
	      i = l;
	    }
	  else
	    ADD_CHAR (string[i]);
	  break;

	case -3:		 
	  cc = string[i + 1];

	   
	  if (cc == 0 || member (cc, history_no_expand_chars) ||
			 (dquote && cc == '"'))
	    {
	      ADD_CHAR (string[i]);
	      break;
	    }

	   
	   
	  if (history_inhibit_expansion_function)
	    {
	      int save_j, temp;

	      save_j = j;
	      ADD_CHAR (string[i]);
	      ADD_CHAR (cc);

	      temp = (*history_inhibit_expansion_function) (result, save_j);
	      if (temp)
		{
		  result[--j] = '\0';	 
		  break;
		}
	      else
	        result[j = save_j] = '\0';
	    }

#if defined (NO_BANG_HASH_MODIFIERS)
	   
	  if (cc == '#')
	    {
	      if (result)
		{
		  temp = (char *)xmalloc (1 + strlen (result));
		  strcpy (temp, result);
		  ADD_STRING (temp);
		  xfree (temp);
		}
	      i++;
	      break;
	    }
#endif
	  qc = squote ? '\'' : (dquote ? '"' : 0);
	  r = history_expand_internal (string, i, qc, &eindex, &temp, result);
	  if (r < 0)
	    {
	      *output = temp;
	      xfree (result);
	      if (string != hstring)
		xfree (string);
	      return -1;
	    }
	  else
	    {
	      if (temp)
		{
		  modified++;
		  if (*temp)
		    ADD_STRING (temp);
		  xfree (temp);
		}
	      only_printing += r == 1;
	      i = eindex;
	    }
	  break;
	}
    }

  *output = result;
  if (string != hstring)
    xfree (string);

  if (only_printing)
    {
#if 0
      add_history (result);
#endif
      return (2);
    }

  return (modified != 0);
}

 
static char *
get_history_word_specifier (char *spec, char *from, int *caller_index)
{
  register int i = *caller_index;
  int first, last;
  int expecting_word_spec = 0;
  char *result;

   
  first = last = 0;
  result = (char *)NULL;

   
  if (spec[i] == ':')
    {
      i++;
      expecting_word_spec++;
    }

   

   
  if (spec[i] == '%')
    {
      *caller_index = i + 1;
      return (search_match ? savestring (search_match) : savestring (""));
    }

   
  if (spec[i] == '*')
    {
      *caller_index = i + 1;
      result = history_arg_extract (1, '$', from);
      return (result ? result : savestring (""));
    }

   
  if (spec[i] == '$')
    {
      *caller_index = i + 1;
      return (history_arg_extract ('$', '$', from));
    }

   

  if (spec[i] == '-')
    first = 0;
  else if (spec[i] == '^')
    {
      first = 1;
      i++;
    }
  else if (_rl_digit_p (spec[i]) && expecting_word_spec)
    {
      for (first = 0; _rl_digit_p (spec[i]); i++)
	first = (first * 10) + _rl_digit_value (spec[i]);
    }
  else
    return ((char *)NULL);	 

  if (spec[i] == '^' || spec[i] == '*')
    {
      last = (spec[i] == '^') ? 1 : '$';	 
      i++;
    }
  else if (spec[i] != '-')
    last = first;
  else
    {
      i++;

      if (_rl_digit_p (spec[i]))
	{
	  for (last = 0; _rl_digit_p (spec[i]); i++)
	    last = (last * 10) + _rl_digit_value (spec[i]);
	}
      else if (spec[i] == '$')
	{
	  i++;
	  last = '$';
	}
      else if (spec[i] == '^')
	{
	  i++;
	  last = 1;
	}
#if 0
      else if (!spec[i] || spec[i] == ':')
	 
#else
      else
	 
#endif
	last = -1;		 
    }

  *caller_index = i;

  if (last >= first || last == '$' || last < 0)
    result = history_arg_extract (first, last, from);

  return (result ? result : (char *)&error_pointer);
}

 
char *
history_arg_extract (int first, int last, const char *string)
{
  register int i, len;
  char *result;
  int size, offset;
  char **list;

   
  if ((list = history_tokenize (string)) == NULL)
    return ((char *)NULL);

  for (len = 0; list[len]; len++)
    ;

  if (last < 0)
    last = len + last - 1;

  if (first < 0)
    first = len + first - 1;

  if (last == '$')
    last = len - 1;

  if (first == '$')
    first = len - 1;

  last++;

  if (first >= len || last > len || first < 0 || last < 0 || first > last)
    result = ((char *)NULL);
  else
    {
      for (size = 0, i = first; i < last; i++)
	size += strlen (list[i]) + 1;
      result = (char *)xmalloc (size + 1);
      result[0] = '\0';

      for (i = first, offset = 0; i < last; i++)
	{
	  strcpy (result + offset, list[i]);
	  offset += strlen (list[i]);
	  if (i + 1 < last)
	    {
      	      result[offset++] = ' ';
	      result[offset] = 0;
	    }
	}
    }

  for (i = 0; i < len; i++)
    xfree (list[i]);
  xfree (list);

  return (result);
}

static int
history_tokenize_word (const char *string, int ind)
{
  register int i, j;
  int delimiter, nestdelim, delimopen;

  i = ind;
  delimiter = nestdelim = 0;

  if (member (string[i], "()\n"))	 
    {
      i++;
      return i;
    }

  if (ISDIGIT (string[i]))
    {
      j = i;
      while (string[j] && ISDIGIT (string[j]))
	j++;
      if (string[j] == 0)
	return (j);
      if (string[j] == '<' || string[j] == '>')
	i = j;			 
      else
	{
	  i = j;
	  goto get_word;	 
	}
    }

  if (member (string[i], "<>;&|"))
    {
      int peek = string[i + 1];

      if (peek == string[i])
	{
	  if (peek == '<' && string[i + 2] == '-')
	    i++;
	  else if (peek == '<' && string[i + 2] == '<')
	    i++;
	  i += 2;
	  return i;
	}
      else if (peek == '&' && (string[i] == '>' || string[i] == '<'))
	{
	  j = i + 2;
	  while (string[j] && ISDIGIT (string[j]))	 
	    j++;
	  if (string[j] =='-')		 
	    j++;
	  return j;
	}
      else if ((peek == '>' && string[i] == '&') || (peek == '|' && string[i] == '>'))
	{
	  i += 2;
	  return i;
	}
       
      else if (peek == '(' && (string[i] == '>' || string[i] == '<'))  
	{
	  i += 2;
	  delimopen = '(';
	  delimiter = ')';
	  nestdelim = 1;
	  goto get_word;
	}

      i++;
      return i;
    }

get_word:
   

  if (delimiter == 0 && member (string[i], HISTORY_QUOTE_CHARACTERS))
    delimiter = string[i++];

  for (; string[i]; i++)
    {
      if (string[i] == '\\' && string[i + 1] == '\n')
	{
	  i++;
	  continue;
	}

      if (string[i] == '\\' && delimiter != '\'' &&
	  (delimiter != '"' || member (string[i], slashify_in_quotes)))
	{
	  i++;
	  continue;
	}

       
      if (nestdelim && string[i] == delimopen)
	{
	  nestdelim++;
	  continue;
	}
      if (nestdelim && string[i] == delimiter)
	{
	  nestdelim--;
	  if (nestdelim == 0)
	    delimiter = 0;
	  continue;
	}
      
      if (delimiter && string[i] == delimiter)
	{
	  delimiter = 0;
	  continue;
	}

       
      if (nestdelim == 0 && delimiter == 0 && member (string[i], "<>$!@?+*") && string[i+1] == '(')  
	{
	  i += 2;
	  delimopen = '(';
	  delimiter = ')';
	  nestdelim = 1;
	  continue;
	}
      
      if (delimiter == 0 && (member (string[i], history_word_delimiters)))
	break;

      if (delimiter == 0 && member (string[i], HISTORY_QUOTE_CHARACTERS))
	delimiter = string[i];
    }

  return i;
}

static char *
history_substring (const char *string, int start, int end)
{
  register int len;
  register char *result;

  len = end - start;
  result = (char *)xmalloc (len + 1);
  strncpy (result, string + start, len);
  result[len] = '\0';
  return result;
}

 
static char **
history_tokenize_internal (const char *string, int wind, int *indp)
{
  char **result;
  register int i, start, result_index, size;

   
  if (indp && wind != -1)
    *indp = -1;

   
  for (i = result_index = size = 0, result = (char **)NULL; string[i]; )
    {
       
      for (; string[i] && fielddelim (string[i]); i++)
	;
      if (string[i] == 0 || string[i] == history_comment_char)
	return (result);

      start = i;

      i = history_tokenize_word (string, start);

       
      if (i == start && history_word_delimiters)
	{
	  i++;
	  while (string[i] && member (string[i], history_word_delimiters))
	    i++;
	}

       
      if (indp && wind != -1 && wind >= start && wind < i)
        *indp = result_index;

      if (result_index + 2 >= size)
	result = (char **)xrealloc (result, ((size += 10) * sizeof (char *)));

      result[result_index++] = history_substring (string, start, i);
      result[result_index] = (char *)NULL;
    }

  return (result);
}

 
char **
history_tokenize (const char *string)
{
  return (history_tokenize_internal (string, -1, (int *)NULL));
}

 
static void
freewords (char **words, int start)
{
  register int i;

  for (i = start; words[i]; i++)
    xfree (words[i]);
}

 
static char *
history_find_word (char *line, int ind)
{
  char **words, *s;
  int i, wind;

  words = history_tokenize_internal (line, ind, &wind);
  if (wind == -1 || words == 0)
    {
      if (words)
	freewords (words, 0);
      FREE (words);
      return ((char *)NULL);
    }
  s = words[wind];
  for (i = 0; i < wind; i++)
    xfree (words[i]);
  freewords (words, wind + 1);
  xfree (words);
  return s;
}
