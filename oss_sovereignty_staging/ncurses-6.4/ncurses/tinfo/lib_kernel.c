 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_kernel.c,v 1.35 2022/07/28 20:14:51 tom Exp $")

#ifdef TERMIOS
static int
_nc_vdisable(void)
{
    int value = -1;
#if defined(_POSIX_VDISABLE) && HAVE_UNISTD_H
    value = _POSIX_VDISABLE;
#endif
#if defined(_PC_VDISABLE) && HAVE_FPATHCONF
    if (value == -1) {
	value = (int) fpathconf(0, _PC_VDISABLE);
	if (value == -1) {
	    value = 0377;
	}
    }
#elif defined(VDISABLE)
    if (value == -1)
	value = VDISABLE;
#endif
    return value;
}
#endif  

 

NCURSES_EXPORT(char)
NCURSES_SP_NAME(erasechar) (NCURSES_SP_DCL0)
{
    int result = ERR;
    TERMINAL *termp = TerminalOf(SP_PARM);

    T((T_CALLED("erasechar(%p)"), (void *) SP_PARM));

    if (termp != 0) {
#ifdef TERMIOS
	result = termp->Ottyb.c_cc[VERASE];
	if (result == _nc_vdisable())
	    result = ERR;
#elif defined(EXP_WIN32_DRIVER)
	result = ERR;
#else
	result = termp->Ottyb.sg_erase;
#endif
    }
    returnChar((char) result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(char)
erasechar(void)
{
    return NCURSES_SP_NAME(erasechar) (CURRENT_SCREEN);
}
#endif

 

NCURSES_EXPORT(char)
NCURSES_SP_NAME(killchar) (NCURSES_SP_DCL0)
{
    int result = ERR;
    TERMINAL *termp = TerminalOf(SP_PARM);

    T((T_CALLED("killchar(%p)"), (void *) SP_PARM));

    if (termp != 0) {
#ifdef TERMIOS
	result = termp->Ottyb.c_cc[VKILL];
	if (result == _nc_vdisable())
	    result = ERR;
#elif defined(EXP_WIN32_DRIVER)
	result = ERR;
#else
	result = termp->Ottyb.sg_kill;
#endif
    }
    returnChar((char) result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(char)
killchar(void)
{
    return NCURSES_SP_NAME(killchar) (CURRENT_SCREEN);
}
#endif

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(flushinp) (NCURSES_SP_DCL0)
{
    TERMINAL *termp = TerminalOf(SP_PARM);

    T((T_CALLED("flushinp(%p)"), (void *) SP_PARM));

    if (termp != 0) {
#ifdef TERMIOS
	tcflush(termp->Filedes, TCIFLUSH);
#else
	errno = 0;
	do {
#if defined(EXP_WIN32_DRIVER)
	    _nc_console_flush(_nc_console_fd2handle(termp->Filedes));
#else
	    ioctl(termp->Filedes, TIOCFLUSH, 0);
#endif
	} while
	    (errno == EINTR);
#endif
	if (SP_PARM) {
	    SP_PARM->_fifohead = -1;
	    SP_PARM->_fifotail = 0;
	    SP_PARM->_fifopeek = 0;
	}
	returnCode(OK);
    }
    returnCode(ERR);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
flushinp(void)
{
    return NCURSES_SP_NAME(flushinp) (CURRENT_SCREEN);
}
#endif
