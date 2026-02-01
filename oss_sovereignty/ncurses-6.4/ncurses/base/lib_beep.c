 

 

 

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_beep.c,v 1.18 2020/02/02 23:34:34 tom Exp $")

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(beep) (NCURSES_SP_DCL0)
{
    int res = ERR;

    T((T_CALLED("beep(%p)"), (void *) SP_PARM));

#ifdef USE_TERM_DRIVER
    if (SP_PARM != 0)
	res = CallDriver_1(SP_PARM, td_doBeepOrFlash, TRUE);
#else
     
    if (cur_term == 0) {
	res = ERR;
    } else if (bell) {
	res = NCURSES_PUTP2_FLUSH("bell", bell);
    } else if (flash_screen) {
	res = NCURSES_PUTP2_FLUSH("flash_screen", flash_screen);
	_nc_flush();
    }
#endif

    returnCode(res);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
beep(void)
{
    return NCURSES_SP_NAME(beep) (CURRENT_SCREEN);
}
#endif
