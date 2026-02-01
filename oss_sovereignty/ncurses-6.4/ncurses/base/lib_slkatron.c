 

 

 
#include <curses.priv.h>

MODULE_ID("$Id: lib_slkatron.c,v 1.13 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(slk_attron) (NCURSES_SP_DCLx const chtype attr)
{
    T((T_CALLED("slk_attron(%p,%s)"), (void *) SP_PARM, _traceattr(attr)));

    if (SP_PARM != 0 && SP_PARM->_slk != 0) {
	TR(TRACE_ATTRS, ("... current %s", _tracech_t(CHREF(SP_PARM->_slk->attr))));
	AddAttr(SP_PARM->_slk->attr, attr);
	if ((attr & A_COLOR) != 0) {
	    SetPair(SP_PARM->_slk->attr, PairNumber(attr));
	}
	TR(TRACE_ATTRS, ("new attribute is %s", _tracech_t(CHREF(SP_PARM->_slk->attr))));
	returnCode(OK);
    } else
	returnCode(ERR);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
slk_attron(const chtype attr)
{
    return NCURSES_SP_NAME(slk_attron) (CURRENT_SCREEN, attr);
}
#endif
