 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_delwin.c,v 1.24 2021/12/11 23:56:25 tom Exp $")

static bool
cannot_delete(WINDOW *win)
{
    bool result = TRUE;

    if (IS_PAD(win)) {
	result = FALSE;
    } else {
	WINDOWLIST *p;
#ifdef USE_SP_WINDOWLIST
	SCREEN *sp = _nc_screen_of(win);
#endif

	for (each_window(SP_PARM, p)) {
	    if (&(p->win) == win) {
		result = FALSE;
	    } else if (IS_SUBWIN(&(p->win))
		       && p->win._parent == win) {
		result = TRUE;
		break;
	    }
	}
    }
    return result;
}

NCURSES_EXPORT(int)
delwin(WINDOW *win)
{
    int result = ERR;

    T((T_CALLED("delwin(%p)"), (void *) win));

    if (_nc_try_global(curses) == 0) {
	if (win == 0
	    || cannot_delete(win)) {
	    result = ERR;
	} else if (IS_PAD(win)) {
	    win->_parent = NULL;
	    result = _nc_freewin(win);
	} else {
#if NCURSES_SP_FUNCS
	    SCREEN *sp = _nc_screen_of(win);
#endif
	    if (IS_SUBWIN(win)) {
		touchwin(win->_parent);
	    } else if (CurScreen(SP_PARM) != 0) {
		touchwin(CurScreen(SP_PARM));
	    }
	    result = _nc_freewin(win);
	}
	_nc_unlock_global(curses);
    }
    returnCode(result);
}
