 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_printw.c,v 1.28 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
printw(const char *fmt, ...)
{
    va_list argp;
    int code;

#ifdef TRACE
    va_list argq;
    va_start(argq, fmt);
    T((T_CALLED("printw(%s%s)"),
       _nc_visbuf(fmt), _nc_varargs(fmt, argq)));
    va_end(argq);
#endif

    va_start(argp, fmt);
    code = vw_printw(stdscr, fmt, argp);
    va_end(argp);

    returnCode(code);
}

NCURSES_EXPORT(int)
wprintw(WINDOW *win, const char *fmt, ...)
{
    va_list argp;
    int code;

#ifdef TRACE
    va_list argq;
    va_start(argq, fmt);
    T((T_CALLED("wprintw(%p,%s%s)"),
       (void *) win, _nc_visbuf(fmt), _nc_varargs(fmt, argq)));
    va_end(argq);
#endif

    va_start(argp, fmt);
    code = vw_printw(win, fmt, argp);
    va_end(argp);

    returnCode(code);
}

NCURSES_EXPORT(int)
mvprintw(int y, int x, const char *fmt, ...)
{
    int code;

#ifdef TRACE
    va_list argq;
    va_start(argq, fmt);
    T((T_CALLED("mvprintw(%d,%d,%s%s)"),
       y, x, _nc_visbuf(fmt), _nc_varargs(fmt, argq)));
    va_end(argq);
#endif

    if ((code = move(y, x)) != ERR) {
	va_list argp;

	va_start(argp, fmt);
	code = vw_printw(stdscr, fmt, argp);
	va_end(argp);
    }
    returnCode(code);
}

NCURSES_EXPORT(int)
mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...)
{
    int code;

#ifdef TRACE
    va_list argq;
    va_start(argq, fmt);
    T((T_CALLED("mvwprintw(%d,%d,%p,%s%s)"),
       y, x, (void *) win, _nc_visbuf(fmt), _nc_varargs(fmt, argq)));
    va_end(argq);
#endif

    if ((code = wmove(win, y, x)) != ERR) {
	va_list argp;

	va_start(argp, fmt);
	code = vw_printw(win, fmt, argp);
	va_end(argp);
    }
    returnCode(code);
}

NCURSES_EXPORT(int)
vwprintw(WINDOW *win, const char *fmt, va_list argp)
{
    char *buf;
    int code = ERR;
#if NCURSES_SP_FUNCS
    SCREEN *sp = _nc_screen_of(win);
#endif

    T((T_CALLED("vwprintw(%p,%s,va_list)"), (void *) win, _nc_visbuf(fmt)));

    buf = NCURSES_SP_NAME(_nc_printf_string) (NCURSES_SP_ARGx fmt, argp);
    if (buf != 0) {
	code = waddstr(win, buf);
    }
    returnCode(code);
}

NCURSES_EXPORT(int)
vw_printw(WINDOW *win, const char *fmt, va_list argp)
{
    char *buf;
    int code = ERR;
#if NCURSES_SP_FUNCS
    SCREEN *sp = _nc_screen_of(win);
#endif

    T((T_CALLED("vw_printw(%p,%s,va_list)"), (void *) win, _nc_visbuf(fmt)));

    buf = NCURSES_SP_NAME(_nc_printf_string) (NCURSES_SP_ARGx fmt, argp);
    if (buf != 0) {
	code = waddstr(win, buf);
    }
    returnCode(code);
}
