 

 

 

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_has_cap.c,v 1.11 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(bool)
NCURSES_SP_NAME(has_ic) (NCURSES_SP_DCL0)
{
    bool code = FALSE;

    T((T_CALLED("has_ic(%p)"), (void *) SP_PARM));

    if (HasTInfoTerminal(SP_PARM)) {
	code = ((insert_character || parm_ich
		 || (enter_insert_mode && exit_insert_mode))
		&& (delete_character || parm_dch)) ? TRUE : FALSE;
    }

    returnCode(code);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(bool)
has_ic(void)
{
    return NCURSES_SP_NAME(has_ic) (CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(bool)
NCURSES_SP_NAME(has_il) (NCURSES_SP_DCL0)
{
    bool code = FALSE;
    T((T_CALLED("has_il(%p)"), (void *) SP_PARM));
    if (HasTInfoTerminal(SP_PARM)) {
	code = ((insert_line || parm_insert_line)
		&& (delete_line || parm_delete_line)) ? TRUE : FALSE;
    }

    returnCode(code);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(bool)
has_il(void)
{
    return NCURSES_SP_NAME(has_il) (CURRENT_SCREEN);
}
#endif
