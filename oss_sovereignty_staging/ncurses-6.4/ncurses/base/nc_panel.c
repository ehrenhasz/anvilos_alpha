 

 

#include <curses.priv.h>

MODULE_ID("$Id: nc_panel.c,v 1.6 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(struct panelhook *)
NCURSES_SP_NAME(_nc_panelhook) (NCURSES_SP_DCL0)
{
    return (SP_PARM
	    ? &(SP_PARM->_panelHook)
	    : (CURRENT_SCREEN
	       ? &(CURRENT_SCREEN->_panelHook)
	       : 0));
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(struct panelhook *)
_nc_panelhook(void)
{
    return NCURSES_SP_NAME(_nc_panelhook) (CURRENT_SCREEN);
}
#endif
