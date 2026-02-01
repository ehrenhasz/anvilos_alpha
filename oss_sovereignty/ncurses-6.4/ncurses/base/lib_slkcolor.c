 

 

 
#include <curses.priv.h>

MODULE_ID("$Id: lib_slkcolor.c,v 1.20 2020/02/02 23:34:34 tom Exp $")

static int
_nc_slk_color(SCREEN *sp, int pair_arg)
{
    int code = ERR;

    T((T_CALLED("slk_color(%p,%d)"), (void *) sp, pair_arg));

    if (sp != 0
	&& sp->_slk != 0
	&& pair_arg >= 0
	&& pair_arg < sp->_pair_limit) {
	TR(TRACE_ATTRS, ("... current is %s", _tracech_t(CHREF(sp->_slk->attr))));
	SetPair(sp->_slk->attr, pair_arg);
	TR(TRACE_ATTRS, ("new attribute is %s", _tracech_t(CHREF(sp->_slk->attr))));
	code = OK;
    }
    returnCode(code);
}

NCURSES_EXPORT(int)
NCURSES_SP_NAME(slk_color) (NCURSES_SP_DCLx NCURSES_PAIRS_T pair_arg)
{
    return _nc_slk_color(SP_PARM, pair_arg);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
slk_color(NCURSES_PAIRS_T pair_arg)
{
    return NCURSES_SP_NAME(slk_color) (CURRENT_SCREEN, pair_arg);
}
#endif

#if NCURSES_EXT_COLORS
NCURSES_EXPORT(int)
NCURSES_SP_NAME(extended_slk_color) (NCURSES_SP_DCLx int pair_arg)
{
    return _nc_slk_color(SP_PARM, pair_arg);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
extended_slk_color(int pair_arg)
{
    return NCURSES_SP_NAME(extended_slk_color) (CURRENT_SCREEN, pair_arg);
}
#endif
#endif
