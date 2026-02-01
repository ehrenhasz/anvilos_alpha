 

 

#include <curses.priv.h>

MODULE_ID("$Id: wresize.c,v 1.42 2021/10/23 18:54:16 tom Exp $")

static int
cleanup_lines(struct ldat *data, int length)
{
    while (--length >= 0)
	FreeAndNull(data[length].text);
    free(data);
    return ERR;
}

 
static void
repair_subwindows(WINDOW *cmp)
{
    WINDOWLIST *wp;
    struct ldat *pline = cmp->_line;
    int row;
#ifdef USE_SP_WINDOWLIST
    SCREEN *sp = _nc_screen_of(cmp);
#endif

    _nc_lock_global(curses);

    for (each_window(SP_PARM, wp)) {
	WINDOW *tst = &(wp->win);

	if (tst->_parent == cmp) {

#define REPAIR1(field, limit) \
	    if (tst->field > cmp->limit) \
		tst->field = cmp->limit

	    REPAIR1(_pary, _maxy);
	    REPAIR1(_parx, _maxx);

#define REPAIR2(field, limit) \
	    if (tst->limit + tst->field > cmp->limit) \
		tst->limit = (NCURSES_SIZE_T) (cmp->limit - tst->field)

	    REPAIR2(_pary, _maxy);
	    REPAIR2(_parx, _maxx);

#define REPAIR3(field, limit) \
	    if (tst->field > tst->limit) \
		tst->field = tst->limit

	    REPAIR3(_cury, _maxy);
	    REPAIR3(_curx, _maxx);

	    REPAIR3(_regtop, _maxy);
	    REPAIR3(_regbottom, _maxy);

	    for (row = 0; row <= tst->_maxy; ++row) {
		tst->_line[row].text = &pline[tst->_pary + row].text[tst->_parx];
	    }
	    repair_subwindows(tst);
	}
    }
    _nc_unlock_global(curses);
}

 
NCURSES_EXPORT(int)
wresize(WINDOW *win, int ToLines, int ToCols)
{
    int col, row, size_x, size_y;
    struct ldat *pline;
    struct ldat *new_lines = 0;

#ifdef TRACE
    T((T_CALLED("wresize(%p,%d,%d)"), (void *) win, ToLines, ToCols));
    if (win) {
	TR(TRACE_UPDATE, ("...beg (%ld, %ld), max(%ld,%ld), reg(%ld,%ld)",
			  (long) win->_begy, (long) win->_begx,
			  (long) win->_maxy, (long) win->_maxx,
			  (long) win->_regtop, (long) win->_regbottom));
	if (USE_TRACEF(TRACE_UPDATE)) {
	    _tracedump("...before", win);
	    _nc_unlock_global(tracef);
	}
    }
#endif

    if (!win || --ToLines < 0 || --ToCols < 0)
	returnCode(ERR);

    size_x = win->_maxx;
    size_y = win->_maxy;

    if (ToLines == size_y
	&& ToCols == size_x)
	returnCode(OK);

    if (IS_SUBWIN(win)) {
	 
	if (win->_pary + ToLines > win->_parent->_maxy
	    || win->_parx + ToCols > win->_parent->_maxx) {
	    returnCode(ERR);
	}
	pline = win->_parent->_line;
    } else {
	pline = 0;
    }

     
    new_lines = typeCalloc(struct ldat, (unsigned) (ToLines + 1));
    if (new_lines == 0)
	returnCode(ERR);

     
    for (row = 0; row <= ToLines; ++row) {
	int begin = (row > size_y) ? 0 : (size_x + 1);
	int end = ToCols;
	NCURSES_CH_T *s;

	if (!IS_SUBWIN(win)) {
	    if (row <= size_y) {
		if (ToCols != size_x) {
		    s = typeMalloc(NCURSES_CH_T, (unsigned) ToCols + 1);
		    if (s == 0)
			returnCode(cleanup_lines(new_lines, row));
		    for (col = 0; col <= ToCols; ++col) {
			bool valid = (col <= size_x);
			if_WIDEC({
			    if (col == ToCols
				&& col < size_x
				&& isWidecBase(win->_line[row].text[col])) {
				valid = FALSE;
			    }
			});
			s[col] = (valid
				  ? win->_line[row].text[col]
				  : win->_nc_bkgd);
		    }
		} else {
		    s = win->_line[row].text;
		}
	    } else {
		s = typeMalloc(NCURSES_CH_T, (unsigned) ToCols + 1);
		if (s == 0)
		    returnCode(cleanup_lines(new_lines, row));
		for (col = 0; col <= ToCols; ++col)
		    s[col] = win->_nc_bkgd;
	    }
	} else if (pline != 0 && pline[win->_pary + row].text != 0) {
	    s = &pline[win->_pary + row].text[win->_parx];
	} else {
	    s = 0;
	}

	if_USE_SCROLL_HINTS(new_lines[row].oldindex = row);
	if (row <= size_y) {
	    new_lines[row].firstchar = win->_line[row].firstchar;
	    new_lines[row].lastchar = win->_line[row].lastchar;
	}
	if ((ToCols != size_x) || (row > size_y)) {
	    if (end >= begin) {	 
		if (new_lines[row].firstchar < begin)
		    new_lines[row].firstchar = (NCURSES_SIZE_T) begin;
	    } else {		 
		new_lines[row].firstchar = 0;
	    }
	    new_lines[row].lastchar = (NCURSES_SIZE_T) ToCols;
	}
	new_lines[row].text = s;
    }

     
    if (!(win->_flags & _SUBWIN)) {
	if (ToCols == size_x) {
	    for (row = ToLines + 1; row <= size_y; row++) {
		FreeAndNull(win->_line[row].text);
	    }
	} else {
	    for (row = 0; row <= size_y; row++) {
		FreeAndNull(win->_line[row].text);
	    }
	}
    }

    FreeAndNull(win->_line);
    win->_line = new_lines;

     
    win->_maxx = (NCURSES_SIZE_T) ToCols;
    win->_maxy = (NCURSES_SIZE_T) ToLines;

    if (win->_regtop > win->_maxy)
	win->_regtop = win->_maxy;
    if (win->_regbottom > win->_maxy
	|| win->_regbottom == size_y)
	win->_regbottom = win->_maxy;

    if (win->_curx > win->_maxx)
	win->_curx = win->_maxx;
    if (win->_cury > win->_maxy)
	win->_cury = win->_maxy;

     
    repair_subwindows(win);

#ifdef TRACE
    TR(TRACE_UPDATE, ("...beg (%ld, %ld), max(%ld,%ld), reg(%ld,%ld)",
		      (long) win->_begy, (long) win->_begx,
		      (long) win->_maxy, (long) win->_maxx,
		      (long) win->_regtop, (long) win->_regbottom));
    if (USE_TRACEF(TRACE_UPDATE)) {
	_tracedump("...after:", win);
	_nc_unlock_global(tracef);
    }
#endif
    returnCode(OK);
}
