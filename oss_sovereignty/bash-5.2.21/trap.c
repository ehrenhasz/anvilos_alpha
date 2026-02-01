 

 

#include "config.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "bashtypes.h"
#include "bashansi.h"

#include <stdio.h>
#include <errno.h>

#include "bashintl.h"

#include <signal.h>

#include "trap.h"

#include "shell.h"
#include "execute_cmd.h"
#include "flags.h"
#include "parser.h"
#include "input.h"	 
#include "jobs.h"
#include "signames.h"
#include "builtins.h"
#include "builtins/common.h"
#include "builtins/builtext.h"

#if defined (READLINE)
#  include <readline/readline.h>
#  include "bashline.h"
#endif

#ifndef errno
extern int errno;
#endif

 
#define SIG_INHERITED   0x0	 
#define SIG_TRAPPED     0x1	 
#define SIG_HARD_IGNORE 0x2	 
#define SIG_SPECIAL     0x4	 
#define SIG_NO_TRAP     0x8	 
#define SIG_INPROGRESS	0x10	 
#define SIG_CHANGED	0x20	 
#define SIG_IGNORED	0x40	 

#define SPECIAL_TRAP(s)	((s) == EXIT_TRAP || (s) == DEBUG_TRAP || (s) == ERROR_TRAP || (s) == RETURN_TRAP)

 
static int sigmodes[BASH_NSIG];

static void free_trap_command (int);
static void change_signal (int, char *);

static int _run_trap_internal (int, char *);

static void free_trap_string (int);
static void reset_signal (int);
static void restore_signal (int);
static void reset_or_restore_signal_handlers (sh_resetsig_func_t *);
static void reinit_trap (int);

static void trap_if_untrapped (int, char *);

 

extern volatile int from_return_trap;
extern int waiting_for_child;

extern WORD_LIST *subst_assign_varlist;

 
SigHandler *original_signals[NSIG];

 
char *trap_list[BASH_NSIG];

 
int pending_traps[NSIG];

 
int running_trap;

 
int trap_saved_exit_value;

 
int wait_signal_received;

int trapped_signal_received;

 
int suppress_debug_trap_verbose = 0;

#define GETORIGSIG(sig) \
  do { \
    original_signals[sig] = (SigHandler *)set_signal_handler (sig, SIG_DFL); \
    set_signal_handler (sig, original_signals[sig]); \
    if (original_signals[sig] == SIG_IGN) \
      sigmodes[sig] |= SIG_HARD_IGNORE; \
  } while (0)

#define SETORIGSIG(sig,handler) \
  do { \
    original_signals[sig] = handler; \
    if (original_signals[sig] == SIG_IGN) \
      sigmodes[sig] |= SIG_HARD_IGNORE; \
  } while (0)

#define GET_ORIGINAL_SIGNAL(sig) \
  if (sig && sig < NSIG && original_signals[sig] == IMPOSSIBLE_TRAP_HANDLER) \
    GETORIGSIG(sig)

void
initialize_traps ()
{
  register int i;

  initialize_signames();

  trap_list[EXIT_TRAP] = trap_list[DEBUG_TRAP] = trap_list[ERROR_TRAP] = trap_list[RETURN_TRAP] = (char *)NULL;
  sigmodes[EXIT_TRAP] = sigmodes[DEBUG_TRAP] = sigmodes[ERROR_TRAP] = sigmodes[RETURN_TRAP] = SIG_INHERITED;
  original_signals[EXIT_TRAP] = IMPOSSIBLE_TRAP_HANDLER;

  for (i = 1; i < NSIG; i++)
    {
      pending_traps[i] = 0;
      trap_list[i] = (char *)DEFAULT_SIG;
      sigmodes[i] = SIG_INHERITED;	 
      original_signals[i] = IMPOSSIBLE_TRAP_HANDLER;
    }

   
#if defined (SIGCHLD)
  GETORIGSIG (SIGCHLD);
  sigmodes[SIGCHLD] |= (SIG_SPECIAL | SIG_NO_TRAP);
#endif  

  GETORIGSIG (SIGINT);
  sigmodes[SIGINT] |= SIG_SPECIAL;

#if defined (__BEOS__)
   
  original_signals[SIGINT] = SIG_DFL;
  sigmodes[SIGINT] &= ~SIG_HARD_IGNORE;
#endif

  GETORIGSIG (SIGQUIT);
  sigmodes[SIGQUIT] |= SIG_SPECIAL;

  if (interactive)
    {
      GETORIGSIG (SIGTERM);
      sigmodes[SIGTERM] |= SIG_SPECIAL;
    }

  get_original_tty_job_signals ();
}

#ifdef DEBUG
 
static char *
trap_handler_string (sig)
     int sig;
{
  if (trap_list[sig] == (char *)DEFAULT_SIG)
    return "DEFAULT_SIG";
  else if (trap_list[sig] == (char *)IGNORE_SIG)
    return "IGNORE_SIG";
  else if (trap_list[sig] == (char *)IMPOSSIBLE_TRAP_HANDLER)
    return "IMPOSSIBLE_TRAP_HANDLER";
  else if (trap_list[sig])
    return trap_list[sig];
  else
    return "NULL";
}
#endif

 
char *
signal_name (sig)
     int sig;
{
  char *ret;

   
  ret = (sig >= BASH_NSIG || sig < 0 || signal_names[sig] == NULL)
	? _("invalid signal number")
	: signal_names[sig];

  return ret;
}

 
int
decode_signal (string, flags)
     char *string;
     int flags;
{
  intmax_t sig;
  char *name;

  if (legal_number (string, &sig))
    return ((sig >= 0 && sig < NSIG) ? (int)sig : NO_SIG);

#if defined (SIGRTMIN) && defined (SIGRTMAX)
  if (STREQN (string, "SIGRTMIN+", 9) || ((flags & DSIG_NOCASE) && strncasecmp (string, "SIGRTMIN+", 9) == 0))
    {
      if (legal_number (string+9, &sig) && sig >= 0 && sig <= SIGRTMAX - SIGRTMIN)
	return (SIGRTMIN + sig);
      else
	return NO_SIG;
    }
  else if (STREQN (string, "RTMIN+", 6) || ((flags & DSIG_NOCASE) && strncasecmp (string, "RTMIN+", 6) == 0))
    {
      if (legal_number (string+6, &sig) && sig >= 0 && sig <= SIGRTMAX - SIGRTMIN)
	return (SIGRTMIN + sig);
      else
	return NO_SIG;
    }
#endif  

   
  for (sig = 0; sig < BASH_NSIG; sig++)
    {
      name = signal_names[sig];
      if (name == 0 || name[0] == '\0')
	continue;

       
      if (STREQN (name, "SIG", 3))
	{
	  name += 3;

	  if ((flags & DSIG_NOCASE) && strcasecmp (string, name) == 0)
	    return ((int)sig);
	  else if ((flags & DSIG_NOCASE) == 0 && strcmp (string, name) == 0)
	    return ((int)sig);
	   
	  else if ((flags & DSIG_SIGPREFIX) == 0)
	    continue;
	}

       
      name = signal_names[sig];
      if ((flags & DSIG_NOCASE) && strcasecmp (string, name) == 0)
	return ((int)sig);
      else if ((flags & DSIG_NOCASE) == 0 && strcmp (string, name) == 0)
	return ((int)sig);
    }

  return (NO_SIG);
}

 
static int catch_flag;

void
run_pending_traps ()
{
  register int sig;
  int x;
  volatile int old_exit_value, old_running;
  WORD_LIST *save_subst_varlist;
  HASH_TABLE *save_tempenv;
  sh_parser_state_t pstate;
  volatile int save_return_catch_flag, function_code;
  procenv_t save_return_catch;
  char *trap_command, *old_trap;
#if defined (ARRAY_VARS)
  ARRAY *ps;
#endif

  if (catch_flag == 0)		 
    return;

  if (running_trap > 0)
    {
      internal_debug ("run_pending_traps: recursive invocation while running trap for signal %d", running_trap-1);
#if defined (SIGWINCH)
      if (running_trap == SIGWINCH+1 && pending_traps[SIGWINCH])
	return;			 
#endif
       
      if (evalnest_max > 0 && evalnest > evalnest_max)
	{
	  internal_error (_("trap handler: maximum trap handler level exceeded (%d)"), evalnest_max);
	  evalnest = 0;
	  jump_to_top_level (DISCARD);
	}
    }

  catch_flag = trapped_signal_received = 0;

   
  trap_saved_exit_value = old_exit_value = last_command_exit_value;
#if defined (ARRAY_VARS)
  ps = save_pipestatus_array ();
#endif
  old_running = running_trap;

  for (sig = 1; sig < NSIG; sig++)
    {
       
      if (pending_traps[sig])
	{
	   
	  running_trap = sig + 1;

	  if (sig == SIGINT)
	    {
	      pending_traps[sig] = 0;	 
	       
	      run_interrupt_trap (0);
	      CLRINTERRUPT;	 
	    }
#if defined (JOB_CONTROL) && defined (SIGCHLD)
	  else if (sig == SIGCHLD &&
		   trap_list[SIGCHLD] != (char *)IMPOSSIBLE_TRAP_HANDLER &&
		   (sigmodes[SIGCHLD] & SIG_INPROGRESS) == 0)
	    {
	      sigmodes[SIGCHLD] |= SIG_INPROGRESS;
	       
	      evalnest++;
	      x = pending_traps[sig];
	      pending_traps[sig] = 0;
	      run_sigchld_trap (x);	 
	      running_trap = 0;
	      evalnest--;
	      sigmodes[SIGCHLD] &= ~SIG_INPROGRESS;
	       
	      continue;
	    }
	  else if (sig == SIGCHLD &&
		   trap_list[SIGCHLD] == (char *)IMPOSSIBLE_TRAP_HANDLER &&
		   (sigmodes[SIGCHLD] & SIG_INPROGRESS) != 0)
	    {
	       
	      running_trap = 0;
	       
	      continue;					 
	    }
	  else if (sig == SIGCHLD && (sigmodes[SIGCHLD] & SIG_INPROGRESS))
	    {
	       
	      running_trap = 0;		 
	       
	      continue;
	    }
#endif
	  else if (trap_list[sig] == (char *)DEFAULT_SIG ||
		   trap_list[sig] == (char *)IGNORE_SIG ||
		   trap_list[sig] == (char *)IMPOSSIBLE_TRAP_HANDLER)
	    {
	       
	      internal_warning (_("run_pending_traps: bad value in trap_list[%d]: %p"),
				sig, trap_list[sig]);
	      if (trap_list[sig] == (char *)DEFAULT_SIG)
		{
		  internal_warning (_("run_pending_traps: signal handler is SIG_DFL, resending %d (%s) to myself"), sig, signal_name (sig));
		  kill (getpid (), sig);
		}
	    }
	  else
	    {
	      old_trap = trap_list[sig];
	      trap_command = savestring (old_trap);

	      save_parser_state (&pstate);
	      save_subst_varlist = subst_assign_varlist;
	      subst_assign_varlist = 0;
	      save_tempenv = temporary_env;
	      temporary_env = 0;	 

#if defined (JOB_CONTROL)
	      save_pipeline (1);	 
#endif
	       
	      pending_traps[sig] = 0;
	      evalnest++;

	      function_code = 0;
	      save_return_catch_flag = return_catch_flag;
	      if (return_catch_flag)
		{
		  COPY_PROCENV (return_catch, save_return_catch);
		  function_code = setjmp_nosigs (return_catch);
		}

	      if (function_code == 0)
	         
		x = parse_and_execute (trap_command, "trap", SEVAL_NONINT|SEVAL_NOHIST|SEVAL_RESETLINE|SEVAL_NOOPTIMIZE);
	      else
		{
		  parse_and_execute_cleanup (sig + 1);	 
		  x = return_catch_value;
		}

	      evalnest--;
#if defined (JOB_CONTROL)
	      restore_pipeline (1);
#endif

	      subst_assign_varlist = save_subst_varlist;
	      restore_parser_state (&pstate);
	      temporary_env = save_tempenv;

	      if (save_return_catch_flag)
		{
		  return_catch_flag = save_return_catch_flag;
		  return_catch_value = x;
		  COPY_PROCENV (save_return_catch, return_catch);
		  if (function_code)
		    {
		      running_trap = old_running;		 
		       
		      sh_longjmp (return_catch, 1);
		    }
		}
	    }

	  pending_traps[sig] = 0;	 
	  running_trap = old_running;
	}
    }

#if defined (ARRAY_VARS)
  restore_pipestatus_array (ps);
#endif
  last_command_exit_value = old_exit_value;
}

 
void
set_trap_state (sig)
     int sig;
{
  catch_flag = 1;
  pending_traps[sig]++;
  trapped_signal_received = sig;
}
    
sighandler
trap_handler (sig)
     int sig;
{
  int oerrno;

  if ((sigmodes[sig] & SIG_TRAPPED) == 0)
    {
      internal_debug ("trap_handler: signal %d: signal not trapped", sig);
      SIGRETURN (0);
    }

   
  if ((subshell_environment & SUBSHELL_IGNTRAP) && trap_list[sig] != (char *)IGNORE_SIG)
    {
      sigset_t mask;

       
      if (original_signals[sig] == IMPOSSIBLE_TRAP_HANDLER)
	original_signals[sig] = SIG_DFL;

      restore_signal (sig);

       
      sigemptyset (&mask);
      sigprocmask (SIG_SETMASK, (sigset_t *)NULL, &mask);
      sigdelset (&mask, sig);
      sigprocmask (SIG_SETMASK, &mask, (sigset_t *)NULL);

      kill (getpid (), sig);

      SIGRETURN (0);
    }

  if ((sig >= NSIG) ||
      (trap_list[sig] == (char *)DEFAULT_SIG) ||
      (trap_list[sig] == (char *)IGNORE_SIG))
    programming_error (_("trap_handler: bad signal %d"), sig);
  else
    {
      oerrno = errno;
#if defined (MUST_REINSTALL_SIGHANDLERS)
#  if defined (JOB_CONTROL) && defined (SIGCHLD)
      if (sig != SIGCHLD)
#  endif  
      set_signal_handler (sig, trap_handler);
#endif  

      set_trap_state (sig);

      if (this_shell_builtin && (this_shell_builtin == wait_builtin))
	{
	  wait_signal_received = sig;
	  if (waiting_for_child && wait_intr_flag)
	    sh_longjmp (wait_intr_buf, 1);
	}

#if defined (READLINE)
       
      if (RL_ISSTATE (RL_STATE_SIGHANDLER))
        bashline_set_event_hook ();
#endif

      errno = oerrno;
    }
  
  SIGRETURN (0);
}

int
next_pending_trap (start)
     int start;
{
  register int i;

  for (i = start; i < NSIG; i++)
    if (pending_traps[i])
      return i;
  return -1;
}

int
first_pending_trap ()
{
  return (next_pending_trap (1));
}

 
int
any_signals_trapped ()
{
  register int i;

  for (i = 1; i < NSIG; i++)
    if ((sigmodes[i] & SIG_TRAPPED) && (sigmodes[i] & SIG_IGNORED) == 0)
      return i;
  return -1;
}

void
clear_pending_traps ()
{
  register int i;

  for (i = 1; i < NSIG; i++)
    pending_traps[i] = 0;
}

void
check_signals ()
{
   
  check_read_timeout ();	 
  QUIT;
}

 
void
check_signals_and_traps ()
{
  check_signals ();

  run_pending_traps ();
}

#if defined (JOB_CONTROL) && defined (SIGCHLD)

#ifdef INCLUDE_UNUSED
 
void
set_sigchld_trap (command_string)
     char *command_string;
{
  set_signal (SIGCHLD, command_string);
}
#endif

 
void
maybe_set_sigchld_trap (command_string)
     char *command_string;
{
  if ((sigmodes[SIGCHLD] & SIG_TRAPPED) == 0 && trap_list[SIGCHLD] == (char *)IMPOSSIBLE_TRAP_HANDLER)
    set_signal (SIGCHLD, command_string);
}

 
void
set_impossible_sigchld_trap ()
{
  restore_default_signal (SIGCHLD);
  change_signal (SIGCHLD, (char *)IMPOSSIBLE_TRAP_HANDLER);
  sigmodes[SIGCHLD] &= ~SIG_TRAPPED;	 
}

 
void
queue_sigchld_trap (nchild)
     int nchild;
{
  if (nchild > 0)
    {
      catch_flag = 1;
      pending_traps[SIGCHLD] += nchild;
      trapped_signal_received = SIGCHLD;
    }
}
#endif  

 
static inline void
trap_if_untrapped (sig, command)
     int sig;
     char *command;
{
  if ((sigmodes[sig] & SIG_TRAPPED) == 0)
    set_signal (sig, command);
}

void
set_debug_trap (command)
     char *command;
{
  set_signal (DEBUG_TRAP, command);
}

 
void
maybe_set_debug_trap (command)
     char *command;
{
  trap_if_untrapped (DEBUG_TRAP, command);
}

void
set_error_trap (command)
     char *command;
{
  set_signal (ERROR_TRAP, command);
}

void
maybe_set_error_trap (command)
     char *command;
{
  trap_if_untrapped (ERROR_TRAP, command);
}

void
set_return_trap (command)
     char *command;
{
  set_signal (RETURN_TRAP, command);
}

void
maybe_set_return_trap (command)
     char *command;
{
  trap_if_untrapped (RETURN_TRAP, command);
}

#ifdef INCLUDE_UNUSED
void
set_sigint_trap (command)
     char *command;
{
  set_signal (SIGINT, command);
}
#endif

 
SigHandler *
set_sigint_handler ()
{
  if (sigmodes[SIGINT] & SIG_HARD_IGNORE)
    return ((SigHandler *)SIG_IGN);

  else if (sigmodes[SIGINT] & SIG_IGNORED)
    return ((SigHandler *)set_signal_handler (SIGINT, SIG_IGN));  

  else if (sigmodes[SIGINT] & SIG_TRAPPED)
    return ((SigHandler *)set_signal_handler (SIGINT, trap_handler));

   
  else if (interactive)	 
    return (set_signal_handler (SIGINT, sigint_sighandler));
  else
    return (set_signal_handler (SIGINT, termsig_sighandler));
}

 
SigHandler *
trap_to_sighandler (sig)
     int sig;
{
  if (sigmodes[sig] & (SIG_IGNORED|SIG_HARD_IGNORE))
    return (SIG_IGN);
  else if (sigmodes[sig] & SIG_TRAPPED)
    return (trap_handler);
  else
    return (SIG_DFL);
}

 
void
set_signal (sig, string)
     int sig;
     char *string;
{
  sigset_t set, oset;

  if (SPECIAL_TRAP (sig))
    {
      change_signal (sig, savestring (string));
      if (sig == EXIT_TRAP && interactive == 0)
	initialize_terminating_signals ();
      return;
    }

   
  if (sigmodes[sig] & SIG_HARD_IGNORE)
    return;

   
  if ((sigmodes[sig] & SIG_TRAPPED) == 0)
    {
       
      if (original_signals[sig] == IMPOSSIBLE_TRAP_HANDLER)
        GETORIGSIG (sig);
      if (original_signals[sig] == SIG_IGN)
	return;
    }

   
  if ((sigmodes[sig] & SIG_NO_TRAP) == 0)
    {
      BLOCK_SIGNAL (sig, set, oset);
      change_signal (sig, savestring (string));
      set_signal_handler (sig, trap_handler);
      UNBLOCK_SIGNAL (oset);
    }
  else
    change_signal (sig, savestring (string));
}

static void
free_trap_command (sig)
     int sig;
{
  if ((sigmodes[sig] & SIG_TRAPPED) && trap_list[sig] &&
      (trap_list[sig] != (char *)IGNORE_SIG) &&
      (trap_list[sig] != (char *)DEFAULT_SIG) &&
      (trap_list[sig] != (char *)IMPOSSIBLE_TRAP_HANDLER))
    free (trap_list[sig]);
}

 
static void
change_signal (sig, value)
     int sig;
     char *value;
{
  if ((sigmodes[sig] & SIG_INPROGRESS) == 0)
    free_trap_command (sig);
  trap_list[sig] = value;

  sigmodes[sig] |= SIG_TRAPPED;
  if (value == (char *)IGNORE_SIG)
    sigmodes[sig] |= SIG_IGNORED;
  else
    sigmodes[sig] &= ~SIG_IGNORED;
  if (sigmodes[sig] & SIG_INPROGRESS)
    sigmodes[sig] |= SIG_CHANGED;
}

void
get_original_signal (sig)
     int sig;
{
   
  if (sig > 0 && sig < NSIG && original_signals[sig] == (SigHandler *)IMPOSSIBLE_TRAP_HANDLER)
    GETORIGSIG (sig);
}

void
get_all_original_signals ()
{
  register int i;

  for (i = 1; i < NSIG; i++)
    GET_ORIGINAL_SIGNAL (i);
}

void
set_original_signal (sig, handler)
     int sig;
     SigHandler *handler;
{
  if (sig > 0 && sig < NSIG && original_signals[sig] == (SigHandler *)IMPOSSIBLE_TRAP_HANDLER)
    SETORIGSIG (sig, handler);
}

 
void
restore_default_signal (sig)
     int sig;
{
  if (SPECIAL_TRAP (sig))
    {
      if ((sig != DEBUG_TRAP && sig != ERROR_TRAP && sig != RETURN_TRAP) ||
	  (sigmodes[sig] & SIG_INPROGRESS) == 0)
	free_trap_command (sig);
      trap_list[sig] = (char *)NULL;
      sigmodes[sig] &= ~SIG_TRAPPED;
      if (sigmodes[sig] & SIG_INPROGRESS)
	sigmodes[sig] |= SIG_CHANGED;
      return;
    }

  GET_ORIGINAL_SIGNAL (sig);

   
  if (sigmodes[sig] & SIG_HARD_IGNORE)
    return;

   
   
  if (((sigmodes[sig] & SIG_TRAPPED) == 0) &&
      (sig != SIGCHLD || (sigmodes[sig] & SIG_INPROGRESS) == 0 || trap_list[sig] != (char *)IMPOSSIBLE_TRAP_HANDLER))
    return;

   
  if ((sigmodes[sig] & SIG_NO_TRAP) == 0)
    set_signal_handler (sig, original_signals[sig]);

   
  change_signal (sig, (char *)DEFAULT_SIG);

   
  sigmodes[sig] &= ~SIG_TRAPPED;
}

 
void
ignore_signal (sig)
     int sig;
{
  if (SPECIAL_TRAP (sig) && ((sigmodes[sig] & SIG_IGNORED) == 0))
    {
      change_signal (sig, (char *)IGNORE_SIG);
      return;
    }

  GET_ORIGINAL_SIGNAL (sig);

   
  if (sigmodes[sig] & SIG_HARD_IGNORE)
    return;

   
  if (sigmodes[sig] & SIG_IGNORED)
    return;

   
  if ((sigmodes[sig] & SIG_NO_TRAP) == 0)
    set_signal_handler (sig, SIG_IGN);

   
  change_signal (sig, (char *)IGNORE_SIG);
}

 
int
run_exit_trap ()
{
  char *trap_command;
  int code, function_code, retval;
#if defined (ARRAY_VARS)
  ARRAY *ps;
#endif

  trap_saved_exit_value = last_command_exit_value;
#if defined (ARRAY_VARS)
  ps = save_pipestatus_array ();
#endif
  function_code = 0;

   
  if ((sigmodes[EXIT_TRAP] & SIG_TRAPPED) &&
      (sigmodes[EXIT_TRAP] & (SIG_IGNORED|SIG_INPROGRESS)) == 0)
    {
      trap_command = savestring (trap_list[EXIT_TRAP]);
      sigmodes[EXIT_TRAP] &= ~SIG_TRAPPED;
      sigmodes[EXIT_TRAP] |= SIG_INPROGRESS;

      retval = trap_saved_exit_value;
      running_trap = 1;

      code = setjmp_nosigs (top_level);

       
      if (return_catch_flag)
	function_code = setjmp_nosigs (return_catch);

      if (code == 0 && function_code == 0)
	{
	  reset_parser ();
	  parse_and_execute (trap_command, "exit trap", SEVAL_NONINT|SEVAL_NOHIST|SEVAL_RESETLINE|SEVAL_NOOPTIMIZE);
	}
      else if (code == ERREXIT)
	retval = last_command_exit_value;
      else if (code == EXITPROG || code == EXITBLTIN)
	retval = last_command_exit_value;
      else if (function_code != 0)
        retval = return_catch_value;
      else
	retval = trap_saved_exit_value;

      running_trap = 0;
#if defined (ARRAY_VARS)
      array_dispose (ps);
#endif

      return retval;
    }

#if defined (ARRAY_VARS)
  restore_pipestatus_array (ps);
#endif
  return (trap_saved_exit_value);
}

void
run_trap_cleanup (sig)
     int sig;
{
   
  sigmodes[sig] &= ~(SIG_INPROGRESS|SIG_CHANGED);
}

#define RECURSIVE_SIG(s) (SPECIAL_TRAP(s) == 0)

 
static int
_run_trap_internal (sig, tag)
     int sig;
     char *tag;
{
  char *trap_command, *old_trap;
  int trap_exit_value;
  volatile int save_return_catch_flag, function_code;
  int old_modes, old_running, old_int;
  int flags;
  procenv_t save_return_catch;
  WORD_LIST *save_subst_varlist;
  HASH_TABLE *save_tempenv;
  sh_parser_state_t pstate;
#if defined (ARRAY_VARS)
  ARRAY *ps;
#endif

  old_modes = old_running = -1;

  trap_exit_value = function_code = 0;
  trap_saved_exit_value = last_command_exit_value;
   
  if ((sigmodes[sig] & SIG_TRAPPED) && ((sigmodes[sig] & SIG_IGNORED) == 0) &&
      (trap_list[sig] != (char *)IMPOSSIBLE_TRAP_HANDLER) &&
#if 1
       
      (RECURSIVE_SIG (sig) || (sigmodes[sig] & SIG_INPROGRESS) == 0))
#else
      ((sigmodes[sig] & SIG_INPROGRESS) == 0))
#endif
    {
      old_trap = trap_list[sig];
      old_modes = sigmodes[sig];
      old_running = running_trap;

      sigmodes[sig] |= SIG_INPROGRESS;
      sigmodes[sig] &= ~SIG_CHANGED;		 
      trap_command =  savestring (old_trap);

      running_trap = sig + 1;

      old_int = interrupt_state;	 
      CLRINTERRUPT;

#if defined (ARRAY_VARS)
      ps = save_pipestatus_array ();
#endif

      save_parser_state (&pstate);
      save_subst_varlist = subst_assign_varlist;
      subst_assign_varlist = 0;
      save_tempenv = temporary_env;
      temporary_env = 0;	 

#if defined (JOB_CONTROL)
      if (sig != DEBUG_TRAP)	 
	save_pipeline (1);	 
#endif

       
      save_return_catch_flag = return_catch_flag;
      if (return_catch_flag)
	{
	  COPY_PROCENV (return_catch, save_return_catch);
	  function_code = setjmp_nosigs (return_catch);
	}

      flags = SEVAL_NONINT|SEVAL_NOHIST|SEVAL_NOOPTIMIZE;
      if (sig != DEBUG_TRAP && sig != RETURN_TRAP && sig != ERROR_TRAP)
	flags |= SEVAL_RESETLINE;
      evalnest++;
      if (function_code == 0)
        {
	  parse_and_execute (trap_command, tag, flags);
	  trap_exit_value = last_command_exit_value;
        }
      else
        trap_exit_value = return_catch_value;
      evalnest--;

#if defined (JOB_CONTROL)
      if (sig != DEBUG_TRAP)	 
	restore_pipeline (1);
#endif

      subst_assign_varlist = save_subst_varlist;
      restore_parser_state (&pstate);

#if defined (ARRAY_VARS)
      restore_pipestatus_array (ps);
#endif

      temporary_env = save_tempenv;

      if ((old_modes & SIG_INPROGRESS) == 0)
	sigmodes[sig] &= ~SIG_INPROGRESS;

      running_trap = old_running;
      interrupt_state = old_int;

      if (sigmodes[sig] & SIG_CHANGED)
	{
#if 0
	   
	  if (SPECIAL_TRAP (sig) == 0)
#endif
	    free (old_trap);
	  sigmodes[sig] &= ~SIG_CHANGED;

	  CHECK_TERMSIG;	 
	}

      if (save_return_catch_flag)
	{
	  return_catch_flag = save_return_catch_flag;
	  return_catch_value = trap_exit_value;
	  COPY_PROCENV (save_return_catch, return_catch);
	  if (function_code)
	    {
#if 0
	      from_return_trap = sig == RETURN_TRAP;
#endif
	      sh_longjmp (return_catch, 1);
	    }
	}
    }

  return trap_exit_value;
}

int
run_debug_trap ()
{
  int trap_exit_value, old_verbose;
  pid_t save_pgrp;
#if defined (PGRP_PIPE)
  int save_pipe[2];
#endif

   
  trap_exit_value = 0;
  if ((sigmodes[DEBUG_TRAP] & SIG_TRAPPED) && ((sigmodes[DEBUG_TRAP] & SIG_IGNORED) == 0) && ((sigmodes[DEBUG_TRAP] & SIG_INPROGRESS) == 0))
    {
#if defined (JOB_CONTROL)
      save_pgrp = pipeline_pgrp;
      pipeline_pgrp = 0;
      save_pipeline (1);
#  if defined (PGRP_PIPE)
      save_pgrp_pipe (save_pipe, 1);
#  endif
      stop_making_children ();
#endif

      old_verbose = echo_input_at_read;
      echo_input_at_read = suppress_debug_trap_verbose ? 0 : echo_input_at_read;

      trap_exit_value = _run_trap_internal (DEBUG_TRAP, "debug trap");

      echo_input_at_read = old_verbose;

#if defined (JOB_CONTROL)
      pipeline_pgrp = save_pgrp;
      restore_pipeline (1);
#  if defined (PGRP_PIPE)
      close_pgrp_pipe ();
      restore_pgrp_pipe (save_pipe);
#  endif
      if (pipeline_pgrp > 0 && ((subshell_environment & (SUBSHELL_ASYNC|SUBSHELL_PIPE)) == 0))
	give_terminal_to (pipeline_pgrp, 1);

      notify_and_cleanup ();
#endif
      
#if defined (DEBUGGER)
       
      if (debugging_mode && trap_exit_value == 2 && return_catch_flag)
	{
	  return_catch_value = trap_exit_value;
	  sh_longjmp (return_catch, 1);
	}
#endif
    }
  return trap_exit_value;
}

void
run_error_trap ()
{
  if ((sigmodes[ERROR_TRAP] & SIG_TRAPPED) && ((sigmodes[ERROR_TRAP] & SIG_IGNORED) == 0) && (sigmodes[ERROR_TRAP] & SIG_INPROGRESS) == 0)
    _run_trap_internal (ERROR_TRAP, "error trap");
}

void
run_return_trap ()
{
  int old_exit_value;

#if 0
  if ((sigmodes[DEBUG_TRAP] & SIG_TRAPPED) && (sigmodes[DEBUG_TRAP] & SIG_INPROGRESS))
    return;
#endif

  if ((sigmodes[RETURN_TRAP] & SIG_TRAPPED) && ((sigmodes[RETURN_TRAP] & SIG_IGNORED) == 0) && (sigmodes[RETURN_TRAP] & SIG_INPROGRESS) == 0)
    {
      old_exit_value = last_command_exit_value;
      _run_trap_internal (RETURN_TRAP, "return trap");
      last_command_exit_value = old_exit_value;
    }
}

 
void
run_interrupt_trap (will_throw)
     int will_throw;	 
{
  if (will_throw && running_trap > 0)
    run_trap_cleanup (running_trap - 1);
  pending_traps[SIGINT] = 0;	 
  catch_flag = 0;
  _run_trap_internal (SIGINT, "interrupt trap");
}

 
void
free_trap_strings ()
{
  register int i;

  for (i = 0; i < NSIG; i++)
    {
      if (trap_list[i] != (char *)IGNORE_SIG)
	free_trap_string (i);
    }
  for (i = NSIG; i < BASH_NSIG; i++)
    {
       
      if ((sigmodes[i] & SIG_TRAPPED) == 0)
	{
	  free_trap_string (i);
	  trap_list[i] = (char *)NULL;
	}
    }
}

 
static void
free_trap_string (sig)
     int sig;
{
  change_signal (sig, (char *)DEFAULT_SIG);
  sigmodes[sig] &= ~SIG_TRAPPED;		 
}

 
static void
reset_signal (sig)
     int sig;
{
  set_signal_handler (sig, original_signals[sig]);
  sigmodes[sig] &= ~SIG_TRAPPED;		 
}

 
static void
restore_signal (sig)
     int sig;
{
  set_signal_handler (sig, original_signals[sig]);
  change_signal (sig, (char *)DEFAULT_SIG);
  sigmodes[sig] &= ~SIG_TRAPPED;
}

static void
reset_or_restore_signal_handlers (reset)
     sh_resetsig_func_t *reset;
{
  register int i;

   
  if (sigmodes[EXIT_TRAP] & SIG_TRAPPED)
    {
      sigmodes[EXIT_TRAP] &= ~SIG_TRAPPED;	 
      if (reset != reset_signal)
	{
	  free_trap_command (EXIT_TRAP);
	  trap_list[EXIT_TRAP] = (char *)NULL;
	}
    }

  for (i = 1; i < NSIG; i++)
    {
      if (sigmodes[i] & SIG_TRAPPED)
	{
	  if (trap_list[i] == (char *)IGNORE_SIG)
	    set_signal_handler (i, SIG_IGN);
	  else
	    (*reset) (i);
	}
      else if (sigmodes[i] & SIG_SPECIAL)
	(*reset) (i);
      pending_traps[i] = 0;	 
    }

   
  if (function_trace_mode == 0)
    {
      sigmodes[DEBUG_TRAP] &= ~SIG_TRAPPED;
      sigmodes[RETURN_TRAP] &= ~SIG_TRAPPED;
    }
  if (error_trace_mode == 0)
    sigmodes[ERROR_TRAP] &= ~SIG_TRAPPED;
}

 
void
reset_signal_handlers ()
{
  reset_or_restore_signal_handlers (reset_signal);
}

 
void
restore_original_signals ()
{
  reset_or_restore_signal_handlers (restore_signal);
}

 
static void
reinit_trap (sig)
     int sig;
{
  sigmodes[sig] |= SIG_TRAPPED;
  if (trap_list[sig] == (char *)IGNORE_SIG)
    sigmodes[sig] |= SIG_IGNORED;
  else
    sigmodes[sig] &= ~SIG_IGNORED;
  if (sigmodes[sig] & SIG_INPROGRESS)
    sigmodes[sig] |= SIG_CHANGED;
}

 
void
restore_traps ()
{
  char *trapstr;
  int i;

   
  trapstr = trap_list[EXIT_TRAP];
  if (trapstr)
    reinit_trap (EXIT_TRAP);

   
  trapstr = trap_list[DEBUG_TRAP];
  if (trapstr && function_trace_mode == 0)
    reinit_trap (DEBUG_TRAP);
  trapstr = trap_list[RETURN_TRAP];
  if (trapstr && function_trace_mode == 0)
    reinit_trap (RETURN_TRAP);
  trapstr = trap_list[ERROR_TRAP];
  if (trapstr && error_trace_mode == 0)
    reinit_trap (ERROR_TRAP);

   
  for (i = 1; i < NSIG; i++)
    {
      trapstr = trap_list[i];
      if (sigmodes[i] & SIG_SPECIAL)
	{
	  if (trapstr && trapstr != (char *)DEFAULT_SIG)
	    reinit_trap (i);
	  if (trapstr == (char *)IGNORE_SIG && (sigmodes[i] & SIG_NO_TRAP) == 0)
	    set_signal_handler (i, SIG_IGN);
	}
      else if (trapstr == (char *)IGNORE_SIG)
	{
	  reinit_trap (i);
	  if ((sigmodes[i] & SIG_NO_TRAP) == 0)
	    set_signal_handler (i, SIG_IGN);
	}
      else if (trapstr != (char *)DEFAULT_SIG)
         
	set_signal (i, trapstr);

      pending_traps[i] = 0;	 
    }
}

 
int
maybe_call_trap_handler (sig)
     int sig;
{
   
  if ((sigmodes[sig] & SIG_TRAPPED) && ((sigmodes[sig] & SIG_IGNORED) == 0))
    {
      switch (sig)
	{
	case SIGINT:
	  run_interrupt_trap (0);
	  break;
	case EXIT_TRAP:
	  run_exit_trap ();
	  break;
	case DEBUG_TRAP:
	  run_debug_trap ();
	  break;
	case ERROR_TRAP:
	  run_error_trap ();
	  break;
	default:
	  trap_handler (sig);
	  break;
	}
      return (1);
    }
  else
    return (0);
}

int
signal_is_trapped (sig)
     int sig;
{
  return (sigmodes[sig] & SIG_TRAPPED);
}

int
signal_is_pending (sig)
     int sig;
{
  return (pending_traps[sig]);
}

int
signal_is_special (sig)
     int sig;
{
  return (sigmodes[sig] & SIG_SPECIAL);
}

int
signal_is_ignored (sig)
     int sig;
{
  return (sigmodes[sig] & SIG_IGNORED);
}

int
signal_is_hard_ignored (sig)
     int sig;
{
  return (sigmodes[sig] & SIG_HARD_IGNORE);
}

void
set_signal_hard_ignored (sig)
     int sig;
{
  sigmodes[sig] |= SIG_HARD_IGNORE;
  original_signals[sig] = SIG_IGN;
}

void
set_signal_ignored (sig)
     int sig;
{
  original_signals[sig] = SIG_IGN;
}

int
signal_in_progress (sig)
     int sig;
{
  return (sigmodes[sig] & SIG_INPROGRESS);
}

#if 0  
int
block_trapped_signals (maskp, omaskp)
     sigset_t *maskp;
     sigset_t *omaskp;
{
  int i;

  sigemptyset (maskp);
  for (i = 1; i < NSIG; i++)
    if (sigmodes[i] & SIG_TRAPPED)
      sigaddset (maskp, i);
  return (sigprocmask (SIG_BLOCK, maskp, omaskp));
}

int
unblock_trapped_signals (maskp)
     sigset_t *maskp;
{
  return (sigprocmask (SIG_SETMASK, maskp, 0));
}
#endif
