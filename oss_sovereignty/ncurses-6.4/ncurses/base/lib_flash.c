 

 

 

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_flash.c,v 1.15 2020/02/02 23:34:34 tom Exp $")

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(flash) (NCURSES_SP_DCL0)
{
    int res = ERR;

    T((T_CALLED("flash(%p)"), (void *) SP_PARM));
#ifdef USE_TERM_DRIVER
    if (SP_PARM != 0)
	res = CallDriver_1(SP_PARM, td_doBeepOrFlash, FALSE);
#else
    if (HasTerminal(SP_PARM)) {
	 
	if (flash_screen) {
	    res = NCURSES_PUTP2_FLUSH("flash_screen", flash_screen);
	} else if (bell) {
	    res = NCURSES_PUTP2_FLUSH("bell", bell);
	}
    }
#endif
    returnCode(res);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
flash(void)
{
    return NCURSES_SP_NAME(flash) (CURRENT_SCREEN);
}
#endif
