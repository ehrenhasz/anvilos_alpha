 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_endwin.c,v 1.25 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(endwin) (NCURSES_SP_DCL0)
{
    int code = ERR;

    T((T_CALLED("endwin(%p)"), (void *) SP_PARM));

    if (SP_PARM) {
#ifdef USE_TERM_DRIVER
	TERMINAL_CONTROL_BLOCK *TCB = TCBOf(SP_PARM);

	SP_PARM->_endwin = ewSuspend;
	if (TCB && TCB->drv && TCB->drv->td_scexit)
	    TCB->drv->td_scexit(SP_PARM);
#else
	SP_PARM->_endwin = ewSuspend;
	SP_PARM->_mouse_wrap(SP_PARM);
	_nc_screen_wrap();
	_nc_mvcur_wrap();	 
#endif
	code = NCURSES_SP_NAME(reset_shell_mode) (NCURSES_SP_ARG);
    }

    returnCode(code);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
endwin(void)
{
    return NCURSES_SP_NAME(endwin) (CURRENT_SCREEN);
}
#endif
