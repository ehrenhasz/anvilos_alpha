 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_scanw.c,v 1.19 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
vwscanw(WINDOW *win, const char *fmt, va_list argp)
{
    char buf[BUFSIZ];
    int code = ERR;

    if (wgetnstr(win, buf, (int) sizeof(buf) - 1) != ERR) {
	if ((code = vsscanf(buf, fmt, argp)) == EOF) {
	    code = ERR;
	}
    }

    return code;
}

NCURSES_EXPORT(int)
vw_scanw(WINDOW *win, const char *fmt, va_list argp)
{
    char buf[BUFSIZ];
    int code = ERR;

    if (wgetnstr(win, buf, (int) sizeof(buf) - 1) != ERR) {
	if ((code = vsscanf(buf, fmt, argp)) == EOF) {
	    code = ERR;
	}
    }

    return code;
}

NCURSES_EXPORT(int)
scanw(const char *fmt, ...)
{
    int code;
    va_list ap;

    T(("scanw(\"%s\",...) called", fmt));

    va_start(ap, fmt);
    code = vw_scanw(stdscr, fmt, ap);
    va_end(ap);
    return (code);
}

NCURSES_EXPORT(int)
wscanw(WINDOW *win, const char *fmt, ...)
{
    int code;
    va_list ap;

    T(("wscanw(%p,\"%s\",...) called", (void *) win, fmt));

    va_start(ap, fmt);
    code = vw_scanw(win, fmt, ap);
    va_end(ap);
    return (code);
}

NCURSES_EXPORT(int)
mvscanw(int y, int x, const char *fmt, ...)
{
    int code;
    va_list ap;

    va_start(ap, fmt);
    code = (move(y, x) == OK) ? vw_scanw(stdscr, fmt, ap) : ERR;
    va_end(ap);
    return (code);
}

NCURSES_EXPORT(int)
mvwscanw(WINDOW *win, int y, int x, const char *fmt, ...)
{
    int code;
    va_list ap;

    va_start(ap, fmt);
    code = (wmove(win, y, x) == OK) ? vw_scanw(win, fmt, ap) : ERR;
    va_end(ap);
    return (code);
}
