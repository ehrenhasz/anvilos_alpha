 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_window.c,v 1.32 2021/10/23 23:06:24 tom Exp $")

NCURSES_EXPORT(void)
_nc_synchook(WINDOW *win)
 
{
    if (win->_immed)
	wrefresh(win);
    if (win->_sync)
	wsyncup(win);
}

NCURSES_EXPORT(int)
mvderwin(WINDOW *win, int y, int x)
 
{
    WINDOW *orig;
    int rc = ERR;

    T((T_CALLED("mvderwin(%p,%d,%d)"), (void *) win, y, x));

    if (win != 0
	&& (orig = win->_parent) != 0
	&& (x >= 0 && y >= 0)
	&& (x + getmaxx(win) <= getmaxx(orig))
	&& (y + getmaxy(win) <= getmaxy(orig))) {
	int i;

	wsyncup(win);
	win->_parx = x;
	win->_pary = y;
	for (i = 0; i < getmaxy(win); i++)
	    win->_line[i].text = &(orig->_line[y++].text[x]);
	rc = OK;
    }
    returnCode(rc);
}

NCURSES_EXPORT(int)
syncok(WINDOW *win, bool bf)
 
{
    T((T_CALLED("syncok(%p,%d)"), (void *) win, bf));

    if (win) {
	win->_sync = bf;
	returnCode(OK);
    } else
	returnCode(ERR);
}

NCURSES_EXPORT(void)
wsyncup(WINDOW *win)
 
 
{
    WINDOW *wp;

    T((T_CALLED("wsyncup(%p)"), (void *) win));
    if (win && win->_parent) {
	for (wp = win; wp->_parent; wp = wp->_parent) {
	    int y;
	    WINDOW *pp = wp->_parent;

	    assert((wp->_pary <= pp->_maxy) &&
		   ((wp->_pary + wp->_maxy) <= pp->_maxy));

	    for (y = 0; y <= wp->_maxy; y++) {
		int left = wp->_line[y].firstchar;
		if (left >= 0) {	 
		    struct ldat *line = &(pp->_line[wp->_pary + y]);
		     
		    int right = wp->_line[y].lastchar + wp->_parx;
		    left += wp->_parx;

		    CHANGED_RANGE(line, left, right);
		}
	    }
	}
    }
    returnVoid;
}

NCURSES_EXPORT(void)
wsyncdown(WINDOW *win)
 
 
{
    T((T_CALLED("wsyncdown(%p)"), (void *) win));

    if (win != NULL && win->_parent != NULL) {
	WINDOW *pp = win->_parent;
	int y;

	 
	wsyncdown(pp);

	 
	assert((win->_pary <= pp->_maxy) &&
	       ((win->_pary + win->_maxy) <= pp->_maxy));

	for (y = 0; y <= win->_maxy; y++) {
	    if (pp->_line[win->_pary + y].firstchar >= 0) {	 
		struct ldat *line = &(win->_line[y]);
		 
		int left = pp->_line[win->_pary + y].firstchar - win->_parx;
		int right = pp->_line[win->_pary + y].lastchar - win->_parx;
		 
		if (left < 0)
		    left = 0;
		if (right > win->_maxx)
		    right = win->_maxx;
		CHANGED_RANGE(line, left, right);
	    }
	}
    }
    returnVoid;
}

NCURSES_EXPORT(void)
wcursyncup(WINDOW *win)
 
{
    WINDOW *wp;

    T((T_CALLED("wcursyncup(%p)"), (void *) win));
    for (wp = win; wp && wp->_parent; wp = wp->_parent) {
	wmove(wp->_parent, wp->_pary + wp->_cury, wp->_parx + wp->_curx);
    }
    returnVoid;
}

NCURSES_EXPORT(WINDOW *)
dupwin(WINDOW *win)
 
{
    WINDOW *nwin = 0;

    T((T_CALLED("dupwin(%p)"), (void *) win));

    if (win != 0) {
#if NCURSES_SP_FUNCS
	SCREEN *sp = _nc_screen_of(win);
#endif
	_nc_lock_global(curses);
	if (IS_PAD(win)) {
	    nwin = NCURSES_SP_NAME(newpad) (NCURSES_SP_ARGx
					    win->_maxy + 1,
					    win->_maxx + 1);
	} else {
	    nwin = NCURSES_SP_NAME(newwin) (NCURSES_SP_ARGx
					    win->_maxy + 1,
					    win->_maxx + 1,
					    win->_begy,
					    win->_begx);
	}

	if (nwin != 0) {
	    int i;
	    size_t linesize;

	    nwin->_curx = win->_curx;
	    nwin->_cury = win->_cury;
	    nwin->_maxy = win->_maxy;
	    nwin->_maxx = win->_maxx;
	    nwin->_begy = win->_begy;
	    nwin->_begx = win->_begx;
	    nwin->_yoffset = win->_yoffset;

	    nwin->_flags = win->_flags & ~_SUBWIN;
	     

	    WINDOW_ATTRS(nwin) = WINDOW_ATTRS(win);
	    nwin->_nc_bkgd = win->_nc_bkgd;

	    nwin->_notimeout = win->_notimeout;
	    nwin->_clear = win->_clear;
	    nwin->_leaveok = win->_leaveok;
	    nwin->_scroll = win->_scroll;
	    nwin->_idlok = win->_idlok;
	    nwin->_idcok = win->_idcok;
	    nwin->_immed = win->_immed;
	    nwin->_sync = win->_sync;
	    nwin->_use_keypad = win->_use_keypad;
	    nwin->_delay = win->_delay;

	    nwin->_parx = 0;
	    nwin->_pary = 0;
	    nwin->_parent = (WINDOW *) 0;
	     

	    nwin->_regtop = win->_regtop;
	    nwin->_regbottom = win->_regbottom;

	    if (IS_PAD(win))
		nwin->_pad = win->_pad;

	    linesize = (unsigned) (win->_maxx + 1) * sizeof(NCURSES_CH_T);
	    for (i = 0; i <= nwin->_maxy; i++) {
		memcpy(nwin->_line[i].text, win->_line[i].text, linesize);
		nwin->_line[i].firstchar = win->_line[i].firstchar;
		nwin->_line[i].lastchar = win->_line[i].lastchar;
	    }
	}
	_nc_unlock_global(curses);
    }
    returnWin(nwin);
}
