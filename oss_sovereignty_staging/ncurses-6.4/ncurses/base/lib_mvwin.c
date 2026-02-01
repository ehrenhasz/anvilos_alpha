 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_mvwin.c,v 1.20 2021/10/23 18:57:41 tom Exp $")

NCURSES_EXPORT(int)
mvwin(WINDOW *win, int by, int bx)
{
#if NCURSES_SP_FUNCS
    SCREEN *sp = _nc_screen_of(win);
#endif

    T((T_CALLED("mvwin(%p,%d,%d)"), (void *) win, by, bx));

    if (!win || IS_PAD(win))
	returnCode(ERR);

     
#if 0
     
    if (IS_SUBWIN(win)) {
	int err = ERR;
	WINDOW *parent = win->_parent;
	if (parent) {		 
	    if ((by - parent->_begy == win->_pary) &&
		(bx - parent->_begx == win->_parx))
		err = OK;	 
	    else {
		WINDOW *clone = dupwin(win);
		if (clone) {
		     

		    werase(win);	 
		     
		    wbkgrnd(win, CHREF(parent->_nc_bkgd));
		    wsyncup(win);	 

		    err = mvderwin(win,
				   by - parent->_begy,
				   bx - parent->_begx);
		    if (err != ERR) {
			err = copywin(clone, win,
				      0, 0, 0, 0, win->_maxy, win->_maxx, 0);
			if (ERR != err)
			    wsyncup(win);
		    }
		    if (ERR == delwin(clone))
			err = ERR;
		}
	    }
	}
	returnCode(err);
    }
#endif

    if (by + win->_maxy > screen_lines(SP_PARM) - 1
	|| bx + win->_maxx > screen_columns(SP_PARM) - 1
	|| by < 0
	|| bx < 0)
	returnCode(ERR);

     
    win->_begy = (NCURSES_SIZE_T) by;
    win->_begx = (NCURSES_SIZE_T) bx;
    returnCode(touchwin(win));
}
