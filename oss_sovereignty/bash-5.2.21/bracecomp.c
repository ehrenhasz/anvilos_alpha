 

 

 

#include "config.h"

#if defined (BRACE_EXPANSION) && defined (READLINE)

#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "bashansi.h"
#include "shmbutil.h"

#include "shell.h"
#include <readline/readline.h>

static int _strcompare PARAMS((char **, char **));

 
static int
string_gcd (s1, s2)
     char *s1, *s2;
{
  register int i;

  if (s1 == NULL || s2 == NULL)
    return (0);

  for (i = 0; *s1 && *s2; ++s1, ++s2, ++i)
    {
      if (*s1 != *s2)
	break;
    }

  return (i);
}

static char *
really_munge_braces (array, real_start, real_end, gcd_zero)
     char **array;
     int real_start, real_end, gcd_zero;
{
  int start, end, gcd;
  char *result, *subterm, *x;
  int result_size, flag, tlen;

  flag = 0;

  if (real_start == real_end)
    {
      x = array[real_start] ? sh_backslash_quote (array[real_start] + gcd_zero, 0, 0)
 			    : sh_backslash_quote (array[0], 0, 0);
      return x;
    }

  result = (char *)xmalloc (result_size = 16);
  *result = '\0';

  for (start = real_start; start < real_end; start = end + 1)
    {
      gcd = strlen (array[start]);
      for (end = start + 1; end < real_end; end++)
	{
	  int temp;

	  temp = string_gcd (array[start], array[end]);

	  if (temp <= gcd_zero)
	    break;

	  gcd = temp;
	}
      end--;

      if (gcd_zero == 0 && start == real_start && end != (real_end - 1))
	{
	   
	  result_size += 1;
	  result = (char *)xrealloc (result, result_size);
	  result[0] = '{'; result[1] = '\0';
	  flag++;
	}

       
      if (start == end)
	{
	  x = savestring (array[start] + gcd_zero);
	  subterm = sh_backslash_quote (x, 0, 0);
	  free (x);
	}
      else
	{
	   
	  tlen = gcd - gcd_zero;
	  x = (char *)xmalloc (tlen + 1);
	  strncpy (x, array[start] + gcd_zero, tlen);
	  x[tlen] = '\0';
	  subterm = sh_backslash_quote (x, 0, 0);
	  free (x);
	  result_size += strlen (subterm) + 1;
	  result = (char *)xrealloc (result, result_size);
	  strcat (result, subterm);
	  free (subterm);
	  strcat (result, "{");
	  subterm = really_munge_braces (array, start, end + 1, gcd);
	  subterm[strlen (subterm) - 1] = '}';
	}

      result_size += strlen (subterm) + 1;
      result = (char *)xrealloc (result, result_size);
      strcat (result, subterm);
      strcat (result, ",");
      free (subterm);
    }

  if (gcd_zero == 0)
    result[strlen (result) - 1] = flag ? '}' : '\0';
  return (result);
}

static int
_strcompare (s1, s2)
     char **s1, **s2;
{
  int result;

  result = **s1 - **s2;
  if (result == 0)
    result = strcmp (*s1, *s2);

  return result;
}

static int
hack_braces_completion (names)
     char **names;
{
  register int i;
  char *temp;

  i = strvec_len (names);
  if (MB_CUR_MAX > 1 && i > 2)
    qsort (names+1, i-1, sizeof (char *), (QSFUNC *)_strcompare);
      
  temp = really_munge_braces (names, 1, i, 0);

  for (i = 0; names[i]; ++i)
    {
      free (names[i]);
      names[i] = NULL;
    }
  names[0] = temp;
  return 0;
}

 
int
bash_brace_completion (count, ignore)
     int count, ignore;
{
  rl_compignore_func_t *orig_ignore_func;
  rl_compentry_func_t *orig_entry_func;
  rl_quote_func_t *orig_quoting_func;
  rl_completion_func_t *orig_attempt_func;
  int orig_quoting_desired, r;

  orig_ignore_func = rl_ignore_some_completions_function;
  orig_attempt_func = rl_attempted_completion_function;
  orig_entry_func = rl_completion_entry_function;
  orig_quoting_func = rl_filename_quoting_function;
  orig_quoting_desired = rl_filename_quoting_desired;

  rl_completion_entry_function = rl_filename_completion_function;
  rl_attempted_completion_function = (rl_completion_func_t *)NULL;
  rl_ignore_some_completions_function = hack_braces_completion;
  rl_filename_quoting_function = (rl_quote_func_t *)NULL;
  rl_filename_quoting_desired = 0;

  r = rl_complete_internal (TAB);

  rl_ignore_some_completions_function = orig_ignore_func;
  rl_attempted_completion_function = orig_attempt_func;
  rl_completion_entry_function = orig_entry_func;
  rl_filename_quoting_function = orig_quoting_func;
  rl_filename_quoting_desired = orig_quoting_desired;

  return r;
}
#endif  
