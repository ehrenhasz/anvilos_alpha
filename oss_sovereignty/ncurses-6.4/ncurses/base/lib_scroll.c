 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_scroll.c,v 1.32 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(void)
_nc_scroll_window(WINDOW *win,
		  int const n,
		  int const top,
		  int const bottom,
		  NCURSES_CH_T blank)
{
    int limit;
    int line;
    int j;
    size_t to_copy = (sizeof(NCURSES_CH_T) * (size_t) (win->_maxx + 1));

    TR(TRACE_MOVE, ("_nc_scroll_window(%p, %d, %ld, %ld)",
		    (void *) win, n, (long) top, (long) bottom));

    if (top < 0
	|| bottom < top
	|| bottom > win->_maxy) {
	TR(TRACE_MOVE, ("nothing to scroll"));
	return;
    }

     
#define BottomLimit(n) ((n) >= 0 && (n) >= top)
#define TopLimit(n)    ((n) <= win->_maxy && (n) <= bottom)

     
    if (n < 0) {
	limit = top - n;
	for (line = bottom; line >= limit && BottomLimit(line); line--) {
	    TR(TRACE_MOVE, ("...copying %d to %d", line + n, line));
	    memcpy(win->_line[line].text,
		   win->_line[line + n].text,
		   to_copy);
	    if_USE_SCROLL_HINTS(win->_line[line].oldindex =
				win->_line[line + n].oldindex);
	}
	for (line = top; line < limit && TopLimit(line); line++) {
	    TR(TRACE_MOVE, ("...filling %d", line));
	    for (j = 0; j <= win->_maxx; j++)
		win->_line[line].text[j] = blank;
	    if_USE_SCROLL_HINTS(win->_line[line].oldindex = _NEWINDEX);
	}
    }

     
    if (n > 0) {
	limit = bottom - n;
	for (line = top; line <= limit && TopLimit(line); line++) {
	    memcpy(win->_line[line].text,
		   win->_line[line + n].text,
		   to_copy);
	    if_USE_SCROLL_HINTS(win->_line[line].oldindex =
				win->_line[line + n].oldindex);
	}
	for (line = bottom; line > limit && BottomLimit(line); line--) {
	    for (j = 0; j <= win->_maxx; j++)
		win->_line[line].text[j] = blank;
	    if_USE_SCROLL_HINTS(win->_line[line].oldindex = _NEWINDEX);
	}
    }
    touchline(win, top, bottom - top + 1);

    if_WIDEC({
	if (WINDOW_EXT(win, addch_used) != 0) {
	    int next = WINDOW_EXT(win, addch_y) + n;
	    if (next < 0 || next > win->_maxy) {
		TR(TRACE_VIRTPUT,
		   ("Alert discarded multibyte on scroll"));
		WINDOW_EXT(win, addch_y) = 0;
	    } else {
		TR(TRACE_VIRTPUT, ("scrolled working position to %d,%d",
				   WINDOW_EXT(win, addch_y),
				   WINDOW_EXT(win, addch_x)));
		WINDOW_EXT(win, addch_y) = next;
	    }
	}
    })
}

NCURSES_EXPORT(int)
wscrl(WINDOW *win, int n)
{
    T((T_CALLED("wscrl(%p,%d)"), (void *) win, n));

    if (!win || !win->_scroll) {
	TR(TRACE_MOVE, ("...scrollok is false"));
	returnCode(ERR);
    }

    if (n != 0) {
	_nc_scroll_window(win, n, win->_regtop, win->_regbottom, win->_nc_bkgd);
	_nc_synchook(win);
    }
    returnCode(OK);
}
