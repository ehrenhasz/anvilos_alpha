 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_refresh.c,v 1.47 2021/11/06 22:22:03 tom Exp $")

NCURSES_EXPORT(int)
wrefresh(WINDOW *win)
{
    int code;
#if NCURSES_SP_FUNCS
    SCREEN *SP_PARM = _nc_screen_of(win);
#endif

    T((T_CALLED("wrefresh(%p)"), (void *) win));

    if (win == 0) {
	code = ERR;
    } else if (win == CurScreen(SP_PARM)) {
	CurScreen(SP_PARM)->_clear = TRUE;
	code = NCURSES_SP_NAME(doupdate) (NCURSES_SP_ARG);
    } else if ((code = wnoutrefresh(win)) == OK) {
	if (win->_clear)
	    NewScreen(SP_PARM)->_clear = TRUE;
	code = NCURSES_SP_NAME(doupdate) (NCURSES_SP_ARG);
	 
	win->_clear = FALSE;
    }
    returnCode(code);
}

NCURSES_EXPORT(int)
wnoutrefresh(WINDOW *win)
{
    int limit_x;
    int src_row, src_col;
    int begx;
    int begy;
    int dst_row, dst_col;
#if USE_SCROLL_HINTS
    bool wide;
#endif
#if NCURSES_SP_FUNCS
    SCREEN *SP_PARM = _nc_screen_of(win);
#endif

    T((T_CALLED("wnoutrefresh(%p)"), (void *) win));

    if (win == NULL)
	returnCode(ERR);

     
    if (IS_PAD(win)) {
	returnCode(pnoutrefresh(win,
				win->_pad._pad_y,
				win->_pad._pad_x,
				win->_pad._pad_top,
				win->_pad._pad_left,
				win->_pad._pad_bottom,
				win->_pad._pad_right));
    }
#ifdef TRACE
    if (USE_TRACEF(TRACE_UPDATE)) {
	_tracedump("...win", win);
	_nc_unlock_global(tracef);
    }
#endif  

     
    begx = win->_begx;
    begy = win->_begy;

    NewScreen(SP_PARM)->_nc_bkgd = win->_nc_bkgd;
    WINDOW_ATTRS(NewScreen(SP_PARM)) = WINDOW_ATTRS(win);

     
    wsyncdown(win);

#if USE_SCROLL_HINTS
     
    wide = (begx <= 1 && win->_maxx >= (NewScreen(SP_PARM)->_maxx - 1));
#endif

    win->_flags &= ~_HASMOVED;

     

     
    limit_x = win->_maxx;
     
    if (limit_x > NewScreen(SP_PARM)->_maxx - begx)
	limit_x = NewScreen(SP_PARM)->_maxx - begx;

    for (src_row = 0, dst_row = begy + win->_yoffset;
	 src_row <= win->_maxy && dst_row <= NewScreen(SP_PARM)->_maxy;
	 src_row++, dst_row++) {
	struct ldat *nline = &(NewScreen(SP_PARM)->_line[dst_row]);
	struct ldat *oline = &win->_line[src_row];

	if (oline->firstchar != _NOCHANGE) {
	    int last_src = oline->lastchar;

	    if (last_src > limit_x)
		last_src = limit_x;

	    src_col = oline->firstchar;
	    dst_col = src_col + begx;

	    if_WIDEC({
		int j;

		 
		if (isWidecExt(oline->text[src_col])) {
		    j = 1 + dst_col - WidecExt(oline->text[src_col]);
		    if (j < 0)
			j = 0;
		    if (dst_col > j) {
			src_col -= (dst_col - j);
			dst_col = j;
		    }
		}

		 
		j = last_src;
		if (WidecExt(oline->text[j])) {
		    ++j;
		    while (j <= limit_x) {
			if (isWidecBase(oline->text[j])) {
			    break;
			} else {
			    last_src = j;
			}
			++j;
		    }
		}
	    });

	    if_WIDEC({
		static cchar_t blank = BLANK;
		int last_dst = begx + ((last_src < win->_maxx)
				       ? last_src
				       : win->_maxx);
		int fix_left = dst_col;
		int fix_right = last_dst;
		int j;

		 
		j = dst_col;
		if (isWidecExt(nline->text[j])) {
		     
		    fix_left = 1 + j - WidecExt(nline->text[j]);
		    if (fix_left < 0)
			fix_left = 0;	 
		}

		j = last_dst;
		if (WidecExt(nline->text[j]) != 0) {
		     
		    ++j;
		    while (j <= NewScreen(SP_PARM)->_maxx &&
			   isWidecExt(nline->text[j])) {
			fix_right = j++;
		    }
		}

		 
		if (fix_left < dst_col || fix_right > last_dst) {
		    for (j = fix_left; j <= fix_right; ++j) {
			nline->text[j] = blank;
			CHANGED_CELL(nline, j);
		    }
		}
	    });

	     
	    for (; src_col <= last_src; src_col++, dst_col++) {
		if (!CharEq(oline->text[src_col], nline->text[dst_col])) {
		    nline->text[dst_col] = oline->text[src_col];
		    CHANGED_CELL(nline, dst_col);
		}
	    }

	}
#if USE_SCROLL_HINTS
	if (wide) {
	    int oind = oline->oldindex;

	    nline->oldindex = ((oind == _NEWINDEX)
			       ? _NEWINDEX
			       : (begy + oind + win->_yoffset));
	}
#endif  

	oline->firstchar = oline->lastchar = _NOCHANGE;
	if_USE_SCROLL_HINTS(oline->oldindex = src_row);
    }

    if (win->_clear) {
	win->_clear = FALSE;
	NewScreen(SP_PARM)->_clear = TRUE;
    }

    if (!win->_leaveok) {
	NewScreen(SP_PARM)->_cury = (NCURSES_SIZE_T) (win->_cury +
						      win->_begy + win->_yoffset);
	NewScreen(SP_PARM)->_curx = (NCURSES_SIZE_T) (win->_curx + win->_begx);
    }
    NewScreen(SP_PARM)->_leaveok = win->_leaveok;

#ifdef TRACE
    if (USE_TRACEF(TRACE_UPDATE)) {
	_tracedump("newscr", NewScreen(SP_PARM));
	_nc_unlock_global(tracef);
    }
#endif  
    returnCode(OK);
}
