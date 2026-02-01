 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_redrawln.c,v 1.18 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
wredrawln(WINDOW *win, int beg, int num)
{
    int i;
    int end;
    size_t len;
    SCREEN *sp;

    T((T_CALLED("wredrawln(%p,%d,%d)"), (void *) win, beg, num));

    if (win == 0)
	returnCode(ERR);

    sp = _nc_screen_of(win);

    if (beg < 0)
	beg = 0;

    if (touchline(win, beg, num) == ERR)
	returnCode(ERR);

    if (touchline(CurScreen(sp), beg + win->_begy, num) == ERR)
	returnCode(ERR);

    end = beg + num;
    if (end > CurScreen(sp)->_maxy + 1 - win->_begy)
	end = CurScreen(sp)->_maxy + 1 - win->_begy;
    if (end > win->_maxy + 1)
	end = win->_maxy + 1;

    len = (size_t) (win->_maxx + 1);
    if (len > (size_t) (CurScreen(sp)->_maxx + 1 - win->_begx))
	len = (size_t) (CurScreen(sp)->_maxx + 1 - win->_begx);
    len *= sizeof(CurScreen(sp)->_line[0].text[0]);

    for (i = beg; i < end; i++) {
	int crow = i + win->_begy;

	memset(CurScreen(sp)->_line[crow].text + win->_begx, 0, len);
	NCURSES_SP_NAME(_nc_make_oldhash) (NCURSES_SP_ARGx crow);
    }

    returnCode(OK);
}
