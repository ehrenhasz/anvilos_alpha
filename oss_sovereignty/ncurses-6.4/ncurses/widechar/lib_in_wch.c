 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_in_wch.c,v 1.7 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
win_wch(WINDOW *win, cchar_t *wcval)
{
    int code = OK;

    TR(TRACE_CCALLS, (T_CALLED("win_wch(%p,%p)"), (void *) win, (void *) wcval));

    if (win != 0
	&& wcval != 0) {
	int row, col;

	getyx(win, row, col);

	*wcval = win->_line[row].text[col];
	TR(TRACE_CCALLS, ("data %s", _tracecchar_t(wcval)));
    } else {
	code = ERR;
    }
    TR(TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}
