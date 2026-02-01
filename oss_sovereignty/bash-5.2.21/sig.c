 

 

#include "config.h"

#include "bashtypes.h"

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include <stdio.h>
#include <signal.h>

#include "bashintl.h"

#include "shell.h"
#include "execute_cmd.h"
#if defined (JOB_CONTROL)
#include "jobs.h"
#endif  
#include "siglist.h"
#include "sig.h"
#include "trap.h"

#include "builtins/common.h"
#include "builtins/builtext.h"

#if defined (READLINE)
#  include "bashline.h"
#  include <readline/readline.h>
#endif

#if defined (HISTORY)
#  include "bashhist.h"
#endif

extern void initialize_siglist PARAMS((void));
extern void set_original_signal PARAMS((int, SigHandler *));

#if !defined (JOB_CONTROL)
extern void initialize_job_signals PARAMS((void));
#endif

 
volatile sig_atomic_t interrupt_state = 0;

 
volatile sig_atomic_t sigwinch_received = 0;

 
volatile sig_atomic_t sigterm_received = 0;

 
volatile sig_atomic_t terminating_signal = 0;

 
procenv_t top_level;

#if defined (JOB_CONTROL) || defined (HAVE_POSIX_SIGNALS)
 
sigset_t top_level_mask;
#endif  

 
int interrupt_immediately = 0;

 
int terminate_immediately = 0;

#if defined (SIGWINCH)
static SigHandler *old_winch = (SigHandler *)SIG_DFL;
#endif

static void initialize_shell_signals PARAMS((void));
static void kill_shell PARAMS((int));

void
initialize_signals (reinit)
     int reinit;
{
  initialize_shell_signals ();
  initialize_job_signals ();
#if !defined (HAVE_SYS_SIGLIST) && !defined (HAVE_UNDER_SYS_SIGLIST) && !defined (HAVE_STRSIGNAL)
  if (reinit == 0)
    initialize_siglist ();
#endif  
}

 
struct termsig {
     int signum;
     SigHandler *orig_handler;
     int orig_flags;
     int core_dump;
};

#define NULL_HANDLER (SigHandler *)SIG_DFL

 
static struct termsig terminating_signals[] = {
#ifdef SIGHUP
{  SIGHUP, NULL_HANDLER, 0 },
#endif

#ifdef SIGINT
{  SIGINT, NULL_HANDLER, 0 },
#endif

#ifdef SIGILL
{  SIGILL, NULL_HANDLER, 0, 1},
#endif

#ifdef SIGTRAP
{  SIGTRAP, NULL_HANDLER, 0, 1 },
#endif

#ifdef SIGIOT
{  SIGIOT, NULL_HANDLER, 0, 1 },
#endif

#ifdef SIGDANGER
{  SIGDANGER, NULL_HANDLER, 0 },
#endif

#ifdef SIGEMT
{  SIGEMT, NULL_HANDLER, 0 },
#endif

#ifdef SIGFPE
{  SIGFPE, NULL_HANDLER, 0, 1 },
#endif

#ifdef SIGBUS
{  SIGBUS, NULL_HANDLER, 0, 1 },
#endif

#ifdef SIGSEGV
{  SIGSEGV, NULL_HANDLER, 0, 1 },
#endif

#ifdef SIGSYS
{  SIGSYS, NULL_HANDLER, 0, 1 },
#endif

#ifdef SIGPIPE
{  SIGPIPE, NULL_HANDLER, 0 },
#endif

#ifdef SIGALRM
{  SIGALRM, NULL_HANDLER, 0 },
#endif

#ifdef SIGTERM
{  SIGTERM, NULL_HANDLER, 0 },
#endif

 
#ifdef SIGXCPU
{  SIGXCPU, NULL_HANDLER, 0, 1 },
#endif

#ifdef SIGXFSZ
{  SIGXFSZ, NULL_HANDLER, 0, 1 },
#endif

#ifdef SIGVTALRM
{  SIGVTALRM, NULL_HANDLER, 0 },
#endif

#if 0
#ifdef SIGPROF
{  SIGPROF, NULL_HANDLER, 0 },
#endif
#endif

#ifdef SIGLOST
{  SIGLOST, NULL_HANDLER, 0 },
#endif

#ifdef SIGUSR1
{  SIGUSR1, NULL_HANDLER, 0 },
#endif

#ifdef SIGUSR2
{  SIGUSR2, NULL_HANDLER, 0 },
#endif
};

#define TERMSIGS_LENGTH (sizeof (terminating_signals) / sizeof (struct termsig))

#define XSIG(x) (terminating_signals[x].signum)
#define XHANDLER(x) (terminating_signals[x].orig_handler)
#define XSAFLAGS(x) (terminating_signals[x].orig_flags)
#define XCOREDUMP(x) (terminating_signals[x].core_dump)

static int termsigs_initialized = 0;

 
void
initialize_terminating_signals ()
{
  register int i;
#if defined (HAVE_POSIX_SIGNALS)
  struct sigaction act, oact;
#endif

  if (termsigs_initialized)
    return;

   
#if defined (HAVE_POSIX_SIGNALS)
  act.sa_handler = termsig_sighandler;
  act.sa_flags = 0;
  sigemptyset (&act.sa_mask);
  sigemptyset (&oact.sa_mask);
  for (i = 0; i < TERMSIGS_LENGTH; i++)
    sigaddset (&act.sa_mask, XSIG (i));
  for (i = 0; i < TERMSIGS_LENGTH; i++)
    {
       
      if (signal_is_trapped (XSIG (i)))
	continue;

      sigaction (XSIG (i), &act, &oact);
      XHANDLER(i) = oact.sa_handler;
      XSAFLAGS(i) = oact.sa_flags;

#if 0
      set_original_signal (XSIG(i), XHANDLER(i));	 
#else
      set_original_signal (XSIG(i), act.sa_handler);	 
#endif

       
       
      if (interactive_shell == 0 && XHANDLER (i) == SIG_IGN)
	{
	  sigaction (XSIG (i), &oact, &act);
	  set_signal_hard_ignored (XSIG (i));
	}
#if defined (SIGPROF) && !defined (_MINIX)
      if (XSIG (i) == SIGPROF && XHANDLER (i) != SIG_DFL && XHANDLER (i) != SIG_IGN)
	sigaction (XSIG (i), &oact, (struct sigaction *)NULL);
#endif  
    }
#else  

  for (i = 0; i < TERMSIGS_LENGTH; i++)
    {
       
      if (signal_is_trapped (XSIG (i)))
	continue;

      XHANDLER(i) = signal (XSIG (i), termsig_sighandler);
      XSAFLAGS(i) = 0;
       
       
      if (interactive_shell == 0 && XHANDLER (i) == SIG_IGN)
	{
	  signal (XSIG (i), SIG_IGN);
	  set_signal_hard_ignored (XSIG (i));
	}
#ifdef SIGPROF
      if (XSIG (i) == SIGPROF && XHANDLER (i) != SIG_DFL && XHANDLER (i) != SIG_IGN)
	signal (XSIG (i), XHANDLER (i));
#endif
    }

#endif  

  termsigs_initialized = 1;
}

static void
initialize_shell_signals ()
{
  if (interactive)
    initialize_terminating_signals ();

#if defined (JOB_CONTROL) || defined (HAVE_POSIX_SIGNALS)
   
  sigemptyset (&top_level_mask);
  sigprocmask (SIG_BLOCK, (sigset_t *)NULL, &top_level_mask);
#  if defined (SIGCHLD)
  if (sigismember (&top_level_mask, SIGCHLD))
    {
      sigdelset (&top_level_mask, SIGCHLD);
      sigprocmask (SIG_SETMASK, &top_level_mask, (sigset_t *)NULL);
    }
#  endif
#endif  

   
  set_signal_handler (SIGQUIT, SIG_IGN);

  if (interactive)
    {
      set_signal_handler (SIGINT, sigint_sighandler);
      get_original_signal (SIGTERM);
      set_signal_handler (SIGTERM, SIG_IGN);
      set_sigwinch_handler ();
    }
}

void
reset_terminating_signals ()
{
  register int i;
#if defined (HAVE_POSIX_SIGNALS)
  struct sigaction act;
#endif

  if (termsigs_initialized == 0)
    return;

#if defined (HAVE_POSIX_SIGNALS)
  act.sa_flags = 0;
  sigemptyset (&act.sa_mask);
  for (i = 0; i < TERMSIGS_LENGTH; i++)
    {
       
      if (signal_is_trapped (XSIG (i)) || signal_is_special (XSIG (i)))
	continue;

      act.sa_handler = XHANDLER (i);
      act.sa_flags = XSAFLAGS (i);
      sigaction (XSIG (i), &act, (struct sigaction *) NULL);
    }
#else  
  for (i = 0; i < TERMSIGS_LENGTH; i++)
    {
      if (signal_is_trapped (XSIG (i)) || signal_is_special (XSIG (i)))
	continue;

      signal (XSIG (i), XHANDLER (i));
    }
#endif  

  termsigs_initialized = 0;
}
#undef XHANDLER

 
void
top_level_cleanup ()
{
   
  while (parse_and_execute_level)
    parse_and_execute_cleanup (-1);

#if defined (PROCESS_SUBSTITUTION)
  unlink_fifo_list ();
#endif  

  run_unwind_protects ();
  loop_level = continuing = breaking = funcnest = 0;
  executing_list = comsub_ignore_return = return_catch_flag = wait_intr_flag = 0;
}

 
void
throw_to_top_level ()
{
  int print_newline = 0;

  if (interrupt_state)
    {
      if (last_command_exit_value < 128)
	last_command_exit_value = 128 + SIGINT;
      set_pipestatus_from_exit (last_command_exit_value);
      print_newline = 1;
      DELINTERRUPT;
    }

  if (interrupt_state)
    return;

  last_command_exit_signal = (last_command_exit_value > 128) ?
				(last_command_exit_value - 128) : 0;
  last_command_exit_value |= 128;
  set_pipestatus_from_exit (last_command_exit_value);

   
  if (signal_is_trapped (SIGINT) && signal_is_pending (SIGINT))
    run_interrupt_trap (1);

   
  while (parse_and_execute_level)
    parse_and_execute_cleanup (-1);

  if (running_trap > 0)
    {
      run_trap_cleanup (running_trap - 1);
      running_trap = 0;
    }

#if defined (JOB_CONTROL)
  give_terminal_to (shell_pgrp, 0);
#endif  

   
  restore_sigmask ();  

  reset_parser ();

#if defined (READLINE)
  if (interactive)
    bashline_reset ();
#endif  

#if defined (PROCESS_SUBSTITUTION)
  unlink_fifo_list ();
#endif  

  run_unwind_protects ();
  loop_level = continuing = breaking = funcnest = 0;
  executing_list = comsub_ignore_return = return_catch_flag = wait_intr_flag = 0;

  if (interactive && print_newline)
    {
      fflush (stdout);
      fprintf (stderr, "\n");
      fflush (stderr);
    }

   
  if (interactive || (interactive_shell && !shell_initialized) ||
      (print_newline && signal_is_trapped (SIGINT)))
    jump_to_top_level (DISCARD);
  else
    jump_to_top_level (EXITPROG);
}

 
void
jump_to_top_level (value)
     int value;
{
  sh_longjmp (top_level, value);
}

void
restore_sigmask ()
{
#if defined (JOB_CONTROL) || defined (HAVE_POSIX_SIGNALS)
  sigprocmask (SIG_SETMASK, &top_level_mask, (sigset_t *)NULL);
#endif
}

static int handling_termsig = 0;

sighandler
termsig_sighandler (sig)
     int sig;
{
   
  if (
#ifdef SIGHUP
    sig != SIGHUP &&
#endif
#ifdef SIGINT
    sig != SIGINT &&
#endif
#ifdef SIGDANGER
    sig != SIGDANGER &&
#endif
#ifdef SIGPIPE
    sig != SIGPIPE &&
#endif
#ifdef SIGALRM
    sig != SIGALRM &&
#endif
#ifdef SIGTERM
    sig != SIGTERM &&
#endif
#ifdef SIGXCPU
    sig != SIGXCPU &&
#endif
#ifdef SIGXFSZ
    sig != SIGXFSZ &&
#endif
#ifdef SIGVTALRM
    sig != SIGVTALRM &&
#endif
#ifdef SIGLOST
    sig != SIGLOST &&
#endif
#ifdef SIGUSR1
    sig != SIGUSR1 &&
#endif
#ifdef SIGUSR2
   sig != SIGUSR2 &&
#endif
   sig == terminating_signal)
    terminate_immediately = 1;

   
  if (handling_termsig)
    kill_shell (sig);		 

  terminating_signal = sig;

  if (terminate_immediately)
    {
#if defined (HISTORY)
       
#  if defined (READLINE)
      if (interactive_shell == 0 || interactive == 0 || (sig != SIGHUP && sig != SIGTERM) || no_line_editing || (RL_ISSTATE (RL_STATE_READCMD) == 0))
#  endif
        history_lines_this_session = 0;
#endif
      terminate_immediately = 0;
      termsig_handler (sig);
    }

#if defined (READLINE)
   
  if (RL_ISSTATE (RL_STATE_SIGHANDLER) || RL_ISSTATE (RL_STATE_TERMPREPPED))
    bashline_set_event_hook ();
#endif

  SIGRETURN (0);
}

void
termsig_handler (sig)
     int sig;
{
   
  if (handling_termsig)
    return;

  handling_termsig = terminating_signal;	 
  terminating_signal = 0;	 

   
  if (sig == SIGINT && signal_is_trapped (SIGINT))
    run_interrupt_trap (0);

#if defined (HISTORY)
   
  if (interactive_shell && interactive && (sig == SIGHUP || sig == SIGTERM) && remember_on_history)
    maybe_save_shell_history ();
#endif  

  if (this_shell_builtin == read_builtin)
    read_tty_cleanup ();

#if defined (JOB_CONTROL)
  if (sig == SIGHUP && (interactive || (subshell_environment & (SUBSHELL_COMSUB|SUBSHELL_PROCSUB))))
    hangup_all_jobs ();

  if ((subshell_environment & (SUBSHELL_COMSUB|SUBSHELL_PROCSUB)) == 0)
    end_job_control ();
#endif  

#if defined (PROCESS_SUBSTITUTION)
  unlink_all_fifos ();
#  if defined (JOB_CONTROL)
  procsub_clear ();
#  endif
#endif  

   
  loop_level = continuing = breaking = funcnest = 0;
  executing_list = comsub_ignore_return = return_catch_flag = wait_intr_flag = 0;

  run_exit_trap ();	 

  kill_shell (sig);
}

static void
kill_shell (sig)
     int sig;
{
  int i, core;
  sigset_t mask;

   
  restore_sigmask ();

  set_signal_handler (sig, SIG_DFL);

  kill (getpid (), sig);

  if (dollar_dollar_pid != 1)
    exit (128+sig);		 

   

   
  sigprocmask (SIG_SETMASK, (sigset_t *)NULL, &mask);
  for (i = core = 0; i < TERMSIGS_LENGTH; i++)
    {
      set_signal_handler (XSIG (i), SIG_DFL);
      sigdelset (&mask, XSIG (i));
      if (sig == XSIG (i))
	core = XCOREDUMP (i);
    }
  sigprocmask (SIG_SETMASK, &mask, (sigset_t *)NULL);

  if (core)
    *((volatile unsigned long *) NULL) = 0xdead0000 + sig;	 

  exit (128+sig);
}
#undef XSIG

 
sighandler
sigint_sighandler (sig)
     int sig;
{
#if defined (MUST_REINSTALL_SIGHANDLERS)
  signal (sig, sigint_sighandler);
#endif

   
  if (interrupt_state == 0)
    ADDINTERRUPT;

   
  if (wait_intr_flag)
    {
      last_command_exit_value = 128 + sig;
      set_pipestatus_from_exit (last_command_exit_value);
      wait_signal_received = sig;
      SIGRETURN (0);
    }

   
  if (signal_is_trapped (sig))
    set_trap_state (sig);

   
  if (interrupt_immediately)
    {
      interrupt_immediately = 0;
      set_exit_status (128 + sig);
      throw_to_top_level ();
    }
#if defined (READLINE)
   
  else if (RL_ISSTATE (RL_STATE_SIGHANDLER))
    bashline_set_event_hook ();
#endif

  SIGRETURN (0);
}

#if defined (SIGWINCH)
sighandler
sigwinch_sighandler (sig)
     int sig;
{
#if defined (MUST_REINSTALL_SIGHANDLERS)
  set_signal_handler (SIGWINCH, sigwinch_sighandler);
#endif  
  sigwinch_received = 1;
  SIGRETURN (0);
}
#endif  

void
set_sigwinch_handler ()
{
#if defined (SIGWINCH)
 old_winch = set_signal_handler (SIGWINCH, sigwinch_sighandler);
#endif
}

void
unset_sigwinch_handler ()
{
#if defined (SIGWINCH)
  set_signal_handler (SIGWINCH, old_winch);
#endif
}

sighandler
sigterm_sighandler (sig)
     int sig;
{
  sigterm_received = 1;		 
  SIGRETURN (0);
}

 
#if !defined (HAVE_POSIX_SIGNALS)

 
sigprocmask (operation, newset, oldset)
     int operation, *newset, *oldset;
{
  int old, new;

  if (newset)
    new = *newset;
  else
    new = 0;

  switch (operation)
    {
    case SIG_BLOCK:
      old = sigblock (new);
      break;

    case SIG_SETMASK:
      old = sigsetmask (new);
      break;

    default:
      internal_error (_("sigprocmask: %d: invalid operation"), operation);
    }

  if (oldset)
    *oldset = old;
}

#else

#if !defined (SA_INTERRUPT)
#  define SA_INTERRUPT 0
#endif

#if !defined (SA_RESTART)
#  define SA_RESTART 0
#endif

SigHandler *
set_signal_handler (sig, handler)
     int sig;
     SigHandler *handler;
{
  struct sigaction act, oact;

  act.sa_handler = handler;
  act.sa_flags = 0;

   
   
#if defined (SIGCHLD)
  if (sig == SIGCHLD)
    act.sa_flags |= SA_RESTART;		 
#endif
   
#if defined (SIGWINCH)
  if (sig == SIGWINCH)
    act.sa_flags |= SA_RESTART;		 
#endif
   
  if (sig == SIGTERM && handler == sigterm_sighandler)
    act.sa_flags |= SA_RESTART;		 

  sigemptyset (&act.sa_mask);
  sigemptyset (&oact.sa_mask);
  if (sigaction (sig, &act, &oact) == 0)
    return (oact.sa_handler);
  else
    return (SIG_DFL);
}
#endif  
