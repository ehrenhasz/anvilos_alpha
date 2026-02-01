 

 

#if !defined (_FLAGS_H_)
#define _FLAGS_H_

#include "stdc.h"

 
#define FLAG_ON '-'
#define FLAG_OFF '+'

#define FLAG_ERROR -1
#define FLAG_UNKNOWN (int *)0

 
struct flags_alist {
  char name;
  int *value;
};

extern const struct flags_alist shell_flags[];
extern char optflags[];

extern int
  mark_modified_vars, errexit_flag, exit_immediately_on_error,
  disallow_filename_globbing,
  place_keywords_in_env, read_but_dont_execute,
  just_one_command, unbound_vars_is_error, echo_input_at_read, verbose_flag,
  echo_command_at_execute, noclobber,
  hashing_enabled, forced_interactive, privileged_mode, jobs_m_flag,
  asynchronous_notification, interactive_comments, no_symbolic_links,
  function_trace_mode, error_trace_mode, pipefail_opt;

 
extern int want_pending_command, read_from_stdin;

#if 0
extern int lexical_scoping;
#endif

#if defined (BRACE_EXPANSION)
extern int brace_expansion;
#endif

#if defined (BANG_HISTORY)
extern int history_expansion;
extern int histexp_flag;
#endif  

#if defined (RESTRICTED_SHELL)
extern int restricted;
extern int restricted_shell;
#endif  

extern int *find_flag PARAMS((int));
extern int change_flag PARAMS((int, int));
extern char *which_set_flags PARAMS((void));
extern void reset_shell_flags PARAMS((void));

extern char *get_current_flags PARAMS((void));
extern void set_current_flags PARAMS((const char *));

extern void initialize_flags PARAMS((void));

 
#define change_flag_char(flag, on_or_off)  change_flag (flag, on_or_off)

#endif  
