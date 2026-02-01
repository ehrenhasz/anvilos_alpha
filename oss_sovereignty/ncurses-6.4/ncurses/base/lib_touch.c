 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_touch.c,v 1.16 2020/02/02 23:34:34 tom Exp $")

#undef is_linetouched

NCURSES_EXPORT(bool)
is_linetouched(WINDOW *win, int line)
{
    T((T_CALLED("is_linetouched(%p,%d)"), (void *) win, line));

     
    if (!win || (line > win->_maxy) || (line < 0)) {
	returnCode(FALSE);
    }

    returnCode(win->_line[line].firstchar != _NOCHANGE ? TRUE : FALSE);
}

NCURSES_EXPORT(bool)
is_wintouched(WINDOW *win)
{
    T((T_CALLED("is_wintouched(%p)"), (void *) win));

    if (win) {
	int i;

	for (i = 0; i <= win->_maxy; i++)
	    if (win->_line[i].firstchar != _NOCHANGE)
		returnCode(TRUE);
    }
    returnCode(FALSE);
}

NCURSES_EXPORT(int)
wtouchln(WINDOW *win, int y, int n, int changed)
{
    int i;

    T((T_CALLED("wtouchln(%p,%d,%d,%d)"), (void *) win, y, n, changed));

    if (!win || (n < 0) || (y < 0) || (y > win->_maxy))
	returnCode(ERR);

    for (i = y; i < y + n; i++) {
	if (i > win->_maxy)
	    break;
	win->_line[i].firstchar = (NCURSES_SIZE_T) (changed ? 0 : _NOCHANGE);
	win->_line[i].lastchar = (NCURSES_SIZE_T) (changed
						   ? win->_maxx
						   : _NOCHANGE);
    }
    returnCode(OK);
}
