 

#include <curses.priv.h>

MODULE_ID("$Id: lib_termname.c,v 1.13 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(char *)
NCURSES_SP_NAME(termname) (NCURSES_SP_DCL0)
{
    char *name = 0;

    T((T_CALLED("termname(%p)"), (void *) SP_PARM));

#if NCURSES_SP_FUNCS
    if (TerminalOf(SP_PARM) != 0) {
	name = TerminalOf(SP_PARM)->_termname;
    }
#else
    if (cur_term != 0)
	name = cur_term->_termname;
#endif

    returnPtr(name);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(char *)
termname(void)
{
    return NCURSES_SP_NAME(termname) (CURRENT_SCREEN);
}
#endif
