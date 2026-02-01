 

 

 

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: lib_insnstr.c,v 1.8 2022/05/21 17:35:15 tom Exp $")

NCURSES_EXPORT(int)
winsnstr(WINDOW *win, const char *s, int n)
{
    int code = ERR;
    const unsigned char *str = (const unsigned char *) s;

    T((T_CALLED("winsnstr(%p,%s,%d)"), (void *) win, _nc_visbufn(s, n), n));

    if (win != 0 && str != 0) {
	SCREEN *sp = _nc_screen_of(win);
#if USE_WIDEC_SUPPORT
	 
	if (sp->_screen_unicode) {
	    size_t nn = (n > 0) ? (size_t) n : strlen(s);
	    wchar_t *buffer = typeMalloc(wchar_t, nn + 1);
	    if (buffer != 0) {
		mbstate_t state;
		size_t n3;
		init_mb(state);
		n3 = mbstowcs(buffer, s, nn);
		if (n3 != (size_t) (-1)) {
		    buffer[n3] = '\0';
		    code = wins_nwstr(win, buffer, (int) n3);
		}
		free(buffer);
	    }
	}
	if (code == ERR)
#endif
	{
	    NCURSES_SIZE_T oy = win->_cury;
	    NCURSES_SIZE_T ox = win->_curx;
	    const unsigned char *cp;

	    for (cp = str; (n <= 0 || (cp - str) < n) && *cp; cp++) {
		_nc_insert_ch(sp, win, (chtype) UChar(*cp));
	    }
	    win->_curx = ox;
	    win->_cury = oy;
	    _nc_synchook(win);
	    code = OK;
	}
    }
    returnCode(code);
}
