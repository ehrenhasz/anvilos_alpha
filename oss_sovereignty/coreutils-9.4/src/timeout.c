 

#include <config.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#if HAVE_PRCTL
# include <sys/prctl.h>
#endif
#include <sys/wait.h>

#include "system.h"
#include "cl-strtod.h"
#include "xstrtod.h"
#include "sig2str.h"
#include "operand2sig.h"
#include "quote.h"

#if HAVE_SETRLIMIT
 
# include <sys/resource.h>
#endif

 
#ifndef SA_RESTART
# define SA_RESTART 0
#endif

#define PROGRAM_NAME "timeout"

#define AUTHORS proper_name_lite ("Padraig Brady", "P\303\241draig Brady")

static int timed_out;
static int term_signal = SIGTERM;   
static pid_t monitored_pid;
static double kill_after;
static bool foreground;       
static bool preserve_status;  
static bool verbose;          
static char const *command;

 
enum
{
      FOREGROUND_OPTION = CHAR_MAX + 1,
      PRESERVE_STATUS_OPTION
};

static struct option const long_options[] =
{
  {"kill-after", required_argument, nullptr, 'k'},
  {"signal", required_argument, nullptr, 's'},
  {"verbose", no_argument, nullptr, 'v'},
  {"foreground", no_argument, nullptr, FOREGROUND_OPTION},
  {"preserve-status", no_argument, nullptr, PRESERVE_STATUS_OPTION},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

 
static void
settimeout (double duration, bool warn)
{

#if HAVE_TIMER_SETTIME
   

  struct timespec ts = dtotimespec (duration);
  struct itimerspec its = { {0, 0}, ts };
  timer_t timerid;
  if (timer_create (CLOCK_REALTIME, nullptr, &timerid) == 0)
    {
      if (timer_settime (timerid, 0, &its, nullptr) == 0)
        return;
      else
        {
          if (warn)
            error (0, errno, _("warning: timer_settime"));
          timer_delete (timerid);
        }
    }
  else if (warn && errno != ENOSYS)
    error (0, errno, _("warning: timer_create"));

#elif HAVE_SETITIMER
   

  struct timeval tv;
  struct timespec ts = dtotimespec (duration);
  tv.tv_sec = ts.tv_sec;
  tv.tv_usec = (ts.tv_nsec + 999) / 1000;
  if (tv.tv_usec == 1000 * 1000)
    {
      if (tv.tv_sec != TYPE_MAXIMUM (time_t))
        {
          tv.tv_sec++;
          tv.tv_usec = 0;
        }
      else
        tv.tv_usec--;
    }
  struct itimerval it = { {0, 0}, tv };
  if (setitimer (ITIMER_REAL, &it, nullptr) == 0)
    return;
  else
    {
      if (warn && errno != ENOSYS)
        error (0, errno, _("warning: setitimer"));
    }
#endif

   

  unsigned int timeint;
  if (UINT_MAX <= duration)
    timeint = UINT_MAX;
  else
    {
      unsigned int duration_floor = duration;
      timeint = duration_floor + (duration_floor < duration);
    }
  alarm (timeint);
}

 

static int
send_sig (pid_t where, int sig)
{
   
  if (where == 0)
    signal (sig, SIG_IGN);
  return kill (where, sig);
}

 
static void
chld (int sig)
{
}


static void
cleanup (int sig)
{
  if (sig == SIGALRM)
    {
      timed_out = 1;
      sig = term_signal;
    }
  if (monitored_pid)
    {
      if (kill_after)
        {
          int saved_errno = errno;  
           
          term_signal = SIGKILL;
          settimeout (kill_after, false);
          kill_after = 0;  
          errno = saved_errno;
        }

       
      if (verbose)
        {
          char signame[MAX (SIG2STR_MAX, INT_BUFSIZE_BOUND (int))];
          if (sig2str (sig, signame) != 0)
            snprintf (signame, sizeof signame, "%d", sig);
          error (0, 0, _("sending signal %s to command %s"),
                 signame, quote (command));
        }
      send_sig (monitored_pid, sig);

       
      if (!foreground)
        {
          send_sig (0, sig);
          if (sig != SIGKILL && sig != SIGCONT)
            {
              send_sig (monitored_pid, SIGCONT);
              send_sig (0, SIGCONT);
            }
        }
    }
  else  
    _exit (128 + sig);
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("\
Usage: %s [OPTION] DURATION COMMAND [ARG]...\n\
  or:  %s [OPTION]\n"), program_name, program_name);

      fputs (_("\
Start COMMAND, and kill it if still running after DURATION.\n\
"), stdout);

      emit_mandatory_arg_note ();

      fputs (_("\
      --preserve-status\n\
                 exit with the same status as COMMAND, even when the\n\
                   command times out\n\
      --foreground\n\
                 when not running timeout directly from a shell prompt,\n\
                   allow COMMAND to read from the TTY and get TTY signals;\n\
                   in this mode, children of COMMAND will not be timed out\n\
  -k, --kill-after=DURATION\n\
                 also send a KILL signal if COMMAND is still running\n\
                   this long after the initial signal was sent\n\
  -s, --signal=SIGNAL\n\
                 specify the signal to be sent on timeout;\n\
                   SIGNAL may be a name like 'HUP' or a number;\n\
                   see 'kill -l' for a list of signals\n"), stdout);
      fputs (_("\
  -v, --verbose  diagnose to stderr any signal sent upon timeout\n"), stdout);

      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);

      fputs (_("\n\
DURATION is a floating point number with an optional suffix:\n\
's' for seconds (the default), 'm' for minutes, 'h' for hours or \
'd' for days.\nA duration of 0 disables the associated timeout.\n"), stdout);

      fputs (_("\n\
Upon timeout, send the TERM signal to COMMAND, if no other SIGNAL specified.\n\
The TERM signal kills any process that does not block or catch that signal.\n\
It may be necessary to use the KILL signal, since this signal can't be caught.\
\n"), stdout);

      fputs (_("\n\
Exit status:\n\
  124  if COMMAND times out, and --preserve-status is not specified\n\
  125  if the timeout command itself fails\n\
  126  if COMMAND is found but cannot be invoked\n\
  127  if COMMAND cannot be found\n\
  137  if COMMAND (or timeout itself) is sent the KILL (9) signal (128+9)\n\
  -    the exit status of COMMAND otherwise\n\
"), stdout);

      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

/* Given a floating point value *X, and a suffix character, SUFFIX_CHAR,
   scale *X by the multiplier implied by SUFFIX_CHAR.  SUFFIX_CHAR may
   be the NUL byte or 's' to denote seconds, 'm' for minutes, 'h' for
   hours, or 'd' for days.  If SUFFIX_CHAR is invalid, don't modify *X
   and return false.  Otherwise return true.  */

static bool
apply_time_suffix (double *x, char suffix_char)
{
  int multiplier;

  switch (suffix_char)
    {
    case 0:
    case 's':
      multiplier = 1;
      break;
    case 'm':
      multiplier = 60;
      break;
    case 'h':
      multiplier = 60 * 60;
      break;
    case 'd':
      multiplier = 60 * 60 * 24;
      break;
    default:
      return false;
    }

  *x *= multiplier;

  return true;
}

static double
parse_duration (char const *str)
{
  double duration;
  char const *ep;

  if (! (xstrtod (str, &ep, &duration, cl_strtod) || errno == ERANGE)
      /* Nonnegative interval.  */
      || ! (0 <= duration)
      /* No extra chars after the number and an optional s,m,h,d char.  */
      || (*ep && *(ep + 1))
      /* Check any suffix char and update timeout based on the suffix.  */
      || !apply_time_suffix (&duration, *ep))
    {
      error (0, 0, _("invalid time interval %s"), quote (str));
      usage (EXIT_CANCELED);
    }

  return duration;
}

static void
unblock_signal (int sig)
{
  sigset_t unblock_set;
  sigemptyset (&unblock_set);
  sigaddset (&unblock_set, sig);
  if (sigprocmask (SIG_UNBLOCK, &unblock_set, nullptr) != 0)
    error (0, errno, _("warning: sigprocmask"));
}

static void
install_sigchld (void)
{
  struct sigaction sa;
  sigemptyset (&sa.sa_mask);  /* Allow concurrent calls to handler */
  sa.sa_handler = chld;
  sa.sa_flags = SA_RESTART;   /* Restart syscalls if possible, as that's
                                 more likely to work cleanly.  */

  sigaction (SIGCHLD, &sa, nullptr);

  /* We inherit the signal mask from our parent process,
     so ensure SIGCHLD is not blocked. */
  unblock_signal (SIGCHLD);
}

static void
install_cleanup (int sigterm)
{
  struct sigaction sa;
  sigemptyset (&sa.sa_mask);  /* Allow concurrent calls to handler */
  sa.sa_handler = cleanup;
  sa.sa_flags = SA_RESTART;   /* Restart syscalls if possible, as that's
                                 more likely to work cleanly.  */

  sigaction (SIGALRM, &sa, nullptr); /* our timeout.  */
  sigaction (SIGINT, &sa, nullptr);  /* Ctrl-C at terminal for example.  */
  sigaction (SIGQUIT, &sa, nullptr); /* Ctrl-\ at terminal for example.  */
  sigaction (SIGHUP, &sa, nullptr);  /* terminal closed for example.  */
  sigaction (SIGTERM, &sa, nullptr); /* if killed, stop monitored proc.  */
  sigaction (sigterm, &sa, nullptr); /* user specified termination signal.  */
}

/* Block all signals which were registered with cleanup() as the signal
   handler, so we never kill processes after waitpid() returns.
   Also block SIGCHLD to ensure it doesn't fire between
   waitpid() polling and sigsuspend() waiting for a signal.
   Return original mask in OLD_SET.  */
static void
block_cleanup_and_chld (int sigterm, sigset_t *old_set)
{
  sigset_t block_set;
  sigemptyset (&block_set);

  sigaddset (&block_set, SIGALRM);
  sigaddset (&block_set, SIGINT);
  sigaddset (&block_set, SIGQUIT);
  sigaddset (&block_set, SIGHUP);
  sigaddset (&block_set, SIGTERM);
  sigaddset (&block_set, sigterm);

  sigaddset (&block_set, SIGCHLD);

  if (sigprocmask (SIG_BLOCK, &block_set, old_set) != 0)
    error (0, errno, _("warning: sigprocmask"));
}

/* Try to disable core dumps for this process.
   Return TRUE if successful, FALSE otherwise.  */
static bool
disable_core_dumps (void)
{
#if HAVE_PRCTL && defined PR_SET_DUMPABLE
  if (prctl (PR_SET_DUMPABLE, 0) == 0)
    return true;

#elif HAVE_SETRLIMIT && defined RLIMIT_CORE
  /* Note this doesn't disable processing by a filter in
     /proc/sys/kernel/core_pattern on Linux.  */
  if (setrlimit (RLIMIT_CORE, &(struct rlimit) {0,0}) == 0)
    return true;

#else
  return false;
#endif

  error (0, errno, _("warning: disabling core dumps failed"));
  return false;
}

int
main (int argc, char **argv)
{
  double timeout;
  char signame[SIG2STR_MAX];
  int c;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  initialize_exit_failure (EXIT_CANCELED);
  atexit (close_stdout);

  while ((c = getopt_long (argc, argv, "+k:s:v", long_options, nullptr)) != -1)
    {
      switch (c)
        {
        case 'k':
          kill_after = parse_duration (optarg);
          break;

        case 's':
          term_signal = operand2sig (optarg, signame);
          if (term_signal == -1)
            usage (EXIT_CANCELED);
          break;

        case 'v':
          verbose = true;
          break;

        case FOREGROUND_OPTION:
          foreground = true;
          break;

        case PRESERVE_STATUS_OPTION:
          preserve_status = true;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_CANCELED);
          break;
        }
    }

  if (argc - optind < 2)
    usage (EXIT_CANCELED);

  timeout = parse_duration (argv[optind++]);

  argv += optind;
  command = argv[0];

  /* Ensure we're in our own group so all subprocesses can be killed.
     Note we don't just put the child in a separate group as
     then we would need to worry about foreground and background groups
     and propagating signals between them.  */
  if (!foreground)
    setpgid (0, 0);

  /* Setup handlers before fork() so that we
     handle any signals caused by child, without races.  */
  install_cleanup (term_signal);
  signal (SIGTTIN, SIG_IGN);   /* Don't stop if background child needs tty.  */
  signal (SIGTTOU, SIG_IGN);   /* Don't stop if background child needs tty.  */
  install_sigchld ();          /* Interrupt sigsuspend() when child exits.   */

  monitored_pid = fork ();
  if (monitored_pid == -1)
    {
      error (0, errno, _("fork system call failed"));
      return EXIT_CANCELED;
    }
  else if (monitored_pid == 0)
    {                           /* child */
      /* exec doesn't reset SIG_IGN -> SIG_DFL.  */
      signal (SIGTTIN, SIG_DFL);
      signal (SIGTTOU, SIG_DFL);

      execvp (argv[0], argv);

      /* exit like sh, env, nohup, ...  */
      int exit_status = errno == ENOENT ? EXIT_ENOENT : EXIT_CANNOT_INVOKE;
      error (0, errno, _("failed to run command %s"), quote (command));
      return exit_status;
    }
  else
    {
      pid_t wait_result;
      int status;

      /* We configure timers so that SIGALRM is sent on expiry.
         Therefore ensure we don't inherit a mask blocking SIGALRM.  */
      unblock_signal (SIGALRM);

      settimeout (timeout, true);

      /* Ensure we don't cleanup() after waitpid() reaps the child,
         to avoid sending signals to a possibly different process.  */
      sigset_t cleanup_set;
      block_cleanup_and_chld (term_signal, &cleanup_set);

      while ((wait_result = waitpid (monitored_pid, &status, WNOHANG)) == 0)
        sigsuspend (&cleanup_set);  /* Wait with cleanup signals unblocked.  */

      if (wait_result < 0)
        {
          /* shouldn't happen.  */
          error (0, errno, _("error waiting for command"));
          status = EXIT_CANCELED;
        }
      else
        {
          if (WIFEXITED (status))
            status = WEXITSTATUS (status);
          else if (WIFSIGNALED (status))
            {
              int sig = WTERMSIG (status);
              if (WCOREDUMP (status))
                error (0, 0, _("the monitored command dumped core"));
              if (!timed_out && disable_core_dumps ())
                {
                  /* exit with the signal flag set.  */
                  signal (sig, SIG_DFL);
                  unblock_signal (sig);
                  raise (sig);
                }
              /* Allow users to distinguish if command was forcibly killed.
                 Needed with --foreground where we don't send SIGKILL to
                 the timeout process itself.  */
              if (timed_out && sig == SIGKILL)
                preserve_status = true;
              status = sig + 128; /* what sh returns for signaled processes.  */
            }
          else
            {
              /* shouldn't happen.  */
              error (0, 0, _("unknown status from command (%d)"), status);
              status = EXIT_FAILURE;
            }
        }

      if (timed_out && !preserve_status)
        status = EXIT_TIMEDOUT;
      return status;
    }
}
