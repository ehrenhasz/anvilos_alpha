 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_inwstr.c,v 1.9 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
winnwstr(WINDOW *win, wchar_t *wstr, int n)
{
    int count = 0;
    cchar_t *text;

    T((T_CALLED("winnwstr(%p,%p,%d)"), (void *) win, (void *) wstr, n));
    if (wstr != 0) {
	if (win) {
	    int row, col;
	    int last = 0;
	    bool done = FALSE;

	    getyx(win, row, col);

	    text = win->_line[row].text;
	    while (count < n && !done && count != ERR) {

		if (!isWidecExt(text[col])) {
		    int inx;
		    wchar_t wch;

		    for (inx = 0; (inx < CCHARW_MAX)
			 && ((wch = text[col].chars[inx]) != 0);
			 ++inx) {
			if (count + 1 > n) {
			    done = TRUE;
			    if (last == 0) {
				count = ERR;	 
			    } else {
				count = last;	 
			    }
			    break;
			}
			wstr[count++] = wch;
		    }
		}
		last = count;
		if (++col > win->_maxx) {
		    break;
		}
	    }
	}
	if (count > 0) {
	    wstr[count] = '\0';
	    T(("winnwstr returns %s", _nc_viswbuf(wstr)));
	}
    }
    returnCode(count);
}

 
NCURSES_EXPORT(int)
winwstr(WINDOW *win, wchar_t *wstr)
{
    int result = OK;

    T((T_CALLED("winwstr(%p,%p)"), (void *) win, (void *) wstr));
    if (win == 0) {
	result = ERR;
    } else if (winnwstr(win, wstr,
			CCHARW_MAX * (win->_maxx - win->_curx + 1)) == ERR) {
	result = ERR;
    }
    returnCode(result);
}
