 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <stdio.h>		 
#include <sys/types.h>
#include <signal.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif  

 
#include "rldefs.h"

#if defined (GWINSZ_IN_SYS_IOCTL)
#  include <sys/ioctl.h>
#endif  

 
#include "readline.h"
#include "history.h"

#include "rlprivate.h"

#if defined (HANDLE_SIGNALS)

#define SIGHANDLER_RETURN return

 
typedef void SigHandler (int);

#if defined (HAVE_POSIX_SIGNALS)
typedef struct sigaction sighandler_cxt;
#  define rl_sigaction(s, nh, oh)	sigaction(s, nh, oh)
#else
typedef struct { SigHandler *sa_handler; int sa_mask, sa_flags; } sighandler_cxt;
#  define sigemptyset(m)
#endif  

#ifndef SA_RESTART
#  define SA_RESTART 0
#endif

static SigHandler *rl_set_sighandler (int, SigHandler *, sighandler_cxt *);
static void rl_maybe_set_sighandler (int, SigHandler *, sighandler_cxt *);
static void rl_maybe_restore_sighandler (int, sighandler_cxt *);

static void rl_signal_handler (int);
static void _rl_handle_signal (int);
     
 

 
int rl_catch_signals = 1;

 
#ifdef SIGWINCH
int rl_catch_sigwinch = 1;
#else
int rl_catch_sigwinch = 0;	 
#endif

 
int volatile _rl_caught_signal = 0;	 

 
int _rl_echoctl = 0;

int _rl_intr_char = 0;
int _rl_quit_char = 0;
int _rl_susp_char = 0;

static int signals_set_flag;
static int sigwinch_set_flag;

#if defined (HAVE_POSIX_SIGNALS)
sigset_t _rl_orig_sigset;
#endif  

 
 
 
 
 

static sighandler_cxt old_int, old_term, old_hup, old_alrm, old_quit;
#if defined (SIGTSTP)
static sighandler_cxt old_tstp, old_ttou, old_ttin;
#endif
#if defined (SIGWINCH)
static sighandler_cxt old_winch;
#endif

_rl_sigcleanup_func_t *_rl_sigcleanup;
void *_rl_sigcleanarg;

 

 
void
_rl_signal_handler (int sig)
{
  _rl_caught_signal = 0;	 

#if defined (SIGWINCH)
  if (sig == SIGWINCH)
    {
      RL_SETSTATE(RL_STATE_SIGHANDLER);

      rl_resize_terminal ();
       
       
      if (rl_signal_event_hook)
	(*rl_signal_event_hook) ();

      RL_UNSETSTATE(RL_STATE_SIGHANDLER);
    }
  else
#endif
    _rl_handle_signal (sig);

  SIGHANDLER_RETURN;
}

static void
rl_signal_handler (int sig)
{
  _rl_caught_signal = sig;
  SIGHANDLER_RETURN;
}

 
static void
_rl_handle_signal (int sig)
{
  int block_sig;

#if defined (HAVE_POSIX_SIGNALS)
  sigset_t set, oset;
#else  
#  if defined (HAVE_BSD_SIGNALS)
  long omask;
#  else  
  sighandler_cxt dummy_cxt;	 
#  endif  
#endif  

  RL_SETSTATE(RL_STATE_SIGHANDLER);

#if !defined (HAVE_BSD_SIGNALS) && !defined (HAVE_POSIX_SIGNALS)
   
#  if defined (SIGALRM)
  if (sig == SIGINT || sig == SIGALRM)
#  else
  if (sig == SIGINT)
#  endif
    rl_set_sighandler (sig, SIG_IGN, &dummy_cxt);
#endif  

   
  if (_rl_sigcleanup)
    {
      (*_rl_sigcleanup) (sig, _rl_sigcleanarg);
      _rl_sigcleanup = 0;
      _rl_sigcleanarg = 0;
    }

#if defined (HAVE_POSIX_SIGNALS)
   
  block_sig = 0;	 
  sigemptyset (&set);
  sigprocmask (SIG_BLOCK, (sigset_t *)NULL, &set);
#endif

  switch (sig)
    {
    case SIGINT:
       
      _rl_reset_completion_state ();
      rl_free_line_state ();
#if defined (READLINE_CALLBACKS)
      rl_callback_sigcleanup ();
#endif

       

#if defined (SIGTSTP)
    case SIGTSTP:
    case SIGTTIN:
    case SIGTTOU:
#  if defined (HAVE_POSIX_SIGNALS)
       
      if (block_sig == 0)
	{
	  sigaddset (&set, SIGTTOU);
	  block_sig = 1;
	}
#  endif
#endif  
    
#if defined (SIGHUP)
    case SIGHUP:
#  if defined (_AIX)
      if (block_sig == 0)
	{
	  sigaddset (&set, sig);
	  block_sig = 1;
	}
#  endif 
#endif
     
    case SIGTERM:
#if defined (SIGALRM)
    case SIGALRM:
      if (sig == SIGALRM)
	_rl_timeout_handle_sigalrm ();
#endif
#if defined (SIGQUIT)
    case SIGQUIT:
#endif

#if defined (HAVE_POSIX_SIGNALS)
      if (block_sig)
	sigprocmask (SIG_BLOCK, &set, &oset);
#endif

      rl_echo_signal_char (sig);
      rl_cleanup_after_signal ();

       

#if defined (HAVE_POSIX_SIGNALS)
       
      if (block_sig)
	sigprocmask (SIG_UNBLOCK, &oset, (sigset_t *)NULL);
#endif

       

#if defined (__EMX__)
      signal (sig, SIG_ACK);
#endif

#if defined (HAVE_KILL)
      kill (getpid (), sig);
#else
      raise (sig);		 
#endif

       

      rl_reset_after_signal ();      
    }

  RL_UNSETSTATE(RL_STATE_SIGHANDLER);
  SIGHANDLER_RETURN;
}

#if defined (SIGWINCH)
static void
rl_sigwinch_handler (int sig)
{
  SigHandler *oh;

#if defined (MUST_REINSTALL_SIGHANDLERS)
  sighandler_cxt dummy_winch;

   
  rl_set_sighandler (SIGWINCH, rl_sigwinch_handler, &dummy_winch);
#endif

  RL_SETSTATE(RL_STATE_SIGHANDLER);
  _rl_caught_signal = sig;

   
  oh = (SigHandler *)old_winch.sa_handler;
  if (oh &&  oh != (SigHandler *)SIG_IGN && oh != (SigHandler *)SIG_DFL)
    (*oh) (sig);

  RL_UNSETSTATE(RL_STATE_SIGHANDLER);
  SIGHANDLER_RETURN;
}
#endif   

 

#if !defined (HAVE_POSIX_SIGNALS)
static int
rl_sigaction (int sig, sighandler_cxt *nh, sighandler_cxt *oh)
{
  oh->sa_handler = signal (sig, nh->sa_handler);
  return 0;
}
#endif  

 
static SigHandler *
rl_set_sighandler (int sig, SigHandler *handler, sighandler_cxt *ohandler)
{
  sighandler_cxt old_handler;
#if defined (HAVE_POSIX_SIGNALS)
  struct sigaction act;

  act.sa_handler = handler;
#  if defined (SIGWINCH)
  act.sa_flags = (sig == SIGWINCH) ? SA_RESTART : 0;
#  else
  act.sa_flags = 0;
#  endif  
  sigemptyset (&act.sa_mask);
  sigemptyset (&ohandler->sa_mask);
  sigaction (sig, &act, &old_handler);
#else
  old_handler.sa_handler = (SigHandler *)signal (sig, handler);
#endif  

   
   
  if (handler != rl_signal_handler || old_handler.sa_handler != rl_signal_handler)
    memcpy (ohandler, &old_handler, sizeof (sighandler_cxt));

  return (ohandler->sa_handler);
}

 
static void
rl_maybe_set_sighandler (int sig, SigHandler *handler, sighandler_cxt *ohandler)
{
  sighandler_cxt dummy;
  SigHandler *oh;

  sigemptyset (&dummy.sa_mask);
  dummy.sa_flags = 0;
  oh = rl_set_sighandler (sig, handler, ohandler);
  if (oh == (SigHandler *)SIG_IGN)
    rl_sigaction (sig, ohandler, &dummy);
}

 
static void
rl_maybe_restore_sighandler (int sig, sighandler_cxt *handler)
{
  sighandler_cxt dummy;

  sigemptyset (&dummy.sa_mask);
  dummy.sa_flags = 0;
  if (handler->sa_handler != SIG_IGN)
    rl_sigaction (sig, handler, &dummy);
}

int
rl_set_signals (void)
{
  sighandler_cxt dummy;
  SigHandler *oh;
#if defined (HAVE_POSIX_SIGNALS)
  static int sigmask_set = 0;
  static sigset_t bset, oset;
#endif

#if defined (HAVE_POSIX_SIGNALS)
  if (rl_catch_signals && sigmask_set == 0)
    {
      sigemptyset (&bset);

      sigaddset (&bset, SIGINT);
      sigaddset (&bset, SIGTERM);
#if defined (SIGHUP)
      sigaddset (&bset, SIGHUP);
#endif
#if defined (SIGQUIT)
      sigaddset (&bset, SIGQUIT);
#endif
#if defined (SIGALRM)
      sigaddset (&bset, SIGALRM);
#endif
#if defined (SIGTSTP)
      sigaddset (&bset, SIGTSTP);
#endif
#if defined (SIGTTIN)
      sigaddset (&bset, SIGTTIN);
#endif
#if defined (SIGTTOU)
      sigaddset (&bset, SIGTTOU);
#endif
      sigmask_set = 1;
    }      
#endif  

  if (rl_catch_signals && signals_set_flag == 0)
    {
#if defined (HAVE_POSIX_SIGNALS)
      sigemptyset (&_rl_orig_sigset);
      sigprocmask (SIG_BLOCK, &bset, &_rl_orig_sigset);
#endif

      rl_maybe_set_sighandler (SIGINT, rl_signal_handler, &old_int);
      rl_maybe_set_sighandler (SIGTERM, rl_signal_handler, &old_term);
#if defined (SIGHUP)
      rl_maybe_set_sighandler (SIGHUP, rl_signal_handler, &old_hup);
#endif
#if defined (SIGQUIT)
      rl_maybe_set_sighandler (SIGQUIT, rl_signal_handler, &old_quit);
#endif

#if defined (SIGALRM)
      oh = rl_set_sighandler (SIGALRM, rl_signal_handler, &old_alrm);
      if (oh == (SigHandler *)SIG_IGN)
	rl_sigaction (SIGALRM, &old_alrm, &dummy);
#if defined (HAVE_POSIX_SIGNALS) && defined (SA_RESTART)
       
      if (oh != (SigHandler *)SIG_DFL && (old_alrm.sa_flags & SA_RESTART))
	rl_sigaction (SIGALRM, &old_alrm, &dummy);
#endif  
#endif  

#if defined (SIGTSTP)
      rl_maybe_set_sighandler (SIGTSTP, rl_signal_handler, &old_tstp);
#endif  

#if defined (SIGTTOU)
      rl_maybe_set_sighandler (SIGTTOU, rl_signal_handler, &old_ttou);
#endif  

#if defined (SIGTTIN)
      rl_maybe_set_sighandler (SIGTTIN, rl_signal_handler, &old_ttin);
#endif  

      signals_set_flag = 1;

#if defined (HAVE_POSIX_SIGNALS)
      sigprocmask (SIG_SETMASK, &_rl_orig_sigset, (sigset_t *)NULL);
#endif
    }
  else if (rl_catch_signals == 0)
    {
#if defined (HAVE_POSIX_SIGNALS)
      sigemptyset (&_rl_orig_sigset);
      sigprocmask (SIG_BLOCK, (sigset_t *)NULL, &_rl_orig_sigset);
#endif
    }

#if defined (SIGWINCH)
  if (rl_catch_sigwinch && sigwinch_set_flag == 0)
    {
      rl_maybe_set_sighandler (SIGWINCH, rl_sigwinch_handler, &old_winch);
      sigwinch_set_flag = 1;
    }
#endif  

  return 0;
}

int
rl_clear_signals (void)
{
  sighandler_cxt dummy;

  if (rl_catch_signals && signals_set_flag == 1)
    {
       
      rl_maybe_restore_sighandler (SIGINT, &old_int);
      rl_maybe_restore_sighandler (SIGTERM, &old_term);
#if defined (SIGHUP)
      rl_maybe_restore_sighandler (SIGHUP, &old_hup);
#endif
#if defined (SIGQUIT)
      rl_maybe_restore_sighandler (SIGQUIT, &old_quit);
#endif
#if defined (SIGALRM)
      rl_maybe_restore_sighandler (SIGALRM, &old_alrm);
#endif

#if defined (SIGTSTP)
      rl_maybe_restore_sighandler (SIGTSTP, &old_tstp);
#endif  

#if defined (SIGTTOU)
      rl_maybe_restore_sighandler (SIGTTOU, &old_ttou);
#endif  

#if defined (SIGTTIN)
      rl_maybe_restore_sighandler (SIGTTIN, &old_ttin);
#endif  

      signals_set_flag = 0;
    }

#if defined (SIGWINCH)
  if (rl_catch_sigwinch && sigwinch_set_flag == 1)
    {
      sigemptyset (&dummy.sa_mask);
      rl_sigaction (SIGWINCH, &old_winch, &dummy);
      sigwinch_set_flag = 0;
    }
#endif

  return 0;
}

 
void
rl_cleanup_after_signal (void)
{
  _rl_clean_up_for_exit ();
  if (rl_deprep_term_function)
    (*rl_deprep_term_function) ();
  rl_clear_pending_input ();
  rl_clear_signals ();
}

 
void
rl_reset_after_signal (void)
{
  if (rl_prep_term_function)
    (*rl_prep_term_function) (_rl_meta_flag);
  rl_set_signals ();
}

  
void
rl_free_line_state (void)
{
  register HIST_ENTRY *entry;

  rl_free_undo_list ();

  entry = current_history ();
  if (entry)
    entry->data = (char *)NULL;

  _rl_kill_kbd_macro ();
  rl_clear_message ();
  _rl_reset_argument ();
}

int
rl_pending_signal (void)
{
  return (_rl_caught_signal);
}

void
rl_check_signals (void)
{
  RL_CHECK_SIGNALS ();
}
#endif   

 
 
 
 
 

#if defined (HAVE_POSIX_SIGNALS)
static sigset_t sigint_set, sigint_oset;
static sigset_t sigwinch_set, sigwinch_oset;
#else  
#  if defined (HAVE_BSD_SIGNALS)
static int sigint_oldmask;
static int sigwinch_oldmask;
#  endif  
#endif  

static int sigint_blocked;
static int sigwinch_blocked;

 
void
_rl_block_sigint (void)
{
  if (sigint_blocked)
    return;

  sigint_blocked = 1;
}

 
void
_rl_release_sigint (void)
{
  if (sigint_blocked == 0)
    return;

  sigint_blocked = 0;
  RL_CHECK_SIGNALS ();
}

 
void
_rl_block_sigwinch (void)
{
  if (sigwinch_blocked)
    return;

#if defined (SIGWINCH)

#if defined (HAVE_POSIX_SIGNALS)
  sigemptyset (&sigwinch_set);
  sigemptyset (&sigwinch_oset);
  sigaddset (&sigwinch_set, SIGWINCH);
  sigprocmask (SIG_BLOCK, &sigwinch_set, &sigwinch_oset);
#else  
#  if defined (HAVE_BSD_SIGNALS)
  sigwinch_oldmask = sigblock (sigmask (SIGWINCH));
#  else  
#    if defined (HAVE_USG_SIGHOLD)
  sighold (SIGWINCH);
#    endif  
#  endif  
#endif  

#endif  

  sigwinch_blocked = 1;
}

 
void
_rl_release_sigwinch (void)
{
  if (sigwinch_blocked == 0)
    return;

#if defined (SIGWINCH)

#if defined (HAVE_POSIX_SIGNALS)
  sigprocmask (SIG_SETMASK, &sigwinch_oset, (sigset_t *)NULL);
#else
#  if defined (HAVE_BSD_SIGNALS)
  sigsetmask (sigwinch_oldmask);
#  else  
#    if defined (HAVE_USG_SIGHOLD)
  sigrelse (SIGWINCH);
#    endif  
#  endif  
#endif  

#endif  

  sigwinch_blocked = 0;
}

 
 
 
 
 
void
rl_echo_signal_char (int sig)
{
  char cstr[3];
  int cslen, c;

  if (_rl_echoctl == 0 || _rl_echo_control_chars == 0)
    return;

  switch (sig)
    {
    case SIGINT:  c = _rl_intr_char; break;
#if defined (SIGQUIT)
    case SIGQUIT: c = _rl_quit_char; break;
#endif
#if defined (SIGTSTP)
    case SIGTSTP: c = _rl_susp_char; break;
#endif
    default: return;
    }

  if (CTRL_CHAR (c) || c == RUBOUT)
    {
      cstr[0] = '^';
      cstr[1] = CTRL_CHAR (c) ? UNCTRL (c) : '?';
      cstr[cslen = 2] = '\0';
    }
  else
    {
      cstr[0] = c;
      cstr[cslen = 1] = '\0';
    }

  _rl_output_some_chars (cstr, cslen);
}
