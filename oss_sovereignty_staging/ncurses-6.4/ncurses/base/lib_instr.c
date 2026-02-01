 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_instr.c,v 1.25 2021/04/03 22:24:18 tom Exp $")

NCURSES_EXPORT(int)
winnstr(WINDOW *win, char *str, int n)
{
    int i = 0;

    T((T_CALLED("winnstr(%p,%p,%d)"), (void *) win, str, n));

    if (!win || !str) {
	i = ERR;
    } else {
	int row = win->_cury;
	int col = win->_curx;
	NCURSES_CH_T *text = win->_line[row].text;

	if (n < 0)
	    n = win->_maxx - col + 1;

	for (; i < n;) {
#if USE_WIDEC_SUPPORT
	    cchar_t *cell = &(text[col]);
	    attr_t attrs;
	    NCURSES_PAIRS_T pair;
	    char *tmp;

	    if (!isWidecExt(*cell)) {
		wchar_t *wch;
		int n2;

		n2 = getcchar(cell, 0, 0, 0, 0);
		if (n2 > 0
		    && (wch = typeCalloc(wchar_t, (unsigned) n2 + 1)) != 0) {
		    bool done = FALSE;

		    if (getcchar(cell, wch, &attrs, &pair, 0) == OK) {
			mbstate_t state;
			size_t n3;

			init_mb(state);
			n3 = wcstombs(0, wch, (size_t) 0);
			if (!isEILSEQ(n3) && (n3 != 0)) {
			    size_t need = n3 + 10 + (size_t) i;
			    int have = (int) n3 + i;

			     
			    if (have > n || (int) need <= 0) {
				done = TRUE;
			    } else if ((tmp = typeCalloc(char, need)) == 0) {
				done = TRUE;
			    } else {
				size_t i3;

				init_mb(state);
				wcstombs(tmp, wch, n3);
				for (i3 = 0; i3 < n3; ++i3)
				    str[i++] = tmp[i3];
				free(tmp);
			    }
			}
		    }
		    free(wch);
		    if (done)
			break;
		}
	    }
#else
	    str[i++] = (char) CharOf(text[col]);
#endif
	    if (++col > win->_maxx) {
		break;
	    }
	}
	str[i] = '\0';		 
    }
    T(("winnstr returns %s", _nc_visbuf(str)));
    returnCode(i);
}
