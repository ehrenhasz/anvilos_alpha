 

 

 
#include <curses.priv.h>

MODULE_ID("$Id: lib_slkatrset.c,v 1.11 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(slk_attrset) (NCURSES_SP_DCLx const chtype attr)
{
    T((T_CALLED("slk_attrset(%p,%s)"), (void *) SP_PARM, _traceattr(attr)));

    if (SP_PARM != 0 && SP_PARM->_slk != 0) {
	SetAttr(SP_PARM->_slk->attr, attr);
	returnCode(OK);
    } else
	returnCode(ERR);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
slk_attrset(const chtype attr)
{
    return NCURSES_SP_NAME(slk_attrset) (CURRENT_SCREEN, attr);
}
#endif
