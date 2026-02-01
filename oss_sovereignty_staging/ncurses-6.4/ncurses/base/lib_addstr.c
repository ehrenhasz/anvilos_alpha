 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_addstr.c,v 1.58 2022/06/11 20:12:04 tom Exp $")

NCURSES_EXPORT(int)
waddnstr(WINDOW *win, const char *astr, int n)
{
    const char *str = astr;
    int code = ERR;

    T((T_CALLED("waddnstr(%p,%s,%d)"), (void *) win, _nc_visbufn(astr, n), n));

    if (win && (str != 0)) {
	TR(TRACE_VIRTPUT | TRACE_ATTRS,
	   ("... current %s", _traceattr(WINDOW_ATTRS(win))));
	code = OK;

	TR(TRACE_VIRTPUT, ("str is not null, length = %d",
			   ((n > 0) ? n : (int) strlen(str))));
	if (n < 0)
	    n = INT_MAX;
	while ((*str != '\0') && (n-- > 0)) {
	    NCURSES_CH_T ch;
	    TR(TRACE_VIRTPUT, ("*str = %#o", UChar(*str)));
	    SetChar(ch, UChar(*str++), A_NORMAL);
	    if (_nc_waddch_nosync(win, ch) == ERR) {
		code = ERR;
		break;
	    }
	}
	_nc_synchook(win);
    }
    TR(TRACE_VIRTPUT, ("waddnstr returns %d", code));
    returnCode(code);
}

NCURSES_EXPORT(int)
waddchnstr(WINDOW *win, const chtype *astr, int n)
{
    NCURSES_SIZE_T y, x;
    int code = OK;
    int i;
    struct ldat *line;

    T((T_CALLED("waddchnstr(%p,%p,%d)"), (void *) win, (const void *) astr, n));

    if (!win || !astr)
	returnCode(ERR);

    y = win->_cury;
    x = win->_curx;
    if (n < 0) {
	const chtype *str;
	n = 0;
	for (str = (const chtype *) astr; *str != 0; str++)
	    n++;
    }
    if (n > win->_maxx - x + 1)
	n = win->_maxx - x + 1;
    if (n == 0)
	returnCode(code);

    line = &(win->_line[y]);
    for (i = 0; i < n && ChCharOf(astr[i]) != '\0'; ++i) {
	SetChar2(line->text[i + x], astr[i]);
    }
    CHANGED_RANGE(line, x, (NCURSES_SIZE_T) (x + n - 1));

    _nc_synchook(win);
    returnCode(code);
}

#if USE_WIDEC_SUPPORT

NCURSES_EXPORT(int)
_nc_wchstrlen(const cchar_t *s)
{
    int result = 0;
    if (s != 0) {
	while (CharOf(s[result]) != L'\0') {
	    result++;
	}
    }
    return result;
}

NCURSES_EXPORT(int)
wadd_wchnstr(WINDOW *win, const cchar_t *astr, int n)
{
    static const NCURSES_CH_T blank = NewChar(BLANK_TEXT);
    NCURSES_SIZE_T y;
    NCURSES_SIZE_T x;
    int code = OK;
    struct ldat *line;
    int i, j, start, len, end;

    T((T_CALLED("wadd_wchnstr(%p,%s,%d)"),
       (void *) win,
       _nc_viscbuf(astr, n),
       n));

    if (!win)
	returnCode(ERR);

    y = win->_cury;
    x = win->_curx;
    if (n < 0) {
	n = _nc_wchstrlen(astr);
    }
    if (n > win->_maxx - x + 1)
	n = win->_maxx - x + 1;
    if (n == 0)
	returnCode(code);

    line = &(win->_line[y]);
    start = x;
    end = x + n - 1;

     
    if (x > 0 && isWidecExt(line->text[x])) {
	for (i = 0; i <= x; ++i) {
	    if (!isWidecExt(line->text[x - i])) {
		 
		start -= i;
		while (i > 0) {
		    line->text[x - i--] = _nc_render(win, blank);
		}
		break;
	    }
	}
    }

     
    for (i = 0; i < n && CharOf(astr[i]) != L'\0' && x <= win->_maxx; ++i) {
	if (isWidecExt(astr[i]))
	    continue;

	len = _nc_wacs_width(CharOf(astr[i]));

	if (x + len - 1 <= win->_maxx) {
	    line->text[x] = _nc_render(win, astr[i]);
	    if (len > 1) {
		for (j = 0; j < len; ++j) {
		    if (j != 0) {
			line->text[x + j] = line->text[x];
		    }
		    SetWidecExt(line->text[x + j], j);
		}
	    } else {
		len = 1;
	    }
	    x = (NCURSES_SIZE_T) (x + len);
	    end += len - 1;
	} else {
	    break;
	}
    }

     
    while (x <= win->_maxx && isWidecExt(line->text[x])) {
	line->text[x] = _nc_render(win, blank);
	++end;
	++x;
    }
    CHANGED_RANGE(line, start, end);

    _nc_synchook(win);
    returnCode(code);
}

NCURSES_EXPORT(int)
waddnwstr(WINDOW *win, const wchar_t *str, int n)
{
    int code = ERR;

    T((T_CALLED("waddnwstr(%p,%s,%d)"), (void *) win, _nc_viswbufn(str, n), n));

    if (win && (str != 0)) {
	TR(TRACE_VIRTPUT | TRACE_ATTRS,
	   ("... current %s", _traceattr(WINDOW_ATTRS(win))));
	code = OK;

	TR(TRACE_VIRTPUT, ("str is not null, length = %d",
			   ((n > 0) ? n : (int) wcslen(str))));
	if (n < 0)
	    n = INT_MAX;
	while ((*str != L('\0')) && (n-- > 0)) {
	    NCURSES_CH_T ch;
	    TR(TRACE_VIRTPUT, ("*str[0] = %#lx", (unsigned long) *str));
	    SetChar(ch, *str++, A_NORMAL);
	    if (wadd_wch(win, &ch) == ERR) {
		code = ERR;
		break;
	    }
	}
	_nc_synchook(win);
    }
    TR(TRACE_VIRTPUT, ("waddnwstr returns %d", code));
    returnCode(code);
}

#endif
