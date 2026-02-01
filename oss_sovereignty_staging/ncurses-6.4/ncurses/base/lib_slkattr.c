 

 

 
#include <curses.priv.h>

MODULE_ID("$Id: lib_slkattr.c,v 1.12 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(attr_t)
NCURSES_SP_NAME(slk_attr) (NCURSES_SP_DCL0)
{
    T((T_CALLED("slk_attr(%p)"), (void *) SP_PARM));

    if (SP_PARM != 0 && SP_PARM->_slk != 0) {
	attr_t result = AttrOf(SP_PARM->_slk->attr) & ALL_BUT_COLOR;
	int pair = GetPair(SP_PARM->_slk->attr);

	result |= (attr_t) ColorPair(pair);
	returnAttr(result);
    } else
	returnAttr(0);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(attr_t)
slk_attr(void)
{
    return NCURSES_SP_NAME(slk_attr) (CURRENT_SCREEN);
}
#endif
