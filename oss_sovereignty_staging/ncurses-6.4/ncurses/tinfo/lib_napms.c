 

 

 

#include <curses.priv.h>

#if HAVE_NANOSLEEP
#include <time.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>		 
#endif
#endif

MODULE_ID("$Id: lib_napms.c,v 1.27 2020/08/15 19:45:23 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(napms) (NCURSES_SP_DCLx int ms)
{
    T((T_CALLED("napms(%d)"), ms));

#ifdef USE_TERM_DRIVER
    CallDriver_1(SP_PARM, td_nap, ms);
#else  
#if NCURSES_SP_FUNCS
    (void) sp;
#endif
#if HAVE_NANOSLEEP
    {
	struct timespec request, remaining;
	request.tv_sec = ms / 1000;
	request.tv_nsec = (ms % 1000) * 1000000;
	while (nanosleep(&request, &remaining) == -1
	       && errno == EINTR) {
	    request = remaining;
	}
    }
#elif defined(_NC_WINDOWS)
    Sleep((DWORD) ms);
#else
    _nc_timed_wait(0, 0, ms, (int *) 0 EVENTLIST_2nd(0));
#endif
#endif  

    returnCode(OK);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
napms(int ms)
{
    return NCURSES_SP_NAME(napms) (CURRENT_SCREEN, ms);
}
#endif
