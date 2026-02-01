 

 

 
#include <curses.priv.h>

MODULE_ID("$Id: lib_slktouch.c,v 1.9 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(slk_touch) (NCURSES_SP_DCL0)
{
    T((T_CALLED("slk_touch(%p)"), (void *) SP_PARM));

    if (SP_PARM == 0 || SP_PARM->_slk == 0)
	returnCode(ERR);
    SP_PARM->_slk->dirty = TRUE;

    returnCode(OK);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
slk_touch(void)
{
    return NCURSES_SP_NAME(slk_touch) (CURRENT_SCREEN);
}
#endif
