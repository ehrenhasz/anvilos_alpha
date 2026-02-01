 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_wunctrl.c,v 1.17 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(wchar_t *)
NCURSES_SP_NAME(wunctrl) (NCURSES_SP_DCLx cchar_t *wc)
{
    static wchar_t str[CCHARW_MAX + 1], *wsp;
    wchar_t *result;

    if (wc == 0) {
	result = 0;
    } else if (SP_PARM != 0 && Charable(*wc)) {
	const char *p =
	NCURSES_SP_NAME(unctrl) (NCURSES_SP_ARGx
				 (unsigned) _nc_to_char((wint_t)CharOf(*wc)));

	for (wsp = str; *p; ++p) {
	    *wsp++ = (wchar_t) _nc_to_widechar(*p);
	}
	*wsp = 0;
	result = str;
    } else {
	result = wc->chars;
    }
    return result;
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(wchar_t *)
wunctrl(cchar_t *wc)
{
    return NCURSES_SP_NAME(wunctrl) (CURRENT_SCREEN, wc);
}
#endif
