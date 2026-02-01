 

 

 
#include <curses.priv.h>

MODULE_ID("$Id: lib_slkatr_set.c,v 1.17 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(slk_attr_set) (NCURSES_SP_DCLx
			       const attr_t attr,
			       NCURSES_PAIRS_T pair_arg,
			       void *opts)
{
    int code = ERR;
    int color_pair = pair_arg;

    T((T_CALLED("slk_attr_set(%p,%s,%d)"),
       (void *) SP_PARM,
       _traceattr(attr),
       color_pair));

    set_extended_pair(opts, color_pair);
    if (SP_PARM != 0
	&& SP_PARM->_slk != 0
	&& color_pair >= 0
	&& color_pair < SP_PARM->_pair_limit) {
	TR(TRACE_ATTRS, ("... current %s", _tracech_t(CHREF(SP_PARM->_slk->attr))));
	SetAttr(SP_PARM->_slk->attr, attr);
	if (color_pair > 0) {
	    SetPair(SP_PARM->_slk->attr, color_pair);
	}
	TR(TRACE_ATTRS, ("new attribute is %s", _tracech_t(CHREF(SP_PARM->_slk->attr))));
	code = OK;
    }
    returnCode(code);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
slk_attr_set(const attr_t attr, NCURSES_COLOR_T pair_arg, void *opts)
{
    return NCURSES_SP_NAME(slk_attr_set) (CURRENT_SCREEN, attr,
					  pair_arg, opts);
}
#endif
