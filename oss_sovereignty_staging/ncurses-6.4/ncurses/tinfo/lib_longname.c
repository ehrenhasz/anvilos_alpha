 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_longname.c,v 1.15 2021/04/03 22:36:21 tom Exp $")

#if USE_REENTRANT
NCURSES_EXPORT(char *)
NCURSES_SP_NAME(longname) (NCURSES_SP_DCL0)
{
    static char empty[] =
    {'\0'};

    T((T_CALLED("longname(%p)"), (void *) SP_PARM));

    if (SP_PARM) {
	char *ptr;

	for (ptr = SP_PARM->_ttytype + strlen(SP_PARM->_ttytype);
	     ptr > SP_PARM->_ttytype;
	     ptr--)
	    if (*ptr == '|')
		returnPtr(ptr + 1);
	returnPtr(SP_PARM->_ttytype);
    }
    return empty;
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(char *)
longname(void)
{
    return NCURSES_SP_NAME(longname) (CURRENT_SCREEN);
}
#endif

#else

 
#if NCURSES_SP_FUNCS
NCURSES_EXPORT(char *)
NCURSES_SP_NAME(longname) (NCURSES_SP_DCL0)
{
    (void) SP_PARM;
    return longname();
}
#endif

NCURSES_EXPORT(char *)
longname(void)
{
    char *ptr;

    T((T_CALLED("longname()")));

    for (ptr = ttytype + strlen(ttytype);
	 ptr > ttytype;
	 ptr--)
	if (*ptr == '|')
	    returnPtr(ptr + 1);
    returnPtr(ttytype);
}
#endif
