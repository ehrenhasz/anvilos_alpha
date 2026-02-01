 

 

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_ttyflags.c,v 1.36 2020/09/05 22:54:47 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(_nc_get_tty_mode) (NCURSES_SP_DCLx TTY * buf)
{
    TERMINAL *termp = TerminalOf(SP_PARM);
    int result = OK;

    if (buf == 0 || termp == 0) {
	result = ERR;
    } else {

#ifdef USE_TERM_DRIVER
	if (SP_PARM != 0) {
	    result = CallDriver_2(SP_PARM, td_sgmode, FALSE, buf);
	} else {
	    result = ERR;
	}
#else
	for (;;) {
	    if (GET_TTY(termp->Filedes, buf) != 0) {
		if (errno == EINTR)
		    continue;
		result = ERR;
	    }
	    break;
	}
#endif

	TR(TRACE_BITS, ("_nc_get_tty_mode(%d): %s",
			termp ? termp->Filedes : -1,
			_nc_trace_ttymode(buf)));
    }
    if (result == ERR && buf != 0)
	memset(buf, 0, sizeof(*buf));

    return (result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
_nc_get_tty_mode(TTY * buf)
{
    return NCURSES_SP_NAME(_nc_get_tty_mode) (CURRENT_SCREEN, buf);
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(_nc_set_tty_mode) (NCURSES_SP_DCLx TTY * buf)
{
    int result = OK;

    if (buf == 0 || SP_PARM == 0) {
	result = ERR;
    } else {
	TERMINAL *termp = TerminalOf(SP_PARM);

	if (0 == termp) {
	    result = ERR;
	} else {
#ifdef USE_TERM_DRIVER
	    result = CallDriver_2(SP_PARM, td_sgmode, TRUE, buf);
#else
	    for (;;) {
		if ((SET_TTY(termp->Filedes, buf) != 0)
#if USE_KLIBC_KBD
		    && !NC_ISATTY(termp->Filedes)
#endif
		    ) {
		    if (errno == EINTR)
			continue;
		    if ((errno == ENOTTY) && (SP_PARM != 0))
			SP_PARM->_notty = TRUE;
		    result = ERR;
		}
		break;
	    }
#endif
	}
	TR(TRACE_BITS, ("_nc_set_tty_mode(%d): %s",
			termp ? termp->Filedes : -1,
			_nc_trace_ttymode(buf)));
    }
    return (result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
_nc_set_tty_mode(TTY * buf)
{
    return NCURSES_SP_NAME(_nc_set_tty_mode) (CURRENT_SCREEN, buf);
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(def_shell_mode) (NCURSES_SP_DCL0)
{
    int rc = ERR;
    TERMINAL *termp = TerminalOf(SP_PARM);

    T((T_CALLED("def_shell_mode(%p) ->term %p"),
       (void *) SP_PARM, (void *) termp));

    if (termp != 0) {
#ifdef USE_TERM_DRIVER
	rc = CallDriver_2(SP_PARM, td_mode, FALSE, TRUE);
#else
	 
	if (_nc_get_tty_mode(&termp->Ottyb) == OK) {
#ifdef TERMIOS
	    if (termp->Ottyb.c_oflag & OFLAGS_TABS)
		tab = back_tab = NULL;
#elif defined(EXP_WIN32_DRIVER)
	     
#else
	    if (termp->Ottyb.sg_flags & XTABS)
		tab = back_tab = NULL;
#endif
	    rc = OK;
	}
#endif
    }
    returnCode(rc);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
def_shell_mode(void)
{
    return NCURSES_SP_NAME(def_shell_mode) (CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(def_prog_mode) (NCURSES_SP_DCL0)
{
    int rc = ERR;
    TERMINAL *termp = TerminalOf(SP_PARM);

    T((T_CALLED("def_prog_mode(%p) ->term %p"), (void *) SP_PARM, (void *) termp));

    if (termp != 0) {
#ifdef USE_TERM_DRIVER
	rc = CallDriver_2(SP_PARM, td_mode, TRUE, TRUE);
#else
	 
	if (_nc_get_tty_mode(&termp->Nttyb) == OK) {
#ifdef TERMIOS
	    termp->Nttyb.c_oflag &= (unsigned) (~OFLAGS_TABS);
#elif defined(EXP_WIN32_DRIVER)
	     
#else
	    termp->Nttyb.sg_flags &= (unsigned) (~XTABS);
#endif
	    rc = OK;
	}
#endif
    }
    returnCode(rc);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
def_prog_mode(void)
{
    return NCURSES_SP_NAME(def_prog_mode) (CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(reset_prog_mode) (NCURSES_SP_DCL0)
{
    int rc = ERR;
    TERMINAL *termp = TerminalOf(SP_PARM);

    T((T_CALLED("reset_prog_mode(%p) ->term %p"), (void *) SP_PARM, (void *) termp));

    if (termp != 0) {
#ifdef USE_TERM_DRIVER
	rc = CallDriver_2(SP_PARM, td_mode, TRUE, FALSE);
#else
	if (_nc_set_tty_mode(&termp->Nttyb) == OK) {
	    if (SP_PARM) {
		if (SP_PARM->_keypad_on)
		    _nc_keypad(SP_PARM, TRUE);
	    }
	    rc = OK;
	}
#endif
    }
    returnCode(rc);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
reset_prog_mode(void)
{
    return NCURSES_SP_NAME(reset_prog_mode) (CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(reset_shell_mode) (NCURSES_SP_DCL0)
{
    int rc = ERR;
    TERMINAL *termp = TerminalOf(SP_PARM);

    T((T_CALLED("reset_shell_mode(%p) ->term %p"),
       (void *) SP_PARM, (void *) termp));

    if (termp != 0) {
#ifdef USE_TERM_DRIVER
	rc = CallDriver_2(SP_PARM, td_mode, FALSE, FALSE);
#else
	if (SP_PARM) {
	    _nc_keypad(SP_PARM, FALSE);
	    _nc_flush();
	}
	rc = _nc_set_tty_mode(&termp->Ottyb);
#endif
    }
    returnCode(rc);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
reset_shell_mode(void)
{
    return NCURSES_SP_NAME(reset_shell_mode) (CURRENT_SCREEN);
}
#endif

static TTY *
saved_tty(NCURSES_SP_DCL0)
{
    TTY *result = 0;

    if (SP_PARM != 0) {
	result = (TTY *) & (SP_PARM->_saved_tty);
    } else {
	if (_nc_prescreen.saved_tty == 0) {
	    _nc_prescreen.saved_tty = typeCalloc(TTY, 1);
	}
	result = _nc_prescreen.saved_tty;
    }
    return result;
}

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(savetty) (NCURSES_SP_DCL0)
{
    T((T_CALLED("savetty(%p)"), (void *) SP_PARM));
    returnCode(NCURSES_SP_NAME(_nc_get_tty_mode) (NCURSES_SP_ARGx saved_tty(NCURSES_SP_ARG)));
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
savetty(void)
{
    return NCURSES_SP_NAME(savetty) (CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(resetty) (NCURSES_SP_DCL0)
{
    T((T_CALLED("resetty(%p)"), (void *) SP_PARM));
    returnCode(NCURSES_SP_NAME(_nc_set_tty_mode) (NCURSES_SP_ARGx saved_tty(NCURSES_SP_ARG)));
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
resetty(void)
{
    return NCURSES_SP_NAME(resetty) (CURRENT_SCREEN);
}
#endif
