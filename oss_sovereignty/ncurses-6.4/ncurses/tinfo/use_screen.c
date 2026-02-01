 

 

#include <curses.priv.h>

MODULE_ID("$Id: use_screen.c,v 1.12 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
use_screen(SCREEN *screen, NCURSES_SCREEN_CB func, void *data)
{
    SCREEN *save_SP;
    int code = OK;
    TR_FUNC_BFR(1);

    T((T_CALLED("use_screen(%p,%s,%p)"),
       (void *) screen,
       TR_FUNC_ARG(0, func),
       (void *) data));

     
    _nc_lock_global(curses);
    save_SP = CURRENT_SCREEN;
    set_term(screen);

    code = func(screen, data);

    set_term(save_SP);
    _nc_unlock_global(curses);
    returnCode(code);
}
