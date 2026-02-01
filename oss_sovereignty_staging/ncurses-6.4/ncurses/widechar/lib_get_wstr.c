 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_get_wstr.c,v 1.20 2021/10/23 19:02:59 tom Exp $")

static int
wadd_wint(WINDOW *win, wint_t *src)
{
    cchar_t tmp;
    wchar_t wch[2];

    wch[0] = (wchar_t) (*src);
    wch[1] = 0;
    setcchar(&tmp, wch, A_NORMAL, (short) 0, NULL);
    return wadd_wch(win, &tmp);
}

 
static wint_t *
WipeOut(WINDOW *win, int y, int x, wint_t *first, wint_t *last, int echoed)
{
    if (last > first) {
	*--last = '\0';
	if (echoed) {
	    int y1 = win->_cury;
	    int x1 = win->_curx;
	    int n;

	    wmove(win, y, x);
	    for (n = 0; first[n] != 0; ++n) {
		wadd_wint(win, first + n);
	    }
	    getyx(win, y, x);
	    while (win->_cury < y1
		   || (win->_cury == y1 && win->_curx < x1))
		waddch(win, (chtype) ' ');

	    wmove(win, y, x);
	}
    }
    return last;
}

NCURSES_EXPORT(int)
wgetn_wstr(WINDOW *win, wint_t *str, int maxlen)
{
    SCREEN *sp = _nc_screen_of(win);
    TTY buf;
    bool oldnl, oldecho, oldraw, oldcbreak;
    wchar_t erasec = 0;
    wchar_t killc = 0;
    wint_t *oldstr = str;
    wint_t *tmpstr = str;
    wint_t ch;
    int y, x, code;

    T((T_CALLED("wgetn_wstr(%p,%p, %d)"), (void *) win, (void *) str, maxlen));

    if (!win)
	returnCode(ERR);

    maxlen = _nc_getstr_limit(maxlen);

    _nc_get_tty_mode(&buf);

    oldnl = sp->_nl;
    oldecho = sp->_echo;
    oldraw = sp->_raw;
    oldcbreak = sp->_cbreak;
    NCURSES_SP_NAME(nl) (NCURSES_SP_ARG);
    NCURSES_SP_NAME(noecho) (NCURSES_SP_ARG);
    NCURSES_SP_NAME(raw) (NCURSES_SP_ARG);

    NCURSES_SP_NAME(erasewchar) (NCURSES_SP_ARGx &erasec);
    NCURSES_SP_NAME(killwchar) (NCURSES_SP_ARGx &killc);

    getyx(win, y, x);

    if (is_wintouched(win) || (win->_flags & _HASMOVED))
	wrefresh(win);

    while ((code = wget_wch(win, &ch)) != ERR) {
	 
	if (ch == '\r')
	    ch = '\n';
	if (ch == '\n') {
	    code = KEY_CODE_YES;
	    ch = KEY_ENTER;
	}
	if (ch != 0 && ch < KEY_MIN) {
	    if (ch == (wint_t) erasec) {
		ch = KEY_BACKSPACE;
		code = KEY_CODE_YES;
	    }
	    if (ch == (wint_t) killc) {
		ch = KEY_EOL;
		code = KEY_CODE_YES;
	    }
	}
	if (code == KEY_CODE_YES) {
	     
	    if (ch == KEY_DOWN || ch == KEY_ENTER) {
		if (oldecho == TRUE
		    && win->_cury == win->_maxy
		    && win->_scroll)
		    wechochar(win, (chtype) '\n');
		break;
	    }
	    if (ch == KEY_LEFT || ch == KEY_BACKSPACE) {
		if (tmpstr > oldstr) {
		    tmpstr = WipeOut(win, y, x, oldstr, tmpstr, oldecho);
		}
	    } else if (ch == KEY_EOL) {
		while (tmpstr > oldstr) {
		    tmpstr = WipeOut(win, y, x, oldstr, tmpstr, oldecho);
		}
	    } else {
		beep();
	    }
	} else if (tmpstr - oldstr >= maxlen) {
	    beep();
	} else {
	    *tmpstr++ = ch;
	    *tmpstr = 0;
	    if (oldecho == TRUE) {
		int oldy = win->_cury;

		if (wadd_wint(win, tmpstr - 1) == ERR) {
		     
		    win->_flags &= ~_WRAPPED;
		    waddch(win, (chtype) ' ');
		    tmpstr = WipeOut(win, y, x, oldstr, tmpstr, oldecho);
		    continue;
		} else if (IS_WRAPPED(win)) {
		     
		    if (win->_scroll
			&& oldy == win->_maxy
			&& win->_cury == win->_maxy) {
			if (--y <= 0) {
			    y = 0;
			}
		    }
		    win->_flags &= ~_WRAPPED;
		}
		wrefresh(win);
	    }
	}
    }

    win->_curx = 0;
    win->_flags &= ~_WRAPPED;
    if (win->_cury < win->_maxy)
	win->_cury++;
    wrefresh(win);

     
    sp->_nl = oldnl;
    sp->_echo = oldecho;
    sp->_raw = oldraw;
    sp->_cbreak = oldcbreak;

    (void) _nc_set_tty_mode(&buf);

    *tmpstr = 0;
    if (code == ERR) {
	if (tmpstr == oldstr) {
	    *tmpstr++ = WEOF;
	    *tmpstr = 0;
	}
	returnCode(ERR);
    }

    T(("wgetn_wstr returns %s", _nc_viswibuf(oldstr)));

    returnCode(OK);
}
