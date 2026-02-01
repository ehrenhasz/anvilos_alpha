 

 

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: lib_addch.c,v 1.141 2022/06/12 15:16:41 tom Exp $")

static const NCURSES_CH_T blankchar = NewChar(BLANK_TEXT);

 

 
#define COLOR_MASK(ch) (~(attr_t)(((ch) & A_COLOR) ? A_COLOR : 0))

static NCURSES_INLINE NCURSES_CH_T
render_char(WINDOW *win, NCURSES_CH_T ch)
 
{
    attr_t a = WINDOW_ATTRS(win);
    int pair = GetPair(ch);

    if (ISBLANK(ch)
	&& AttrOf(ch) == A_NORMAL
	&& pair == 0) {
	 
	ch = win->_nc_bkgd;
	SetAttr(ch, a | AttrOf(win->_nc_bkgd));
	if ((pair = GET_WINDOW_PAIR(win)) == 0)
	    pair = GetPair(win->_nc_bkgd);
	SetPair(ch, pair);
    } else {
	 
	a |= AttrOf(win->_nc_bkgd) & COLOR_MASK(a);
	 
	if (pair == 0) {
	    if ((pair = GET_WINDOW_PAIR(win)) == 0)
		pair = GetPair(win->_nc_bkgd);
	}
	AddAttr(ch, (a & COLOR_MASK(AttrOf(ch))));
	SetPair(ch, pair);
    }

    TR(TRACE_VIRTPUT,
       ("render_char bkg %s (%d), attrs %s (%d) -> ch %s (%d)",
	_tracech_t2(1, CHREF(win->_nc_bkgd)),
	GetPair(win->_nc_bkgd),
	_traceattr(WINDOW_ATTRS(win)),
	GET_WINDOW_PAIR(win),
	_tracech_t2(3, CHREF(ch)),
	GetPair(ch)));

    return (ch);
}

NCURSES_EXPORT(NCURSES_CH_T)
_nc_render(WINDOW *win, NCURSES_CH_T ch)
 
{
    return render_char(win, ch);
}

 
#ifndef NDEBUG			 
#define CHECK_POSITION(win, x, y) \
	if (y > win->_maxy \
	 || x > win->_maxx \
	 || y < 0 \
	 || x < 0) { \
		TR(TRACE_VIRTPUT, ("Alert! Win=%p _curx = %d, _cury = %d " \
				   "(_maxx = %d, _maxy = %d)", win, x, y, \
				   win->_maxx, win->_maxy)); \
		return(ERR); \
	}
#else
#define CHECK_POSITION(win, x, y)	 
#endif

static bool
newline_forces_scroll(WINDOW *win, NCURSES_SIZE_T *ypos)
{
    bool result = FALSE;

    if (*ypos >= win->_regtop && *ypos <= win->_regbottom) {
	if (*ypos == win->_regbottom) {
	    *ypos = win->_regbottom;
	    result = TRUE;
	} else if (*ypos < win->_maxy) {
	    *ypos = (NCURSES_SIZE_T) (*ypos + 1);
	}
    } else if (*ypos < win->_maxy) {
	*ypos = (NCURSES_SIZE_T) (*ypos + 1);
    }
    return result;
}

 
static int
wrap_to_next_line(WINDOW *win)
{
    win->_flags |= _WRAPPED;
    if (newline_forces_scroll(win, &(win->_cury))) {
	win->_curx = win->_maxx;
	if (!win->_scroll)
	    return (ERR);
	scroll(win);
    }
    win->_curx = 0;
    return (OK);
}

#if USE_WIDEC_SUPPORT
static int waddch_literal(WINDOW *, NCURSES_CH_T);
 
static void
fill_cells(WINDOW *win, int count)
{
    NCURSES_CH_T blank = blankchar;
    int save_x = win->_curx;
    int save_y = win->_cury;

    while (count-- > 0) {
	if (waddch_literal(win, blank) == ERR)
	    break;
    }
    win->_curx = (NCURSES_SIZE_T) save_x;
    win->_cury = (NCURSES_SIZE_T) save_y;
}
#endif

 
#if USE_WIDEC_SUPPORT
NCURSES_EXPORT(int)
_nc_build_wch(WINDOW *win, ARG_CH_T ch)
{
    char *buffer = WINDOW_EXT(win, addch_work);
    int len;
    int x = win->_curx;
    int y = win->_cury;
    mbstate_t state;
    wchar_t result;

    if ((WINDOW_EXT(win, addch_used) != 0) &&
	(WINDOW_EXT(win, addch_x) != x ||
	 WINDOW_EXT(win, addch_y) != y)) {
	 
	WINDOW_EXT(win, addch_used) = 0;
	TR(TRACE_VIRTPUT,
	   ("Alert discarded multibyte on move (%d,%d) -> (%d,%d)",
	    WINDOW_EXT(win, addch_y), WINDOW_EXT(win, addch_x),
	    y, x));
    }
    WINDOW_EXT(win, addch_x) = x;
    WINDOW_EXT(win, addch_y) = y;

     
    if (!is8bits(CharOf(CHDEREF(ch)))) {
	if (WINDOW_EXT(win, addch_used) != 0) {
	     
	    WINDOW_EXT(win, addch_used) = 0;
	    TR(TRACE_VIRTPUT,
	       ("Alert discarded incomplete multibyte"));
	}
	return 1;
    }

    init_mb(state);
    buffer[WINDOW_EXT(win, addch_used)] = (char) CharOf(CHDEREF(ch));
    WINDOW_EXT(win, addch_used) += 1;
    buffer[WINDOW_EXT(win, addch_used)] = '\0';
    if ((len = (int) mbrtowc(&result,
			     buffer,
			     (size_t) WINDOW_EXT(win, addch_used),
			     &state)) > 0) {
	attr_t attrs = AttrOf(CHDEREF(ch));
	if_EXT_COLORS(int pair = GetPair(CHDEREF(ch)));
	SetChar(CHDEREF(ch), result, attrs);
	if_EXT_COLORS(SetPair(CHDEREF(ch), pair));
	WINDOW_EXT(win, addch_used) = 0;
    } else if (len == -1) {
	 
	TR(TRACE_VIRTPUT, ("Alert! mbrtowc returns error"));
	 
	WINDOW_EXT(win, addch_used) = 0;
    }
    return len;
}
#endif  

static
#if !USE_WIDEC_SUPPORT		 
NCURSES_INLINE
#endif
int
waddch_literal(WINDOW *win, NCURSES_CH_T ch)
{
    int x;
    int y;
    struct ldat *line;

    x = win->_curx;
    y = win->_cury;

    CHECK_POSITION(win, x, y);

    ch = render_char(win, ch);

    line = win->_line + y;

    CHANGED_CELL(line, x);

     
#if NCURSES_SP_FUNCS
#define DeriveSP() SCREEN *sp = _nc_screen_of(win);
#else
#define DeriveSP()		 
#endif
    if_WIDEC({
	DeriveSP();
	if (WINDOW_EXT(win, addch_used) != 0 || !Charable(ch)) {
	    int len = _nc_build_wch(win, CHREF(ch));

	    if (len >= -1) {
		attr_t attr = AttrOf(ch);

		 
		if (len == -1 && is8bits(CharOf(ch))) {
		    const char *s = NCURSES_SP_NAME(unctrl)
		      (NCURSES_SP_ARGx (chtype) CharOf(ch));

		    if (s[1] != '\0') {
			int rc = OK;
			while (*s != '\0') {
			    rc = waddch(win, UChar(*s) | attr);
			    if (rc != OK)
				break;
			    ++s;
			}
			return rc;
		    }
		}
		if (len == -1)
		    return waddch(win, ' ' | attr);
	    } else {
		return OK;
	    }
	}
    });

     
    if_WIDEC({
	int len = _nc_wacs_width(CharOf(ch));
	int i;
	int j;

	if (len == 0) {		 
	    if ((x > 0 && y >= 0)
		|| (win->_maxx >= 0 && win->_cury >= 1)) {
		NCURSES_CH_T *dst;
		wchar_t *chars;
		if (x > 0 && y >= 0) {
		    for (j = x - 1; j >= 0; --j) {
			if (!isWidecExt(win->_line[y].text[j])) {
			    win->_curx = (NCURSES_SIZE_T) j;
			    break;
			}
		    }
		    dst = &(win->_line[y].text[j]);
		} else {
		    dst = &(win->_line[y - 1].text[win->_maxx]);
		}
		chars = dst->chars;
		for (i = 0; i < CCHARW_MAX; ++i) {
		    if (chars[i] == 0) {
			TR(TRACE_VIRTPUT,
			   ("adding non-spacing %s (level %d)",
			    _tracech_t(CHREF(ch)), i));
			chars[i] = CharOf(ch);
			break;
		    }
		}
	    }
	    goto testwrapping;
	} else if (len > 1) {	 
	     
	    if (len > win->_maxx + 1) {
		TR(TRACE_VIRTPUT, ("character will not fit"));
		return ERR;
	    } else if (x + len > win->_maxx + 1) {
		int count = win->_maxx + 1 - x;
		TR(TRACE_VIRTPUT, ("fill %d remaining cells", count));
		fill_cells(win, count);
		if (wrap_to_next_line(win) == ERR)
		    return ERR;
		x = win->_curx;
		y = win->_cury;
		CHECK_POSITION(win, x, y);
		line = win->_line + y;
	    }
	     
	    for (i = 0; i < len; ++i) {
		if (isWidecBase(win->_line[y].text[x + i])) {
		    break;
		} else if (isWidecExt(win->_line[y].text[x + i])) {
		    for (j = i; x + j <= win->_maxx; ++j) {
			if (!isWidecExt(win->_line[y].text[x + j])) {
			    TR(TRACE_VIRTPUT, ("fill %d orphan cells", j));
			    fill_cells(win, j);
			    break;
			}
		    }
		    break;
		}
	    }
	     
	    for (i = 0; i < len; ++i) {
		NCURSES_CH_T value = ch;
		SetWidecExt(value, i);
		TR(TRACE_VIRTPUT, ("multicolumn %d:%d (%d,%d)",
				   i + 1, len,
				   win->_begy + y, win->_begx + x));
		line->text[x] = value;
		CHANGED_CELL(line, x);
		++x;
	    }
	    goto testwrapping;
	}
    });

     
    line->text[x++] = ch;
     
    if_WIDEC(
  testwrapping:
    );

    TR(TRACE_VIRTPUT, ("cell (%d, %d..%d) = %s",
		       win->_cury, win->_curx, x - 1,
		       _tracech_t(CHREF(line->text[win->_curx]))));

    if (x > win->_maxx) {
	return wrap_to_next_line(win);
    }
    win->_curx = (NCURSES_SIZE_T) x;
    return OK;
}

static NCURSES_INLINE int
waddch_nosync(WINDOW *win, const NCURSES_CH_T ch)
 
{
    NCURSES_SIZE_T x, y;
    chtype t = (chtype) CharOf(ch);
#if USE_WIDEC_SUPPORT || NCURSES_SP_FUNCS || USE_REENTRANT
    SCREEN *sp = _nc_screen_of(win);
#endif
    const char *s = NCURSES_SP_NAME(unctrl) (NCURSES_SP_ARGx t);
    int tabsize = 8;

     
    if ((AttrOf(ch) & A_ALTCHARSET)
	|| (
#if USE_WIDEC_SUPPORT
	       (sp != 0 && sp->_legacy_coding) &&
#endif
	       s[1] == 0
	)
	|| (
	       (isprint((int) t) && !iscntrl((int) t))
#if USE_WIDEC_SUPPORT
	       || ((sp == 0 || !sp->_legacy_coding) &&
		   (WINDOW_EXT(win, addch_used)
		    || !_nc_is_charable(CharOf(ch))))
#endif
	)) {
	return waddch_literal(win, ch);
    }

     
    x = win->_curx;
    y = win->_cury;
    CHECK_POSITION(win, x, y);

    switch (t) {
    case '\t':
#if USE_REENTRANT
	tabsize = *ptrTabsize(sp);
#else
	tabsize = TABSIZE;
#endif
	x = (NCURSES_SIZE_T) (x + (tabsize - (x % tabsize)));
	 
	if ((!win->_scroll && (y == win->_regbottom))
	    || (x <= win->_maxx)) {
	    NCURSES_CH_T blank = blankchar;
	    AddAttr(blank, AttrOf(ch));
	    while (win->_curx < x) {
		if (waddch_literal(win, blank) == ERR)
		    return (ERR);
	    }
	    break;
	} else {
	    wclrtoeol(win);
	    win->_flags |= _WRAPPED;
	    if (newline_forces_scroll(win, &y)) {
		x = win->_maxx;
		if (win->_scroll) {
		    scroll(win);
		    x = 0;
		}
	    } else {
		x = 0;
	    }
	}
	break;
    case '\n':
	wclrtoeol(win);
	if (newline_forces_scroll(win, &y)) {
	    if (win->_scroll)
		scroll(win);
	    else
		return (ERR);
	}
	 
    case '\r':
	x = 0;
	win->_flags &= ~_WRAPPED;
	break;
    case '\b':
	if (x == 0)
	    return (OK);
	x--;
	win->_flags &= ~_WRAPPED;
	break;
    default:
	while (*s) {
	    NCURSES_CH_T sch;
	    SetChar(sch, UChar(*s++), AttrOf(ch));
	    if_EXT_COLORS(SetPair(sch, GetPair(ch)));
	    if (waddch_literal(win, sch) == ERR)
		return ERR;
	}
	return (OK);
    }

    win->_curx = x;
    win->_cury = y;

    return (OK);
}

NCURSES_EXPORT(int)
_nc_waddch_nosync(WINDOW *win, const NCURSES_CH_T c)
 
{
    return (waddch_nosync(win, c));
}

 

 

NCURSES_EXPORT(int)
waddch(WINDOW *win, const chtype ch)
{
    int code = ERR;
    NCURSES_CH_T wch;
    SetChar2(wch, ch);

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_CALLED("waddch(%p, %s)"), (void *) win,
				      _tracechtype(ch)));

    if (win && (waddch_nosync(win, wch) != ERR)) {
	_nc_synchook(win);
	code = OK;
    }

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}

NCURSES_EXPORT(int)
wechochar(WINDOW *win, const chtype ch)
{
    int code = ERR;
    NCURSES_CH_T wch;
    SetChar2(wch, ch);

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_CALLED("wechochar(%p, %s)"),
				      (void *) win,
				      _tracechtype(ch)));

    if (win && (waddch_nosync(win, wch) != ERR)) {
	bool save_immed = win->_immed;
	win->_immed = TRUE;
	_nc_synchook(win);
	win->_immed = save_immed;
	code = OK;
    }
    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}
