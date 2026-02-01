 

 

 
#include <curses.priv.h>

#include <SigAction.h>

MODULE_ID("$Id: lib_tstp.c,v 1.54 2022/12/24 22:22:10 tom Exp $")

#if defined(SIGTSTP) && (HAVE_SIGACTION || HAVE_SIGVEC)
#define USE_SIGTSTP 1
#else
#define USE_SIGTSTP 0
#endif

#ifdef TRACE
static const char *
signal_name(int sig)
{
    switch (sig) {
#ifdef SIGALRM
    case SIGALRM:
	return "SIGALRM";
#endif
#ifdef SIGCONT
    case SIGCONT:
	return "SIGCONT";
#endif
    case SIGINT:
	return "SIGINT";
#ifdef SIGQUIT
    case SIGQUIT:
	return "SIGQUIT";
#endif
    case SIGTERM:
	return "SIGTERM";
#ifdef SIGTSTP
    case SIGTSTP:
	return "SIGTSTP";
#endif
#ifdef SIGTTOU
    case SIGTTOU:
	return "SIGTTOU";
#endif
#ifdef SIGWINCH
    case SIGWINCH:
	return "SIGWINCH";
#endif
    default:
	return "unknown signal";
    }
}
#endif

 

#if USE_SIGTSTP
static void
handle_SIGTSTP(int dummy GCC_UNUSED)
{
    SCREEN *sp = CURRENT_SCREEN;
    sigset_t mask, omask;
    sigaction_t act, oact;

#ifdef SIGTTOU
    int sigttou_blocked;
#endif

    _nc_globals.have_sigtstp = 1;
    T(("handle_SIGTSTP() called"));

     
    if (sp != 0 && (sp->_endwin == ewRunning))
#if HAVE_TCGETPGRP
	if (tcgetpgrp(STDIN_FILENO) == getpgrp())
#endif
	    NCURSES_SP_NAME(def_prog_mode) (NCURSES_SP_ARG);

     
    (void) sigemptyset(&mask);
#ifdef SIGALRM
    (void) sigaddset(&mask, SIGALRM);
#endif
#if USE_SIGWINCH
    (void) sigaddset(&mask, SIGWINCH);
#endif
    (void) sigprocmask(SIG_BLOCK, &mask, &omask);

#ifdef SIGTTOU
    sigttou_blocked = sigismember(&omask, SIGTTOU);
    if (!sigttou_blocked) {
	(void) sigemptyset(&mask);
	(void) sigaddset(&mask, SIGTTOU);
	(void) sigprocmask(SIG_BLOCK, &mask, NULL);
    }
#endif

     
    NCURSES_SP_NAME(endwin) (NCURSES_SP_ARG);

     
    (void) sigemptyset(&mask);
    (void) sigaddset(&mask, SIGTSTP);
#ifdef SIGTTOU
    if (!sigttou_blocked) {
	 
	(void) sigaddset(&mask, SIGTTOU);
    }
#endif
    (void) sigprocmask(SIG_UNBLOCK, &mask, NULL);

     
    act.sa_handler = SIG_DFL;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#ifdef SA_RESTART
    act.sa_flags |= SA_RESTART;
#endif  
    sigaction(SIGTSTP, &act, &oact);
    kill(getpid(), SIGTSTP);

     

    T(("SIGCONT received"));
    sigaction(SIGTSTP, &oact, NULL);
    NCURSES_SP_NAME(flushinp) (NCURSES_SP_ARG);

     
    NCURSES_SP_NAME(def_shell_mode) (NCURSES_SP_ARG);

     
    NCURSES_SP_NAME(doupdate) (NCURSES_SP_ARG);

     
    (void) sigprocmask(SIG_SETMASK, &omask, NULL);
}
#endif  

static void
handle_SIGINT(int sig)
{
    SCREEN *sp = CURRENT_SCREEN;

     
    if (!_nc_globals.cleanup_nested++
	&& (sig == SIGINT || sig == SIGTERM)) {
#if HAVE_SIGACTION || HAVE_SIGVEC
	sigaction_t act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = SIG_IGN;
	if (sigaction(sig, &act, NULL) == 0)
#else
	if (signal(sig, SIG_IGN) != SIG_ERR)
#endif
	{
	    SCREEN *scan;
	    for (each_screen(scan)) {
		if (scan->_ofp != 0
		    && NC_ISATTY(fileno(scan->_ofp))) {
		    scan->_outch = NCURSES_SP_NAME(_nc_outch);
		}
		set_term(scan);
		NCURSES_SP_NAME(endwin) (NCURSES_SP_ARG);
		if (sp)
		    sp->_endwin = ewInitial;	 
	    }
	}
    }
    _exit(EXIT_FAILURE);
}

# ifndef _nc_set_read_thread
NCURSES_EXPORT(void)
_nc_set_read_thread(bool enable)
{
    _nc_lock_global(curses);
    if (enable) {
#  if USE_WEAK_SYMBOLS
	if ((pthread_self) && (pthread_kill) && (pthread_equal))
#  endif
	    _nc_globals.read_thread = pthread_self();
    } else {
	_nc_globals.read_thread = 0;
    }
    _nc_unlock_global(curses);
}
# endif

#if USE_SIGWINCH

static void
handle_SIGWINCH(int sig GCC_UNUSED)
{
    _nc_globals.have_sigwinch = 1;
# if USE_PTHREADS_EINTR
    if (_nc_globals.read_thread) {
	if (!pthread_equal(pthread_self(), _nc_globals.read_thread))
	    pthread_kill(_nc_globals.read_thread, SIGWINCH);
	_nc_globals.read_thread = 0;
    }
# endif
}
#endif  

 
static int
CatchIfDefault(int sig, void (*handler) (int))
{
    int result;
#if HAVE_SIGACTION || HAVE_SIGVEC
    sigaction_t old_act;
    sigaction_t new_act;

    memset(&new_act, 0, sizeof(new_act));
    sigemptyset(&new_act.sa_mask);
#ifdef SA_RESTART
#ifdef SIGWINCH
    if (sig != SIGWINCH)
#endif
	new_act.sa_flags |= SA_RESTART;
#endif  
    new_act.sa_handler = handler;

    if (sigaction(sig, NULL, &old_act) == 0
	&& (old_act.sa_handler == SIG_DFL
	    || old_act.sa_handler == handler
#if USE_SIGWINCH
	    || (sig == SIGWINCH && old_act.sa_handler == SIG_IGN)
#endif
	)) {
	(void) sigaction(sig, &new_act, NULL);
	result = TRUE;
    } else {
	result = FALSE;
    }
#else  
    void (*ohandler) (int);

    ohandler = signal(sig, SIG_IGN);
    if (ohandler == SIG_DFL
	|| ohandler == handler
#if USE_SIGWINCH
	|| (sig == SIGWINCH && ohandler == SIG_IGN)
#endif
	) {
	signal(sig, handler);
	result = TRUE;
    } else {
	signal(sig, ohandler);
	result = FALSE;
    }
#endif
    T(("CatchIfDefault - will %scatch %s",
       result ? "" : "not ", signal_name(sig)));
    return result;
}

 
NCURSES_EXPORT(void)
_nc_signal_handler(int enable)
{
    T((T_CALLED("_nc_signal_handler(%d)"), enable));
#if USE_SIGTSTP			 
    {
	static bool ignore_tstp = FALSE;

	if (!ignore_tstp) {
	    static sigaction_t new_sigaction, old_sigaction;

	    if (!enable) {
		new_sigaction.sa_handler = SIG_IGN;
		sigaction(SIGTSTP, &new_sigaction, &old_sigaction);
	    } else if (new_sigaction.sa_handler != SIG_DFL) {
		sigaction(SIGTSTP, &old_sigaction, NULL);
	    } else if (sigaction(SIGTSTP, NULL, &old_sigaction) == 0
		       && (old_sigaction.sa_handler == SIG_DFL)) {
		sigemptyset(&new_sigaction.sa_mask);
#ifdef SA_RESTART
		new_sigaction.sa_flags |= SA_RESTART;
#endif  
		new_sigaction.sa_handler = handle_SIGTSTP;
		(void) sigaction(SIGTSTP, &new_sigaction, NULL);
	    } else {
		ignore_tstp = TRUE;
	    }
	}
    }
#endif  

    if (!_nc_globals.init_signals) {
	if (enable) {
	    CatchIfDefault(SIGINT, handle_SIGINT);
	    CatchIfDefault(SIGTERM, handle_SIGINT);
#if USE_SIGWINCH
	    CatchIfDefault(SIGWINCH, handle_SIGWINCH);
#endif
	    _nc_globals.init_signals = TRUE;
	}
    }
    returnVoid;
}
