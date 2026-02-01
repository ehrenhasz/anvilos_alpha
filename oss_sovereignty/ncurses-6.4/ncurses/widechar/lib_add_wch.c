 

 

#include <curses.priv.h>

#if HAVE_WCTYPE_H
#include <wctype.h>
#endif

MODULE_ID("$Id: lib_add_wch.c,v 1.17 2021/06/17 21:26:02 tom Exp $")

 
static const cchar_t blankchar = NewChar(BLANK_TEXT);

 

 
#define COLOR_MASK(ch) (~(attr_t)(((ch) & A_COLOR) ? A_COLOR : 0))

static NCURSES_INLINE cchar_t
render_char(WINDOW *win, cchar_t ch)
 
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

    if (*ypos >= win->_regtop && *ypos == win->_regbottom) {
	*ypos = win->_regbottom;
	result = TRUE;
    } else {
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

static int wadd_wch_literal(WINDOW *, cchar_t);
 
static void
fill_cells(WINDOW *win, int count)
{
    cchar_t blank = blankchar;
    int save_x = win->_curx;
    int save_y = win->_cury;

    while (count-- > 0) {
	if (wadd_wch_literal(win, blank) == ERR)
	    break;
    }
    win->_curx = (NCURSES_SIZE_T) save_x;
    win->_cury = (NCURSES_SIZE_T) save_y;
}

static int
wadd_wch_literal(WINDOW *win, cchar_t ch)
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

     
    {
	int len = _nc_wacs_width(CharOf(ch));
	int i;
	int j;
	wchar_t *chars;

	if (len == 0) {		 
	    if ((x > 0 && y >= 0)
		|| (win->_maxx >= 0 && win->_cury >= 1)) {
		if (x > 0 && y >= 0)
		    chars = (win->_line[y].text[x - 1].chars);
		else
		    chars = (win->_line[y - 1].text[win->_maxx].chars);
		for (i = 0; i < CCHARW_MAX; ++i) {
		    if (chars[i] == 0) {
			TR(TRACE_VIRTPUT,
			   ("added non-spacing %d: %x",
			    x, (int) CharOf(ch)));
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
		cchar_t value = ch;
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
    }

     
    line->text[x++] = ch;
     
  testwrapping:

    TR(TRACE_VIRTPUT, ("cell (%ld, %ld..%d) = %s",
		       (long) win->_cury, (long) win->_curx, x - 1,
		       _tracech_t(CHREF(ch))));

    if (x > win->_maxx) {
	return wrap_to_next_line(win);
    }
    win->_curx = (NCURSES_SIZE_T) x;
    return OK;
}

static NCURSES_INLINE int
wadd_wch_nosync(WINDOW *win, cchar_t ch)
 
{
    NCURSES_SIZE_T x, y;
    wchar_t *s;
    int tabsize = 8;
#if USE_REENTRANT
    SCREEN *sp = _nc_screen_of(win);
#endif

     
    if ((AttrOf(ch) & A_ALTCHARSET)
	|| iswprint((wint_t) CharOf(ch)))
	return wadd_wch_literal(win, ch);

     
    x = win->_curx;
    y = win->_cury;

    switch (CharOf(ch)) {
    case '\t':
#if USE_REENTRANT
	tabsize = *ptrTabsize(sp);
#else
	tabsize = TABSIZE;
#endif
	x = (NCURSES_SIZE_T) (x + (tabsize - (x % tabsize)));
	 
	if ((!win->_scroll && (y == win->_regbottom))
	    || (x <= win->_maxx)) {
	    cchar_t blank = blankchar;
	    AddAttr(blank, AttrOf(ch));
	    while (win->_curx < x) {
		if (wadd_wch_literal(win, blank) == ERR)
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
	if ((s = wunctrl(&ch)) != 0) {
	    while (*s) {
		cchar_t sch;
		SetChar(sch, *s++, AttrOf(ch));
		if_EXT_COLORS(SetPair(sch, GetPair(ch)));
		if (wadd_wch_literal(win, sch) == ERR)
		    return ERR;
	    }
	    return OK;
	}
	return ERR;
    }

    win->_curx = x;
    win->_cury = y;

    return OK;
}

 

 

NCURSES_EXPORT(int)
wadd_wch(WINDOW *win, const cchar_t *wch)
{
    int code = ERR;

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_CALLED("wadd_wch(%p, %s)"),
				      (void *) win,
				      _tracecchar_t(wch)));

    if (win && (wadd_wch_nosync(win, *wch) != ERR)) {
	_nc_synchook(win);
	code = OK;
    }

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}

NCURSES_EXPORT(int)
wecho_wchar(WINDOW *win, const cchar_t *wch)
{
    int code = ERR;

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_CALLED("wechochar(%p, %s)"),
				      (void *) win,
				      _tracecchar_t(wch)));

    if (win && (wadd_wch_nosync(win, *wch) != ERR)) {
	bool save_immed = win->_immed;
	win->_immed = TRUE;
	_nc_synchook(win);
	win->_immed = save_immed;
	code = OK;
    }
    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}
