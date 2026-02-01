 

 

#include "config.h"
#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "shell.h"
#include "execute_cmd.h"
#include "flags.h"

#if defined (BANG_HISTORY)
#  include "bashhist.h"
#endif

#if defined (JOB_CONTROL)
extern int set_job_control PARAMS((int));
#endif

 
 
 
 
 

 
int mark_modified_vars = 0;

 
int asynchronous_notification = 0;

 
int errexit_flag = 0;
int exit_immediately_on_error = 0;

 
int disallow_filename_globbing = 0;

 
int place_keywords_in_env = 0;

 
int read_but_dont_execute = 0;

 
int just_one_command = 0;

 
int noclobber = 0;

 
int unbound_vars_is_error = 0;

 
int echo_input_at_read = 0;
int verbose_flag = 0;

 
int echo_command_at_execute = 0;

 
int jobs_m_flag = 0;

 
int forced_interactive = 0;

 
int no_symbolic_links = 0;

 
 
 
 
 

#if 0
 
int lexical_scoping = 0;
#endif

 
int hashing_enabled = 1;

#if defined (BANG_HISTORY)
 
int history_expansion = HISTEXPAND_DEFAULT;
int histexp_flag = 0;
#endif  

 
int interactive_comments = 1;

#if defined (RESTRICTED_SHELL)
 
int restricted = 0;		 
int restricted_shell = 0;	 
#endif  

 
int privileged_mode = 0;

#if defined (BRACE_EXPANSION)
 
int brace_expansion = 1;
#endif

 
int function_trace_mode = 0;

 
int error_trace_mode = 0;

 
int pipefail_opt = 0;

 
 
 
 
 

const struct flags_alist shell_flags[] = {
   
  { 'a', &mark_modified_vars },
#if defined (JOB_CONTROL)
  { 'b', &asynchronous_notification },
#endif  
  { 'e', &errexit_flag },
  { 'f', &disallow_filename_globbing },
  { 'h', &hashing_enabled },
  { 'i', &forced_interactive },
  { 'k', &place_keywords_in_env },
#if defined (JOB_CONTROL)
  { 'm', &jobs_m_flag },
#endif  
  { 'n', &read_but_dont_execute },
  { 'p', &privileged_mode },
#if defined (RESTRICTED_SHELL)
  { 'r', &restricted },
#endif  
  { 't', &just_one_command },
  { 'u', &unbound_vars_is_error },
  { 'v', &verbose_flag },
  { 'x', &echo_command_at_execute },

   
#if 0
  { 'l', &lexical_scoping },
#endif
#if defined (BRACE_EXPANSION)
  { 'B', &brace_expansion },
#endif
  { 'C', &noclobber },
  { 'E', &error_trace_mode },
#if defined (BANG_HISTORY)
  { 'H', &histexp_flag },
#endif  
  { 'P', &no_symbolic_links },
  { 'T', &function_trace_mode },
  {0, (int *)NULL}
};

#define NUM_SHELL_FLAGS (sizeof (shell_flags) / sizeof (struct flags_alist))

char optflags[NUM_SHELL_FLAGS+4] = { '+' };

int *
find_flag (name)
     int name;
{
  int i;
  for (i = 0; shell_flags[i].name; i++)
    {
      if (shell_flags[i].name == name)
	return (shell_flags[i].value);
    }
  return (FLAG_UNKNOWN);
}

 
int
change_flag (flag, on_or_off)
  int flag;
  int on_or_off;
{
  int *value, old_value;

#if defined (RESTRICTED_SHELL)
   
  if (restricted && flag == 'r' && on_or_off == FLAG_OFF)
    return (FLAG_ERROR);
#endif  

  value = find_flag (flag);

  if ((value == (int *)FLAG_UNKNOWN) || (on_or_off != FLAG_ON && on_or_off != FLAG_OFF))
    return (FLAG_ERROR);

  old_value = *value;
  *value = (on_or_off == FLAG_ON) ? 1 : 0;

   
  switch (flag)
    {
#if defined (BANG_HISTORY)
    case 'H':
      history_expansion = histexp_flag;
      if (on_or_off == FLAG_ON)
	bash_initialize_history ();
      break;
#endif

#if defined (JOB_CONTROL)
    case 'm':
      set_job_control (on_or_off == FLAG_ON);
      break;
#endif  

    case 'e':
      if (builtin_ignoring_errexit == 0)
	exit_immediately_on_error = errexit_flag;
      break;

    case 'n':
      if (interactive_shell)
	read_but_dont_execute = 0;
      break;

    case 'p':
      if (on_or_off == FLAG_OFF)
	disable_priv_mode ();
      break;

#if defined (RESTRICTED_SHELL)
    case 'r':
      if (on_or_off == FLAG_ON && shell_initialized)
	maybe_make_restricted (shell_name);
      break;
#endif

    case 'v':
      echo_input_at_read = verbose_flag;
      break;
    }

  return (old_value);
}

 
char *
which_set_flags ()
{
  char *temp;
  int i, string_index;

  temp = (char *)xmalloc (1 + NUM_SHELL_FLAGS + read_from_stdin + want_pending_command);
  for (i = string_index = 0; shell_flags[i].name; i++)
    if (*(shell_flags[i].value))
      temp[string_index++] = shell_flags[i].name;

  if (want_pending_command)
    temp[string_index++] = 'c';
  if (read_from_stdin)
    temp[string_index++] = 's';

  temp[string_index] = '\0';
  return (temp);
}

char *
get_current_flags ()
{
  char *temp;
  int i;

  temp = (char *)xmalloc (1 + NUM_SHELL_FLAGS);
  for (i = 0; shell_flags[i].name; i++)
    temp[i] = *(shell_flags[i].value);
  temp[i] = '\0';
  return (temp);
}

void
set_current_flags (bitmap)
     const char *bitmap;
{
  int i;

  if (bitmap == 0)
    return;
  for (i = 0; shell_flags[i].name; i++)
    *(shell_flags[i].value) = bitmap[i];
}

void
reset_shell_flags ()
{
  mark_modified_vars = disallow_filename_globbing = 0;
  place_keywords_in_env = read_but_dont_execute = just_one_command = 0;
  noclobber = unbound_vars_is_error = 0;
  echo_command_at_execute = jobs_m_flag = forced_interactive = 0;
  no_symbolic_links = 0;
  privileged_mode = pipefail_opt = 0;

  error_trace_mode = function_trace_mode = 0;

  exit_immediately_on_error = errexit_flag = 0;
  echo_input_at_read = verbose_flag = 0;

  hashing_enabled = interactive_comments = 1;

#if defined (JOB_CONTROL)
  asynchronous_notification = 0;
#endif

#if defined (BANG_HISTORY)
  histexp_flag = 0;
#endif

#if defined (BRACE_EXPANSION)
  brace_expansion = 1;
#endif

#if defined (RESTRICTED_SHELL)
  restricted = 0;
#endif
}

void
initialize_flags ()
{
  register int i;

  for (i = 0; shell_flags[i].name; i++)
    optflags[i+1] = shell_flags[i].name;
  optflags[++i] = 'o';
  optflags[++i] = ';';
  optflags[i+1] = '\0';
}
