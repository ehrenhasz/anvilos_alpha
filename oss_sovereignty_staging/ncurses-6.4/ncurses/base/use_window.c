 

 

#include <curses.priv.h>

MODULE_ID("$Id: use_window.c,v 1.13 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
use_window(WINDOW *win, NCURSES_WINDOW_CB func, void *data)
{
    int code = OK;
    TR_FUNC_BFR(1);

    T((T_CALLED("use_window(%p,%s,%p)"),
       (void *) win,
       TR_FUNC_ARG(0, func),
       data));

    _nc_lock_global(curses);
    code = func(win, data);
    _nc_unlock_global(curses);

    returnCode(code);
}
