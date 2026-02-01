 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_nl.c,v 1.13 2020/02/02 23:34:34 tom Exp $")

#ifdef __EMX__
#include <io.h>
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(nl) (NCURSES_SP_DCL0)
{
    T((T_CALLED("nl(%p)"), (void *) SP_PARM));
    if (0 == SP_PARM)
	returnCode(ERR);
    SP_PARM->_nl = TRUE;
#ifdef __EMX__
    _nc_flush();
    _fsetmode(NC_OUTPUT(SP_PARM), "t");
#endif
    returnCode(OK);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
nl(void)
{
    return NCURSES_SP_NAME(nl) (CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(nonl) (NCURSES_SP_DCL0)
{
    T((T_CALLED("nonl(%p)"), (void *) SP_PARM));
    if (0 == SP_PARM)
	returnCode(ERR);
    SP_PARM->_nl = FALSE;
#ifdef __EMX__
    _nc_flush();
    _fsetmode(NC_OUTPUT(SP_PARM), "b");
#endif
    returnCode(OK);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
nonl(void)
{
    return NCURSES_SP_NAME(nonl) (CURRENT_SCREEN);
}
#endif
