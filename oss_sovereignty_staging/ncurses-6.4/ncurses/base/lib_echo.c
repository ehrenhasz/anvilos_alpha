 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_echo.c,v 1.9 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(echo) (NCURSES_SP_DCL0)
{
    T((T_CALLED("echo(%p)"), (void *) SP_PARM));
    if (0 == SP_PARM)
	returnCode(ERR);
    SP_PARM->_echo = TRUE;
    returnCode(OK);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
echo(void)
{
    return NCURSES_SP_NAME(echo) (CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(noecho) (NCURSES_SP_DCL0)
{
    T((T_CALLED("noecho(%p)"), (void *) SP_PARM));
    if (0 == SP_PARM)
	returnCode(ERR);
    SP_PARM->_echo = FALSE;
    returnCode(OK);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
noecho(void)
{
    return NCURSES_SP_NAME(noecho) (CURRENT_SCREEN);
}
#endif
