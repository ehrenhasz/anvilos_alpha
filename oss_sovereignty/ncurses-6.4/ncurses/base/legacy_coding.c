 

 

#include <curses.priv.h>

MODULE_ID("$Id: legacy_coding.c,v 1.6 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(use_legacy_coding) (NCURSES_SP_DCLx int level)
{
    int result = ERR;

    T((T_CALLED("use_legacy_coding(%p,%d)"), (void *) SP_PARM, level));
    if (level >= 0 && level <= 2 && SP_PARM != 0) {
	result = SP_PARM->_legacy_coding;
	SP_PARM->_legacy_coding = level;
    }
    returnCode(result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
use_legacy_coding(int level)
{
    return NCURSES_SP_NAME(use_legacy_coding) (CURRENT_SCREEN, level);
}
#endif
