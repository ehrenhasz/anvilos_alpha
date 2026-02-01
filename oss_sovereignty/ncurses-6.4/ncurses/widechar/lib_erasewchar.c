 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_erasewchar.c,v 1.5 2021/05/22 23:51:14 tom Exp $")

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(erasewchar) (NCURSES_SP_DCLx wchar_t *wch);
NCURSES_EXPORT(int)
NCURSES_SP_NAME(erasewchar) (NCURSES_SP_DCLx wchar_t *wch)
{
    int value;
    int result = ERR;

    T((T_CALLED("erasewchar()")));
    if ((value = NCURSES_SP_NAME(erasechar) (NCURSES_SP_ARG)) != ERR) {
	*wch = (wchar_t) value;
	result = OK;
    }
    returnCode(result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
erasewchar(wchar_t *wch)
{
    return NCURSES_SP_NAME(erasewchar) (CURRENT_SCREEN, wch);
}
#endif

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(killwchar) (NCURSES_SP_DCLx wchar_t *wch);
NCURSES_EXPORT(int)
NCURSES_SP_NAME(killwchar) (NCURSES_SP_DCLx wchar_t *wch)
{
    int value;
    int result = ERR;

    T((T_CALLED("killwchar()")));
    if ((value = NCURSES_SP_NAME(killchar) (NCURSES_SP_ARG)) != ERR) {
	*wch = (wchar_t) value;
	result = OK;
    }
    returnCode(result);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
killwchar(wchar_t *wch)
{
    return NCURSES_SP_NAME(killwchar) (CURRENT_SCREEN, wch);
}
#endif
