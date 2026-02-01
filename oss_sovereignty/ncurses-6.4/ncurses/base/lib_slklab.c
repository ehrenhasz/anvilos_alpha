 

 

 
#include <curses.priv.h>

MODULE_ID("$Id: lib_slklab.c,v 1.11 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(char *)
NCURSES_SP_NAME(slk_label) (NCURSES_SP_DCLx int n)
{
    T((T_CALLED("slk_label(%p,%d)"), (void *) SP_PARM, n));

    if (SP_PARM == 0 || SP_PARM->_slk == 0 || n < 1 || n > SP_PARM->_slk->labcnt)
	returnPtr(0);
    returnPtr(SP_PARM->_slk->ent[n - 1].ent_text);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(char *)
slk_label(int n)
{
    return NCURSES_SP_NAME(slk_label) (CURRENT_SCREEN, n);
}
#endif
