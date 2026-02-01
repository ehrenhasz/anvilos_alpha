 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_data.c,v 1.87 2022/07/09 22:03:21 tom Exp $")

 
#if USE_REENTRANT
NCURSES_EXPORT(WINDOW *)
NCURSES_PUBLIC_VAR(stdscr) (void)
{
    return CURRENT_SCREEN ? StdScreen(CURRENT_SCREEN) : 0;
}
NCURSES_EXPORT(WINDOW *)
NCURSES_PUBLIC_VAR(curscr) (void)
{
    return CURRENT_SCREEN ? CurScreen(CURRENT_SCREEN) : 0;
}
NCURSES_EXPORT(WINDOW *)
NCURSES_PUBLIC_VAR(newscr) (void)
{
    return CURRENT_SCREEN ? NewScreen(CURRENT_SCREEN) : 0;
}
#else
NCURSES_EXPORT_VAR(WINDOW *) stdscr = 0;
NCURSES_EXPORT_VAR(WINDOW *) curscr = 0;
NCURSES_EXPORT_VAR(WINDOW *) newscr = 0;
#endif

NCURSES_EXPORT_VAR(SCREEN *) _nc_screen_chain = 0;

 
#if BROKEN_LINKER
static SCREEN *my_screen;

NCURSES_EXPORT(SCREEN *)
_nc_screen(void)
{
    return my_screen;
}

NCURSES_EXPORT(int)
_nc_alloc_screen(void)
{
    my_screen = _nc_alloc_screen_sp();
    T(("_nc_alloc_screen_sp %p", my_screen));
    return (my_screen != 0);
}

NCURSES_EXPORT(void)
_nc_set_screen(SCREEN *sp)
{
    my_screen = sp;
}

#else

NCURSES_EXPORT_VAR(SCREEN *) SP = NULL;  
#endif
 
#define CHARS_0s { '\0' }

#define TGETENT_0 { 0L, FALSE, NULL, NULL, NULL }
#define TGETENT_0s { TGETENT_0, TGETENT_0, TGETENT_0, TGETENT_0 }

NCURSES_EXPORT_VAR(NCURSES_GLOBALS) _nc_globals = {
    0,				 
    0,				 
    0,				 

    FALSE,			 
    FALSE,			 

    NULL,			 
    NULL,			 

    FALSE,			 
    FALSE,			 
    0,				 

    NULL,			 
    0,				 

    NULL,			 
    NULL,			 
    0,				 

    0,				 

    2048,			 

    NULL,			 
    0,				 

    TGETENT_0s,			 
    0,				 
    0,				 
    0,				 

    0,				 
    0,				 
    0,				 
    0,				 
    { { 0, 0 } },		 

#if HAVE_TSEARCH
    NULL,			 
    0,				 
#endif  

#ifdef USE_TERM_DRIVER
    0,				 
#endif

#ifndef USE_SP_WINDOWLIST
    0,				 
#endif

#if USE_HOME_TERMINFO
    NULL,			 
#endif

#if !USE_SAFE_SPRINTF
    0,				 
    0,				 
#endif

#ifdef USE_PTHREADS
    PTHREAD_MUTEX_INITIALIZER,	 
    PTHREAD_MUTEX_INITIALIZER,	 
    PTHREAD_MUTEX_INITIALIZER,	 
    PTHREAD_MUTEX_INITIALIZER,	 
    PTHREAD_MUTEX_INITIALIZER,	 
    PTHREAD_MUTEX_INITIALIZER,	 
    0,				 
    0,				 
#if USE_PTHREADS_EINTR
    0,				 
#endif
#endif
#if USE_WIDEC_SUPPORT
    CHARS_0s,			 
#endif
#ifdef TRACE
    FALSE,			 
    CHARS_0s,			 
    0,				 
    NULL,			 
    -1,				 

    NULL,			 
    0,				 

    NULL,			 
    0,				 

    CHARS_0s,			 

    NULL,			 
    0,				 

    NULL,			 
    0,				 

    { CHARS_0s, CHARS_0s },	 
    0,				 
    -1,				 
#if !defined(USE_PTHREADS) && USE_REENTRANT
    0,				 
#endif
#endif  
#if NO_LEAKS
    FALSE,			 
#endif
};

#define STACK_FRAME_0	{ { 0 }, 0 }
#define STACK_FRAME_0s	{ STACK_FRAME_0 }
#define NUM_VARS_0s	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }

#define RIPOFF_0	{ 0,0,0 }
#define RIPOFF_0s	{ RIPOFF_0 }

NCURSES_EXPORT_VAR(NCURSES_PRESCREEN) _nc_prescreen = {
    NULL,			 
    TRUE,			 
    FALSE,			 
    A_NORMAL,			 
    {				 
	NULL,			 

	STACK_FRAME_0s,		 
	0,			 

	NULL,			 
	0,			 
	0,			 

	NULL,			 
	0,			 

	NUM_VARS_0s,		 
#ifdef TRACE
	NULL,			 
#endif
    },
    NULL,			 
    FALSE,			 
    0,				 
#ifndef USE_SP_RIPOFF
    RIPOFF_0s,			 
    NULL,			 
#endif
#if NCURSES_NO_PADDING
    FALSE,			 
#endif
#if BROKEN_LINKER || USE_REENTRANT
    NULL,			 
    0,				 
    0,				 
    8,				 
    1000,			 
    0,				 
#endif
#ifdef TRACE
#if BROKEN_LINKER || USE_REENTRANT
    0L,				 
    NULL,			 
#endif
#endif
};
 

 
NCURSES_EXPORT(SCREEN *)
_nc_screen_of(WINDOW *win)
{
    SCREEN *sp = 0;

    if (win != 0) {
	sp = WINDOW_EXT(win, screen);
    }
    return (sp);
}

 
#ifdef USE_PTHREADS
static void
init_global_mutexes(void)
{
    static bool initialized = FALSE;

    if (!initialized) {
	initialized = TRUE;
	_nc_mutex_init(&_nc_globals.mutex_curses);
	_nc_mutex_init(&_nc_globals.mutex_prescreen);
	_nc_mutex_init(&_nc_globals.mutex_screen);
	_nc_mutex_init(&_nc_globals.mutex_update);
	_nc_mutex_init(&_nc_globals.mutex_tst_tracef);
	_nc_mutex_init(&_nc_globals.mutex_tracef);
    }
}

NCURSES_EXPORT(void)
_nc_init_pthreads(void)
{
    if (_nc_use_pthreads)
	return;
# if USE_WEAK_SYMBOLS
    if ((pthread_mutex_init) == 0)
	return;
    if ((pthread_mutex_lock) == 0)
	return;
    if ((pthread_mutex_unlock) == 0)
	return;
    if ((pthread_mutex_trylock) == 0)
	return;
    if ((pthread_mutexattr_settype) == 0)
	return;
# endif
    _nc_use_pthreads = 1;
    init_global_mutexes();
}

 
NCURSES_EXPORT(void)
_nc_mutex_init(pthread_mutex_t * obj)
{
    pthread_mutexattr_t recattr;

    if (_nc_use_pthreads) {
	pthread_mutexattr_init(&recattr);
	pthread_mutexattr_settype(&recattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(obj, &recattr);
    }
}

NCURSES_EXPORT(int)
_nc_mutex_lock(pthread_mutex_t * obj)
{
    int rc = 0;
    if (_nc_use_pthreads != 0)
	rc = pthread_mutex_lock(obj);
    return rc;
}

NCURSES_EXPORT(int)
_nc_mutex_trylock(pthread_mutex_t * obj)
{
    int rc = 0;
    if (_nc_use_pthreads != 0)
	rc = pthread_mutex_trylock(obj);
    return rc;
}

NCURSES_EXPORT(int)
_nc_mutex_unlock(pthread_mutex_t * obj)
{
    int rc = 0;
    if (_nc_use_pthreads != 0)
	rc = pthread_mutex_unlock(obj);
    return rc;
}
#endif  

#if defined(USE_PTHREADS) || USE_PTHREADS_EINTR
#if USE_WEAK_SYMBOLS
 
NCURSES_EXPORT(int)
_nc_sigprocmask(int how, const sigset_t * newmask, sigset_t * oldmask)
{
    if ((pthread_sigmask))
	return pthread_sigmask(how, newmask, oldmask);
    else
	return (sigprocmask) (how, newmask, oldmask);
}
#endif
#endif  
