 

 

#include <config.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include <stdio.h>
#include <chartypes.h>
#include "../bashtypes.h"
#include "posixstat.h"
#include <signal.h>

#include <errno.h>

#if defined (PREFER_STDARG)
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif

#include "../bashansi.h"
#include "../bashintl.h"

#define NEED_FPURGE_DECL

#include "../shell.h"
#include "maxpath.h"
#include "../flags.h"
#include "../parser.h"
#include "../jobs.h"
#include "../builtins.h"
#include "../input.h"
#include "../execute_cmd.h"
#include "../trap.h"
#include "bashgetopt.h"
#include "common.h"
#include "builtext.h"
#include <tilde/tilde.h>

#if defined (HISTORY)
#  include "../bashhist.h"
#endif

#if !defined (errno)
extern int errno;   
#endif  

extern const char * const bash_getcwd_errstr;

 
sh_builtin_func_t *last_shell_builtin = (sh_builtin_func_t *)NULL;
sh_builtin_func_t *this_shell_builtin = (sh_builtin_func_t *)NULL;

 
 
 
 
 

 

static void
builtin_error_prolog ()
{
  char *name;

  name = get_name_for_error ();
  fprintf (stderr, "%s: ", name);

  if (interactive_shell == 0)
    fprintf (stderr, _("line %d: "), executing_line_number ());

  if (this_command_name && *this_command_name)
    fprintf (stderr, "%s: ", this_command_name);
}

void
#if defined (PREFER_STDARG)
builtin_error (const char *format, ...)
#else
builtin_error (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;

  builtin_error_prolog ();

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  va_end (args);
  fprintf (stderr, "\n");
}

void
#if defined (PREFER_STDARG)
builtin_warning (const char *format, ...)
#else
builtin_warning (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;

  builtin_error_prolog ();
  fprintf (stderr, _("warning: "));

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  va_end (args);
  fprintf (stderr, "\n");
}

 
void
builtin_usage ()
{
  if (this_command_name && *this_command_name)
    fprintf (stderr, _("%s: usage: "), this_command_name);
  fprintf (stderr, "%s\n", _(current_builtin->short_doc));
  fflush (stderr);
}

 
void
no_args (list)
     WORD_LIST *list;
{
  if (list)
    {
      builtin_error (_("too many arguments"));
      top_level_cleanup ();
      jump_to_top_level (DISCARD);
    }
}

 
int
no_options (list)
     WORD_LIST *list;
{
  int opt;

  reset_internal_getopt ();
  if ((opt = internal_getopt (list, "")) != -1)
    {
      if (opt == GETOPT_HELP)
	{
	  builtin_help ();
	  return (2);
	}
      builtin_usage ();
      return (1);
    }
  return (0);
}

void
sh_needarg (s)
     char *s;
{
  builtin_error (_("%s: option requires an argument"), s);
}

void
sh_neednumarg (s)
     char *s;
{
  builtin_error (_("%s: numeric argument required"), s);
}

void
sh_notfound (s)
     char *s;
{
  builtin_error (_("%s: not found"), s);
}

 
void
sh_invalidopt (s)
     char *s;
{
  builtin_error (_("%s: invalid option"), s);
}

void
sh_invalidoptname (s)
     char *s;
{
  builtin_error (_("%s: invalid option name"), s);
}

void
sh_invalidid (s)
     char *s;
{
  builtin_error (_("`%s': not a valid identifier"), s);
}

void
sh_invalidnum (s)
     char *s;
{
  char *msg;

  if (*s == '0' && isdigit ((unsigned char)s[1]))
    msg = _("invalid octal number");
  else if (*s == '0' && s[1] == 'x')
    msg = _("invalid hex number");
  else
    msg = _("invalid number");
  builtin_error ("%s: %s", s, msg);
}

void
sh_invalidsig (s)
     char *s;
{
  builtin_error (_("%s: invalid signal specification"), s);
}

void
sh_badpid (s)
     char *s;
{
  builtin_error (_("`%s': not a pid or valid job spec"), s);
}

void
sh_readonly (s)
     const char *s;
{
  builtin_error (_("%s: readonly variable"), s);
}

void
sh_noassign (s)
     const char *s;
{
  internal_error (_("%s: cannot assign"), s);	 
}

void
sh_erange (s, desc)
     char *s, *desc;
{
  if (s)
    builtin_error (_("%s: %s out of range"), s, desc ? desc : _("argument"));
  else
    builtin_error (_("%s out of range"), desc ? desc : _("argument"));
}

#if defined (JOB_CONTROL)
void
sh_badjob (s)
     char *s;
{
  builtin_error (_("%s: no such job"), s);
}

void
sh_nojobs (s)
     char *s;
{
  if (s)
    builtin_error (_("%s: no job control"), s);
  else
    builtin_error (_("no job control"));
}
#endif

#if defined (RESTRICTED_SHELL)
void
sh_restricted (s)
     char *s;
{
  if (s)
    builtin_error (_("%s: restricted"), s);
  else
    builtin_error (_("restricted"));
}
#endif

void
sh_notbuiltin (s)
     char *s;
{
  builtin_error (_("%s: not a shell builtin"), s);
}

void
sh_wrerror ()
{
#if defined (DONT_REPORT_BROKEN_PIPE_WRITE_ERRORS) && defined (EPIPE)
  if (errno != EPIPE)
#endif  
  builtin_error (_("write error: %s"), strerror (errno));
}

void
sh_ttyerror (set)
     int set;
{
  if (set)
    builtin_error (_("error setting terminal attributes: %s"), strerror (errno));
  else
    builtin_error (_("error getting terminal attributes: %s"), strerror (errno));
}

int
sh_chkwrite (s)
     int s;
{
  QUIT;
  fflush (stdout);
  QUIT;
  if (ferror (stdout))
    {
      sh_wrerror ();
      fpurge (stdout);
      clearerr (stdout);
      return (EXECUTION_FAILURE);
    }
  return (s);
}

 
 
 
 
 

 
char **
make_builtin_argv (list, ip)
     WORD_LIST *list;
     int *ip;
{
  char **argv;

  argv = strvec_from_word_list (list, 0, 1, ip);
  argv[0] = this_command_name;
  return argv;
}

 
void
remember_args (list, destructive)
     WORD_LIST *list;
     int destructive;
{
  register int i;

  posparam_count = 0;

  for (i = 1; i < 10; i++)
    {
      if ((destructive || list) && dollar_vars[i])
	{
	  free (dollar_vars[i]);
	  dollar_vars[i] = (char *)NULL;
	}

      if (list)
	{
	  dollar_vars[posparam_count = i] = savestring (list->word->word);
	  list = list->next;
	}
    }

   
  if (destructive || list)
    {
      dispose_words (rest_of_args);
      rest_of_args = copy_word_list (list);
      posparam_count += list_length (list);
    }

  if (destructive)
    set_dollar_vars_changed ();

  invalidate_cached_quoted_dollar_at ();
}

void
shift_args (times)
     int times;
{
  WORD_LIST *temp;
  int count;

  if (times <= 0)		 
    return;

  while (times-- > 0)
    {
      if (dollar_vars[1])
	free (dollar_vars[1]);

      for (count = 1; count < 9; count++)
	dollar_vars[count] = dollar_vars[count + 1];

      if (rest_of_args)
	{
	  temp = rest_of_args;
	  dollar_vars[9] = savestring (temp->word->word);
	  rest_of_args = rest_of_args->next;
	  temp->next = (WORD_LIST *)NULL;
	  dispose_words (temp);
	}
      else
	dollar_vars[9] = (char *)NULL;

      posparam_count--;
    }
}

int
number_of_args ()
{
#if 0
  register WORD_LIST *list;
  int n;

  for (n = 0; n < 9 && dollar_vars[n+1]; n++)
    ;
  for (list = rest_of_args; list; list = list->next)
    n++;

if (n != posparam_count)
  itrace("number_of_args: n (%d) != posparam_count (%d)", n, posparam_count);
#else
  return posparam_count;
#endif
}

static int changed_dollar_vars;

 
int
dollar_vars_changed ()
{
  return (changed_dollar_vars);
}

void
set_dollar_vars_unchanged ()
{
  changed_dollar_vars = 0;
}

void
set_dollar_vars_changed ()
{
  if (variable_context)
    changed_dollar_vars |= ARGS_FUNC;
  else if (this_shell_builtin == set_builtin)
    changed_dollar_vars |= ARGS_SETBLTIN;
  else
    changed_dollar_vars |= ARGS_INVOC;
}

 
 
 
 
 

 
int
get_numeric_arg (list, fatal, count)
     WORD_LIST *list;
     int fatal;
     intmax_t *count;
{
  char *arg;

  if (count)
    *count = 1;

  if (list && list->word && ISOPTION (list->word->word, '-'))
    list = list->next;

  if (list)
    {
      arg = list->word->word;
      if (arg == 0 || (legal_number (arg, count) == 0))
	{
	  sh_neednumarg (list->word->word ? list->word->word : "`'");
	  if (fatal == 0)
	    return 0;
	  else if (fatal == 1)		 
	    throw_to_top_level ();
	  else				 
	    {
	      top_level_cleanup ();
	      jump_to_top_level (DISCARD);
	    }
	}
      no_args (list->next);
    }

  return (1);
}

 
int
get_exitstat (list)
     WORD_LIST *list;
{
  int status;
  intmax_t sval;
  char *arg;

  if (list && list->word && ISOPTION (list->word->word, '-'))
    list = list->next;

  if (list == 0)
    {
       
      if (this_shell_builtin == return_builtin && running_trap > 0 && running_trap != DEBUG_TRAP+1)
	return (trap_saved_exit_value);
      return (last_command_exit_value);
    }

  arg = list->word->word;
  if (arg == 0 || legal_number (arg, &sval) == 0)
    {
      sh_neednumarg (list->word->word ? list->word->word : "`'");
      return EX_BADUSAGE;
    }
  no_args (list->next);

  status = sval & 255;
  return status;
}

 
int
read_octal (string)
     char *string;
{
  int result, digits;

  result = digits = 0;
  while (*string && ISOCTAL (*string))
    {
      digits++;
      result = (result * 8) + (*string++ - '0');
      if (result > 07777)
	return -1;
    }

  if (digits == 0 || *string)
    result = -1;

  return (result);
}

 
 
 
 
 

 
char *the_current_working_directory = (char *)NULL;

char *
get_working_directory (for_whom)
     char *for_whom;
{
  if (no_symbolic_links)
    {
      FREE (the_current_working_directory);
      the_current_working_directory = (char *)NULL;
    }

  if (the_current_working_directory == 0)
    {
#if defined (GETCWD_BROKEN)
      the_current_working_directory = getcwd (0, PATH_MAX);
#else
      the_current_working_directory = getcwd (0, 0);
#endif
      if (the_current_working_directory == 0)
	{
	  fprintf (stderr, _("%s: error retrieving current directory: %s: %s\n"),
		   (for_whom && *for_whom) ? for_whom : get_name_for_error (),
		   _(bash_getcwd_errstr), strerror (errno));
	  return (char *)NULL;
	}
    }

  return (savestring (the_current_working_directory));
}

 
void
set_working_directory (name)
     char *name;
{
  FREE (the_current_working_directory);
  the_current_working_directory = savestring (name);
}

 
 
 
 
 

#if defined (JOB_CONTROL)
int
get_job_by_name (name, flags)
     const char *name;
     int flags;
{
  register int i, wl, cl, match, job;
  register PROCESS *p;
  register JOB *j;

  job = NO_JOB;
  wl = strlen (name);
  for (i = js.j_jobslots - 1; i >= 0; i--)
    {
      j = get_job_by_jid (i);
      if (j == 0 || ((flags & JM_STOPPED) && J_JOBSTATE(j) != JSTOPPED))
        continue;

      p = j->pipe;
      do
        {
	  if (flags & JM_EXACT)
	    {
	      cl = strlen (p->command);
	      match = STREQN (p->command, name, cl);
	    }
	  else if (flags & JM_SUBSTRING)
	    match = strcasestr (p->command, name) != (char *)0;
	  else
	    match = STREQN (p->command, name, wl);

	  if (match == 0)
	    {
	      p = p->next;
	      continue;
	    }
	  else if (flags & JM_FIRSTMATCH)
	    return i;		 
	  else if (job != NO_JOB)
	    {
	      if (this_shell_builtin)
	        builtin_error (_("%s: ambiguous job spec"), name);
	      else
	        internal_error (_("%s: ambiguous job spec"), name);
	      return (DUP_JOB);
	    }
	  else
	    job = i;
        }
      while (p != j->pipe);
    }

  return (job);
}

 
int
get_job_spec (list)
     WORD_LIST *list;
{
  register char *word;
  int job, jflags;

  if (list == 0)
    return (js.j_current);

  word = list->word->word;

  if (*word == '\0')
    return (NO_JOB);

  if (*word == '%')
    word++;

  if (DIGIT (*word) && all_digits (word))
    {
      job = atoi (word);
      return ((job < 0 || job > js.j_jobslots) ? NO_JOB : job - 1);
    }

  jflags = 0;
  switch (*word)
    {
    case 0:
    case '%':
    case '+':
      return (js.j_current);

    case '-':
      return (js.j_previous);

    case '?':			 
      jflags |= JM_SUBSTRING;
      word++;
       

    default:
      return get_job_by_name (word, jflags);
    }
}
#endif  

 
int
display_signal_list (list, forcecols)
     WORD_LIST *list;
     int forcecols;
{
  register int i, column;
  char *name;
  int result, signum, dflags;
  intmax_t lsignum;

  result = EXECUTION_SUCCESS;
  if (!list)
    {
      for (i = 1, column = 0; i < NSIG; i++)
	{
	  name = signal_name (i);
	  if (STREQN (name, "SIGJUNK", 7) || STREQN (name, "Unknown", 7))
	    continue;

	  if (posixly_correct && !forcecols)
	    {
	       
	      if (STREQN (name, "SIG", 3))
		name += 3;
	      printf ("%s%s", name, (i == NSIG - 1) ? "" : " ");
	    }
	  else
	    {
	      printf ("%2d) %s", i, name);

	      if (++column < 5)
		printf ("\t");
	      else
		{
		  printf ("\n");
		  column = 0;
		}
	    }
	}

      if ((posixly_correct && !forcecols) || column != 0)
	printf ("\n");
      return result;
    }

   
  while (list)
    {
      if (legal_number (list->word->word, &lsignum))
	{
	   
	  if (lsignum > 128)
	    lsignum -= 128;
	  if (lsignum < 0 || lsignum >= NSIG)
	    {
	      sh_invalidsig (list->word->word);
	      result = EXECUTION_FAILURE;
	      list = list->next;
	      continue;
	    }

	  signum = lsignum;
	  name = signal_name (signum);
	  if (STREQN (name, "SIGJUNK", 7) || STREQN (name, "Unknown", 7))
	    {
	      list = list->next;
	      continue;
	    }
	   
	  printf ("%s\n", (this_shell_builtin == kill_builtin && signum > 0) ? name + 3 : name);
	}
      else
	{
	  dflags = DSIG_NOCASE;
	  if (posixly_correct == 0 || this_shell_builtin != kill_builtin)
	    dflags |= DSIG_SIGPREFIX;
	  signum = decode_signal (list->word->word, dflags);
	  if (signum == NO_SIG)
	    {
	      sh_invalidsig (list->word->word);
	      result = EXECUTION_FAILURE;
	      list = list->next;
	      continue;
	    }
	  printf ("%d\n", signum);
	}
      list = list->next;
    }
  return (result);
}

 
 
 
 
 

 
struct builtin *
builtin_address_internal (name, disabled_okay)
     char *name;
     int disabled_okay;
{
  int hi, lo, mid, j;

  hi = num_shell_builtins - 1;
  lo = 0;

  while (lo <= hi)
    {
      mid = (lo + hi) / 2;

      j = shell_builtins[mid].name[0] - name[0];

      if (j == 0)
	j = strcmp (shell_builtins[mid].name, name);

      if (j == 0)
	{
	   
	  if (shell_builtins[mid].function &&
	      ((shell_builtins[mid].flags & BUILTIN_DELETED) == 0) &&
	      ((shell_builtins[mid].flags & BUILTIN_ENABLED) || disabled_okay))
	    return (&shell_builtins[mid]);
	  else
	    return ((struct builtin *)NULL);
	}
      if (j > 0)
	hi = mid - 1;
      else
	lo = mid + 1;
    }
  return ((struct builtin *)NULL);
}

 
sh_builtin_func_t *
find_shell_builtin (name)
     char *name;
{
  current_builtin = builtin_address_internal (name, 0);
  return (current_builtin ? current_builtin->function : (sh_builtin_func_t *)NULL);
}

 
sh_builtin_func_t *
builtin_address (name)
     char *name;
{
  current_builtin = builtin_address_internal (name, 1);
  return (current_builtin ? current_builtin->function : (sh_builtin_func_t *)NULL);
}

 
sh_builtin_func_t *
find_special_builtin (name)
     char *name;
{
  current_builtin = builtin_address_internal (name, 0);
  return ((current_builtin && (current_builtin->flags & SPECIAL_BUILTIN)) ?
  			current_builtin->function :
  			(sh_builtin_func_t *)NULL);
}

static int
shell_builtin_compare (sbp1, sbp2)
     struct builtin *sbp1, *sbp2;
{
  int result;

  if ((result = sbp1->name[0] - sbp2->name[0]) == 0)
    result = strcmp (sbp1->name, sbp2->name);

  return (result);
}

 
void
initialize_shell_builtins ()
{
  qsort (shell_builtins, num_shell_builtins, sizeof (struct builtin),
    (QSFUNC *)shell_builtin_compare);
}

#if !defined (HELP_BUILTIN)
void
builtin_help ()
{
  printf ("%s: %s\n", this_command_name, _("help not available in this version"));
}
#endif

 
 
 
 
 

 
SHELL_VAR *
builtin_bind_variable (name, value, flags)
     char *name;
     char *value;
     int flags;
{
  SHELL_VAR *v;
  int vflags, bindflags;

#if defined (ARRAY_VARS)
   
  vflags = assoc_expand_once ? (VA_NOEXPAND|VA_ONEWORD) : 0;
  bindflags = flags | (assoc_expand_once ? ASS_NOEXPAND : 0) | ASS_ALLOWALLSUB;
  if (flags & ASS_NOEXPAND)
    vflags |= VA_NOEXPAND;
  if (flags & ASS_ONEWORD)
    vflags |= VA_ONEWORD;

  if (valid_array_reference (name, vflags) == 0)
    v = bind_variable (name, value, flags);
  else
    v = assign_array_element (name, value, bindflags, (array_eltstate_t *)0);
#else  
  v = bind_variable (name, value, flags);
#endif  

  if (v && readonly_p (v) == 0 && noassign_p (v) == 0)
    VUNSETATTR (v, att_invisible);

  return v;
}

SHELL_VAR *
builtin_bind_var_to_int (name, val, flags)
     char *name;
     intmax_t val;
     int flags;
{
  SHELL_VAR *v;

  v = bind_var_to_int (name, val, flags|ASS_ALLOWALLSUB);
  return v;
}

#if defined (ARRAY_VARS)
SHELL_VAR *
builtin_find_indexed_array (array_name, flags)
     char *array_name;
     int flags;
{
  SHELL_VAR *entry;

  if ((flags & 2) && legal_identifier (array_name) == 0)
    {
      sh_invalidid (array_name);
      return (SHELL_VAR *)NULL;
    }

  entry = find_or_make_array_variable (array_name, 1);
   
  if (entry == 0)
    return entry;
  else if (array_p (entry) == 0)
    {
      builtin_error (_("%s: not an indexed array"), array_name);
      return (SHELL_VAR *)NULL;
    }
  else if (invisible_p (entry))
    VUNSETATTR (entry, att_invisible);	 

  if (flags & 1)
    array_flush (array_cell (entry));

  return entry;
}
#endif  	

 
int
builtin_unbind_variable (vname)
     const char *vname;
{
  SHELL_VAR *v;

  v = find_variable (vname);
  if (v && readonly_p (v))
    {
      builtin_error (_("%s: cannot unset: readonly %s"), vname, "variable");
      return -2;
    }
  else if (v && non_unsettable_p (v))
    {
      builtin_error (_("%s: cannot unset"), vname);
      return -2;
    }
  return (unbind_variable (vname));
}

int
builtin_arrayref_flags (w, baseflags)
     WORD_DESC *w;
     int baseflags;
{
  char *t;
  int vflags;

  vflags = baseflags;

   
  if (w->flags & W_ARRAYREF)
    vflags |= VA_ONEWORD|VA_NOEXPAND;

#  if 0
   
  if (assoc_expand_once && (t =  strchr (w->word, '[')) && t[strlen(t) - 1] == ']')
    vflags |= VA_ONEWORD|VA_NOEXPAND;
#  endif

  return vflags;
}

 
 
 
 
 

#if defined (ARRAY_VARS)
int
set_expand_once (nval, uwp)
     int nval, uwp;
{
  int oa;

  oa = assoc_expand_once;
  if (shell_compatibility_level > 51)	 
    {
      if (uwp)
	unwind_protect_int (assoc_expand_once);
      assoc_expand_once = nval;
    }
  return oa;
}
#endif
