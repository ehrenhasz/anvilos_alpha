 

 

 
#include <curses.priv.h>

MODULE_ID("$Id: lib_slkclear.c,v 1.15 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(slk_clear) (NCURSES_SP_DCL0)
{
    int rc = ERR;

    T((T_CALLED("slk_clear(%p)"), (void *) SP_PARM));

    if (SP_PARM != 0 && SP_PARM->_slk != 0) {
	SP_PARM->_slk->hidden = TRUE;
	 
	SP_PARM->_slk->win->_nc_bkgd = StdScreen(SP_PARM)->_nc_bkgd;
	WINDOW_ATTRS(SP_PARM->_slk->win) = WINDOW_ATTRS(StdScreen(SP_PARM));
	if (SP_PARM->_slk->win == StdScreen(SP_PARM)) {
	    rc = OK;
	} else {
	    werase(SP_PARM->_slk->win);
	    rc = wrefresh(SP_PARM->_slk->win);
	}
    }
    returnCode(rc);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
slk_clear(void)
{
    return NCURSES_SP_NAME(slk_clear) (CURRENT_SCREEN);
}
#endif
