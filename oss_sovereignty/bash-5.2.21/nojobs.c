 

 

 

#include "config.h"

#include "bashtypes.h"
#include "filecntl.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <errno.h>

#if defined (BUFFERED_INPUT)
#  include "input.h"
#endif

 
#include "shtty.h"

#include "bashintl.h"

#include "shell.h"
#include "jobs.h"
#include "execute_cmd.h"
#include "trap.h"

#include "builtins/builtext.h"	 
#include "builtins/common.h"

#define DEFAULT_CHILD_MAX 4096

#if defined (_POSIX_VERSION) || !defined (HAVE_KILLPG)
#  define killpg(pg, sig)		kill(-(pg),(sig))
#endif  

#if !defined (HAVE_SIGINTERRUPT) && !defined (HAVE_POSIX_SIGNALS)
#  define siginterrupt(sig, code)
#endif  

#if defined (HAVE_WAITPID)
#  define WAITPID(pid, statusp, options) waitpid (pid, statusp, options)
#else
#  define WAITPID(pid, statusp, options) wait (statusp)
#endif  

 
#define input_tty() (shell_tty != -1) ? shell_tty : fileno (stderr)

#if !defined (errno)
extern int errno;
#endif  

extern void set_original_signal PARAMS((int, SigHandler *));

volatile pid_t last_made_pid = NO_PID;
volatile pid_t last_asynchronous_pid = NO_PID;

static int queue_sigchld;		 
int waiting_for_child;

 
int already_making_children = 0;

 
int shell_tty = -1;

 
int check_window_size = CHECKWINSIZE_DEFAULT;

 
int job_control = 0;

int running_in_background = 0;	 

 
struct proc_status {
  pid_t pid;
  int status;	 
  int flags;
};

 
#define PROC_RUNNING	0x01
#define PROC_NOTIFIED	0x02
#define PROC_ASYNC	0x04
#define PROC_SIGNALED	0x10

 
#define PROC_BAD	 -1
#define PROC_STILL_ALIVE -2

static struct proc_status *pid_list = (struct proc_status *)NULL;
static int pid_list_size;
static int wait_sigint_received;

static long child_max = -1L;

static void alloc_pid_list PARAMS((void));
static int find_proc_slot PARAMS((pid_t));
static int find_index_by_pid PARAMS((pid_t));
static int find_status_by_pid PARAMS((pid_t));
static int process_exit_status PARAMS((WAIT));
static int find_termsig_by_pid PARAMS((pid_t));
static int get_termsig PARAMS((WAIT));
static void set_pid_status PARAMS((pid_t, WAIT));
static void set_pid_flags PARAMS((pid_t, int));
static void unset_pid_flags PARAMS((pid_t, int));
static int get_pid_flags PARAMS((pid_t));
static void add_pid PARAMS((pid_t, int));
static void mark_dead_jobs_as_notified PARAMS((int));

static sighandler wait_sigint_handler PARAMS((int));
static char *j_strsignal PARAMS((int));

#if defined (HAVE_WAITPID)
static void reap_zombie_children PARAMS((void));
#endif

#if !defined (HAVE_SIGINTERRUPT) && defined (HAVE_POSIX_SIGNALS)
static int siginterrupt PARAMS((int, int));
#endif

static void restore_sigint_handler PARAMS((void));

 
static void
alloc_pid_list ()
{
  register int i;
  int old = pid_list_size;

  pid_list_size += 10;
  pid_list = (struct proc_status *)xrealloc (pid_list, pid_list_size * sizeof (struct proc_status));

   
  for (i = old; i < pid_list_size; i++)
    {
      pid_list[i].pid = NO_PID;
      pid_list[i].status = pid_list[i].flags = 0;
    }
}

 
static int
find_proc_slot (pid)
     pid_t pid;
{
  register int i;

  for (i = 0; i < pid_list_size; i++)
    if (pid_list[i].pid == NO_PID || pid_list[i].pid == pid)
      return (i);

  if (i == pid_list_size)
    alloc_pid_list ();

  return (i);
}

 
static int
find_index_by_pid (pid)
     pid_t pid;
{
  register int i;

  for (i = 0; i < pid_list_size; i++)
    if (pid_list[i].pid == pid)
      return (i);

  return (NO_PID);
}

 
static int
find_status_by_pid (pid)
     pid_t pid;
{
  int i;

  i = find_index_by_pid (pid);
  if (i == NO_PID)
    return (PROC_BAD);
  if (pid_list[i].flags & PROC_RUNNING)
    return (PROC_STILL_ALIVE);
  return (pid_list[i].status);
}

static int
process_exit_status (status)
     WAIT status;
{
  if (WIFSIGNALED (status))
    return (128 + WTERMSIG (status));
  else
    return (WEXITSTATUS (status));
}

 
static int
find_termsig_by_pid (pid)
     pid_t pid;
{
  int i;

  i = find_index_by_pid (pid);
  if (i == NO_PID)
    return (0);
  if (pid_list[i].flags & PROC_RUNNING)
    return (0);
  return (get_termsig ((WAIT)pid_list[i].status));
}

 
static int
get_termsig (status)
     WAIT status;
{
  if (WIFSTOPPED (status) == 0 && WIFSIGNALED (status))
    return (WTERMSIG (status));
  else
    return (0);
}

 
static void
set_pid_status (pid, status)
     pid_t pid;
     WAIT status;
{
  int slot;

#if defined (COPROCESS_SUPPORT)
  coproc_pidchk (pid, status);
#endif

#if defined (PROCESS_SUBSTITUTION)
  if ((slot = find_procsub_child (pid)) >= 0)
    set_procsub_status (slot, pid, WSTATUS (status));
     
#endif

  slot = find_index_by_pid (pid);
  if (slot == NO_PID)
    return;

  pid_list[slot].status = process_exit_status (status);
  pid_list[slot].flags &= ~PROC_RUNNING;
  if (WIFSIGNALED (status))
    pid_list[slot].flags |= PROC_SIGNALED;
   
  if ((pid_list[slot].flags & PROC_ASYNC) == 0)
    pid_list[slot].flags |= PROC_NOTIFIED;
}

 
static void
set_pid_flags (pid, flags)
     pid_t pid;
     int flags;
{
  int slot;

  slot = find_index_by_pid (pid);
  if (slot == NO_PID)
    return;

  pid_list[slot].flags |= flags;
}

 
static void
unset_pid_flags (pid, flags)
     pid_t pid;
     int flags;
{
  int slot;

  slot = find_index_by_pid (pid);
  if (slot == NO_PID)
    return;

  pid_list[slot].flags &= ~flags;
}

 
static int
get_pid_flags (pid)
     pid_t pid;
{
  int slot;

  slot = find_index_by_pid (pid);
  if (slot == NO_PID)
    return 0;

  return (pid_list[slot].flags);
}

static void
add_pid (pid, async)
     pid_t pid;
     int async;
{
  int slot;

  slot = find_proc_slot (pid);

  pid_list[slot].pid = pid;
  pid_list[slot].status = -1;
  pid_list[slot].flags = PROC_RUNNING;
  if (async)
    pid_list[slot].flags |= PROC_ASYNC;
}

static void
mark_dead_jobs_as_notified (force)
     int force;
{
  register int i, ndead;

   
  for (i = ndead = 0; force == 0 && i < pid_list_size; i++)
    {
      if (pid_list[i].pid == NO_PID)
	continue;
      if (((pid_list[i].flags & PROC_RUNNING) == 0) &&
	   (pid_list[i].flags & PROC_ASYNC))
	ndead++;
    }

  if (child_max < 0)
    child_max = getmaxchild ();
  if (child_max < 0)
    child_max = DEFAULT_CHILD_MAX;

  if (force == 0 && ndead <= child_max)
    return;

   
  for (i = 0; i < pid_list_size; i++)
    {
      if (pid_list[i].pid == NO_PID)
	continue;
      if (((pid_list[i].flags & PROC_RUNNING) == 0) &&
	   pid_list[i].pid != last_asynchronous_pid)
	{
	  pid_list[i].flags |= PROC_NOTIFIED;
	  if (force == 0 && (pid_list[i].flags & PROC_ASYNC) && --ndead <= child_max)
	    break;
	}
    }
}

 
int
cleanup_dead_jobs ()
{
  register int i;

#if defined (HAVE_WAITPID)
  reap_zombie_children ();
#endif

  for (i = 0; i < pid_list_size; i++)
    {
      if (pid_list[i].pid != NO_PID &&
	    (pid_list[i].flags & PROC_RUNNING) == 0 &&
	    (pid_list[i].flags & PROC_NOTIFIED))
	pid_list[i].pid = NO_PID;
    }

#if defined (COPROCESS_SUPPORT)
  coproc_reap ();
#endif

  return 0;
}

void
reap_dead_jobs ()
{
  mark_dead_jobs_as_notified (0);
  cleanup_dead_jobs ();
}

 
int
initialize_job_control (force)
     int force;
{
  shell_tty = fileno (stderr);

  if (interactive)
    get_tty_state ();
  return 0;
}

 
void
initialize_job_signals ()
{
  set_signal_handler (SIGINT, sigint_sighandler);

   
  if (login_shell)
    ignore_tty_job_signals ();
}

#if defined (HAVE_WAITPID)
 
static void
reap_zombie_children ()
{
#  if defined (WNOHANG)
  pid_t pid;
  WAIT status;

  CHECK_TERMSIG;
  CHECK_WAIT_INTR;
  while ((pid = waitpid (-1, (int *)&status, WNOHANG)) > 0)
    set_pid_status (pid, status);
#  endif  
  CHECK_TERMSIG;
  CHECK_WAIT_INTR;
}
#endif  

#if !defined (HAVE_SIGINTERRUPT) && defined (HAVE_POSIX_SIGNALS)

#if !defined (SA_RESTART)
#  define SA_RESTART 0
#endif

static int
siginterrupt (sig, flag)
     int sig, flag;
{
  struct sigaction act;

  sigaction (sig, (struct sigaction *)NULL, &act);

  if (flag)
    act.sa_flags &= ~SA_RESTART;
  else
    act.sa_flags |= SA_RESTART;

  return (sigaction (sig, &act, (struct sigaction *)NULL));
}
#endif  

 
pid_t
make_child (command, flags)
     char *command;
     int flags;
{
  pid_t pid;
  int async_p, forksleep;
  sigset_t set, oset;

   
  if (command)
    free (command);

  async_p = (flags & FORK_ASYNC);
  start_pipeline ();

#if defined (BUFFERED_INPUT)
   
  if (default_buffered_input != -1 && (!async_p || default_buffered_input > 0))
    sync_buffered_stream (default_buffered_input);
#endif  

   
  if (interactive_shell)
    {
      sigemptyset (&set);
      sigaddset (&set, SIGTERM);
      sigemptyset (&oset);
      sigprocmask (SIG_BLOCK, &set, &oset);
      set_signal_handler (SIGTERM, SIG_DFL);
    }

   
  forksleep = 1;
  while ((pid = fork ()) < 0 && errno == EAGAIN && forksleep < FORKSLEEP_MAX)
    {
      sys_error ("fork: retry");

#if defined (HAVE_WAITPID)
       
      reap_zombie_children ();
      if (forksleep > 1 && sleep (forksleep) != 0)
        break;
#else
      if (sleep (forksleep) != 0)
	break;
#endif  
      forksleep <<= 1;
    }

  if (pid != 0)
    if (interactive_shell)
      {
	set_signal_handler (SIGTERM, SIG_IGN);
	sigprocmask (SIG_SETMASK, &oset, (sigset_t *)NULL);
      }

  if (pid < 0)
    {
      sys_error ("fork");
      last_command_exit_value = EX_NOEXEC;
      throw_to_top_level ();
    }

  if (pid == 0)
    {
#if defined (BUFFERED_INPUT)
      unset_bash_input (0);
#endif  

      CLRINTERRUPT;	 

       
      restore_sigmask ();

#if 0
       
      if (async_p)
	last_asynchronous_pid = getpid ();
#endif

      subshell_environment |= SUBSHELL_IGNTRAP;

      default_tty_job_signals ();
    }
  else
    {
       

      last_made_pid = pid;

      if (async_p)
	last_asynchronous_pid = pid;

      add_pid (pid, async_p);
    }
  return (pid);
}

void
ignore_tty_job_signals ()
{
#if defined (SIGTSTP)
  set_signal_handler (SIGTSTP, SIG_IGN);
  set_signal_handler (SIGTTIN, SIG_IGN);
  set_signal_handler (SIGTTOU, SIG_IGN);
#endif
}

void
default_tty_job_signals ()
{
#if defined (SIGTSTP)
  if (signal_is_trapped (SIGTSTP) == 0 && signal_is_hard_ignored (SIGTSTP))
    set_signal_handler (SIGTSTP, SIG_IGN);
  else
    set_signal_handler (SIGTSTP, SIG_DFL);
  if (signal_is_trapped (SIGTTIN) == 0 && signal_is_hard_ignored (SIGTTIN))
    set_signal_handler (SIGTTIN, SIG_IGN);
  else
    set_signal_handler (SIGTTIN, SIG_DFL);
  if (signal_is_trapped (SIGTTOU) == 0 && signal_is_hard_ignored (SIGTTOU))
    set_signal_handler (SIGTTOU, SIG_IGN);
  else
    set_signal_handler (SIGTTOU, SIG_DFL);
#endif
}

 
void
get_original_tty_job_signals ()
{
  static int fetched = 0;

  if (fetched == 0)
    {
#if defined (SIGTSTP)
      if (interactive_shell)
	{
	  set_original_signal (SIGTSTP, SIG_DFL);
	  set_original_signal (SIGTTIN, SIG_DFL);
	  set_original_signal (SIGTTOU, SIG_DFL);
	}
      else
	{
	  get_original_signal (SIGTSTP);
	  get_original_signal (SIGTTIN);
	  get_original_signal (SIGTTOU);
	}
#endif
      fetched = 1;
    }
}

 
int
wait_for_single_pid (pid, flags)
     pid_t pid;
     int flags;
{
  pid_t got_pid;
  WAIT status;
  int pstatus;

  pstatus = find_status_by_pid (pid);

  if (pstatus == PROC_BAD)
    {
      internal_error (_("wait: pid %ld is not a child of this shell"), (long)pid);
      return (257);
    }

  if (pstatus != PROC_STILL_ALIVE)
    {
      if (pstatus > 128)
	last_command_exit_signal = find_termsig_by_pid (pid);
      return (pstatus);
    }

  siginterrupt (SIGINT, 1);
  while ((got_pid = WAITPID (pid, &status, 0)) != pid)
    {
      CHECK_TERMSIG;
      CHECK_WAIT_INTR;
      if (got_pid < 0)
	{
	  if (errno != EINTR && errno != ECHILD)
	    {
	      siginterrupt (SIGINT, 0);
	      sys_error ("wait");
	    }
	  break;
	}
      else if (got_pid > 0)
	set_pid_status (got_pid, status);
    }

  if (got_pid > 0)
    {
      set_pid_status (got_pid, status);
      set_pid_flags (got_pid, PROC_NOTIFIED);
    }

  siginterrupt (SIGINT, 0);
  QUIT;
  CHECK_WAIT_INTR;

  return (got_pid > 0 ? process_exit_status (status) : -1);
}

 
int
wait_for_background_pids (ps)
     struct procstat *ps;
{
  pid_t got_pid;
  WAIT status;
  int njobs;

   

  njobs = 0;
  siginterrupt (SIGINT, 1);

   
  waiting_for_child = 1;
  while ((got_pid = WAITPID (-1, &status, 0)) != -1)
    {
      waiting_for_child = 0;
      njobs++;
      set_pid_status (got_pid, status);
      if (ps)
	{
	  ps->pid = got_pid;
	  ps->status = process_exit_status (status);
	}
      waiting_for_child = 1;
      CHECK_WAIT_INTR;
    }
  waiting_for_child = 0;

  if (errno != EINTR && errno != ECHILD)
    {
      siginterrupt (SIGINT, 0);
      sys_error("wait");
    }

  siginterrupt (SIGINT, 0);
  QUIT;
  CHECK_WAIT_INTR;

  mark_dead_jobs_as_notified (1);
  cleanup_dead_jobs ();

  return njobs;
}

void
wait_sigint_cleanup ()
{
}

 
#define INVALID_SIGNAL_HANDLER (SigHandler *)wait_for_background_pids
static SigHandler *old_sigint_handler = INVALID_SIGNAL_HANDLER;

static void
restore_sigint_handler ()
{
  if (old_sigint_handler != INVALID_SIGNAL_HANDLER)
    {
      set_signal_handler (SIGINT, old_sigint_handler);
      old_sigint_handler = INVALID_SIGNAL_HANDLER;
    }
}

 
static sighandler
wait_sigint_handler (sig)
     int sig;
{
  SigHandler *sigint_handler;

   
  if (this_shell_builtin && this_shell_builtin == wait_builtin &&
      signal_is_trapped (SIGINT) &&
      ((sigint_handler = trap_to_sighandler (SIGINT)) == trap_handler))
    {
      last_command_exit_value = 128+SIGINT;
      restore_sigint_handler ();
      trap_handler (SIGINT);	 
      wait_signal_received = SIGINT;
      SIGRETURN (0);
    }

  wait_sigint_received = 1;

  SIGRETURN (0);
}

static char *
j_strsignal (s)
     int s;
{
  static char retcode_name_buffer[64] = { '\0' };
  char *x;

  x = strsignal (s);
  if (x == 0)
    {
      x = retcode_name_buffer;
      sprintf (x, "Signal %d", s);
    }
  return x;
}

 
int
wait_for (pid, flags)
     pid_t pid;
     int flags;
{
  int return_val, pstatus;
  pid_t got_pid;
  WAIT status;

  pstatus = find_status_by_pid (pid);

  if (pstatus == PROC_BAD)
    return (0);

  if (pstatus != PROC_STILL_ALIVE)
    {
      if (pstatus > 128)
	last_command_exit_signal = find_termsig_by_pid (pid);
      return (pstatus);
    }

   
  wait_sigint_received = 0;
  if (interactive_shell == 0)
    old_sigint_handler = set_signal_handler (SIGINT, wait_sigint_handler);

  waiting_for_child = 1;  
  CHECK_WAIT_INTR;
  while ((got_pid = WAITPID (-1, &status, 0)) != pid)  
    {
      waiting_for_child = 0;
      CHECK_TERMSIG;
      CHECK_WAIT_INTR;
      if (got_pid < 0 && errno == ECHILD)
	{
#if !defined (_POSIX_VERSION)
	  status.w_termsig = status.w_retcode = 0;
#else
	  status = 0;
#endif  
	  break;
	}
      else if (got_pid < 0 && errno != EINTR)
	programming_error ("wait_for(%ld): %s", (long)pid, strerror(errno));
      else if (got_pid > 0)
	set_pid_status (got_pid, status);
      waiting_for_child = 1;
    }
  waiting_for_child = 0;

  if (got_pid > 0)
    set_pid_status (got_pid, status);

#if defined (HAVE_WAITPID)
  if (got_pid >= 0)
    reap_zombie_children ();
#endif  

  CHECK_TERMSIG;
  CHECK_WAIT_INTR;

  if (interactive_shell == 0)
    {
      SigHandler *temp_handler;

      temp_handler = old_sigint_handler;
      restore_sigint_handler ();

       
      if (WIFSIGNALED (status) && (WTERMSIG (status) == SIGINT))
	{

	  if (maybe_call_trap_handler (SIGINT) == 0)
	    {
	      if (temp_handler == SIG_DFL)
		termsig_handler (SIGINT);
	      else if (temp_handler != INVALID_SIGNAL_HANDLER && temp_handler != SIG_IGN)
		(*temp_handler) (SIGINT);
	    }
	}
    }

   
   
  return_val = process_exit_status (status);
  last_command_exit_signal = get_termsig (status);

#if defined (DONT_REPORT_SIGPIPE) && defined (DONT_REPORT_SIGTERM)
#  define REPORTSIG(x) ((x) != SIGINT && (x) != SIGPIPE && (x) != SIGTERM)
#elif !defined (DONT_REPORT_SIGPIPE) && !defined (DONT_REPORT_SIGTERM)
#  define REPORTSIG(x) ((x) != SIGINT)
#elif defined (DONT_REPORT_SIGPIPE)
#  define REPORTSIG(x) ((x) != SIGINT && (x) != SIGPIPE)
#else
#  define REPORTSIG(x) ((x) != SIGINT && (x) != SIGTERM)
#endif

  if ((WIFSTOPPED (status) == 0) && WIFSIGNALED (status) && REPORTSIG(WTERMSIG (status)))
    {
      fprintf (stderr, "%s", j_strsignal (WTERMSIG (status)));
      if (WIFCORED (status))
	fprintf (stderr, _(" (core dumped)"));
      fprintf (stderr, "\n");
    }

  if (interactive_shell && subshell_environment == 0)
    {
      if (WIFSIGNALED (status) || WIFSTOPPED (status))
	set_tty_state ();
      else
	get_tty_state ();
    }
  else if (interactive_shell == 0 && subshell_environment == 0 && check_window_size)
    get_new_window_size (0, (int *)0, (int *)0);

  return (return_val);
}

 
int
kill_pid (pid, signal, group)
     pid_t pid;
     int signal, group;
{
  int result;

  if (pid < -1)
    {
      pid = -pid;
      group = 1;
    }
  result = group ? killpg (pid, signal) : kill (pid, signal);
  return (result);
}

static TTYSTRUCT shell_tty_info;
static int got_tty_state;

 
int
get_tty_state ()
{
  int tty;

  tty = input_tty ();
  if (tty != -1)
    {
      ttgetattr (tty, &shell_tty_info);
      got_tty_state = 1;
      if (check_window_size)
	get_new_window_size (0, (int *)0, (int *)0);
    }
  return 0;
}

 
int
set_tty_state ()
{
  int tty;

  tty = input_tty ();
  if (tty != -1)
    {
      if (got_tty_state == 0)
	return 0;
      ttsetattr (tty, &shell_tty_info);
    }
  return 0;
}

 
int
give_terminal_to (pgrp, force)
     pid_t pgrp;
     int force;
{
  return 0;
}

 
int
stop_pipeline (async, ignore)
     int async;
     COMMAND *ignore;
{
  already_making_children = 0;
  return 0;
}

void
start_pipeline ()
{
  already_making_children = 1;
}

void
stop_making_children ()
{
  already_making_children = 0;
}

 
void
without_job_control ()
{
  stop_making_children ();
  last_made_pid = NO_PID;	 
}

int
get_job_by_pid (pid, block, ignore)
     pid_t pid;
     int block;
     PROCESS **ignore;
{
  int i;

  i = find_index_by_pid (pid);
  return ((i == NO_PID) ? PROC_BAD : i);
}

 
void
describe_pid (pid)
     pid_t pid;
{
  fprintf (stderr, "%ld\n", (long) pid);
}

int
freeze_jobs_list ()
{
  return 0;
}

void
unfreeze_jobs_list ()
{
}

void
set_jobs_list_frozen (s)
     int s;
{
}

int
count_all_jobs ()
{
  return 0;
}
