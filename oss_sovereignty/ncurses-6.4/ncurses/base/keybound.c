 

 

#include <curses.priv.h>

MODULE_ID("$Id: keybound.c,v 1.12 2020/02/02 23:34:34 tom Exp $")

 
NCURSES_EXPORT(char *)
NCURSES_SP_NAME(keybound) (NCURSES_SP_DCLx int code, int count)
{
    char *result = 0;

    T((T_CALLED("keybound(%p, %d,%d)"), (void *) SP_PARM, code, count));
    if (SP_PARM != 0 && code >= 0) {
	result = _nc_expand_try(SP_PARM->_keytry,
				(unsigned) code,
				&count,
				(size_t) 0);
    }
    returnPtr(result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(char *)
keybound(int code, int count)
{
    return NCURSES_SP_NAME(keybound) (CURRENT_SCREEN, code, count);
}
#endif
