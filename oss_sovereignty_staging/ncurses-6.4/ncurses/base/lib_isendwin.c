 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_isendwin.c,v 1.9 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(bool)
NCURSES_SP_NAME(isendwin) (NCURSES_SP_DCL0)
{
    if (SP_PARM == NULL)
	return FALSE;
    return (SP_PARM->_endwin == ewSuspend);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(bool)
isendwin(void)
{
    return NCURSES_SP_NAME(isendwin) (CURRENT_SCREEN);
}
#endif
