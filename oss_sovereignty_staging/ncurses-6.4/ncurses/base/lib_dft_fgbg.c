 

 

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_dft_fgbg.c,v 1.31 2021/04/03 22:27:18 tom Exp $")

 
NCURSES_EXPORT(int)
NCURSES_SP_NAME(use_default_colors) (NCURSES_SP_DCL0)
{
    T((T_CALLED("use_default_colors(%p)"), (void *) SP_PARM));
    returnCode(NCURSES_SP_NAME(assume_default_colors) (NCURSES_SP_ARGx -1, -1));
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
use_default_colors(void)
{
    return NCURSES_SP_NAME(use_default_colors) (CURRENT_SCREEN);
}
#endif

 
NCURSES_EXPORT(int)
NCURSES_SP_NAME(assume_default_colors) (NCURSES_SP_DCLx int fg, int bg)
{
    int code = ERR;

    T((T_CALLED("assume_default_colors(%p,%d,%d)"), (void *) SP_PARM, fg, bg));
    if (SP_PARM != 0) {
#ifdef USE_TERM_DRIVER
	code = CallDriver_2(SP_PARM, td_defaultcolors, fg, bg);
#else
	if ((orig_pair || orig_colors) && !initialize_pair) {

	    SP_PARM->_default_color = isDefaultColor(fg) || isDefaultColor(bg);
	    SP_PARM->_has_sgr_39_49 = (tigetflag("AX") == TRUE);
	    SP_PARM->_default_fg = isDefaultColor(fg) ? COLOR_DEFAULT : fg;
	    SP_PARM->_default_bg = isDefaultColor(bg) ? COLOR_DEFAULT : bg;
	    if (SP_PARM->_color_pairs != 0) {
		bool save = SP_PARM->_default_color;
		SP_PARM->_assumed_color = TRUE;
		SP_PARM->_default_color = TRUE;
		NCURSES_SP_NAME(init_pair) (NCURSES_SP_ARGx 0,
					    (short)fg,
					    (short)bg);
		SP_PARM->_default_color = save;
	    }
	    code = OK;
	}
#endif
    }
    returnCode(code);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
assume_default_colors(int fg, int bg)
{
    return NCURSES_SP_NAME(assume_default_colors) (CURRENT_SCREEN, fg, bg);
}
#endif
