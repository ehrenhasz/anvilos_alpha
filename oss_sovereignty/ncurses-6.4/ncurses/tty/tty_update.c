 

 

 

#define NEW_PAIR_INTERNAL 1

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

#if defined __HAIKU__ && defined __BEOS__
#undef __BEOS__
#endif

#ifdef __BEOS__
#undef false
#undef true
#include <OS.h>
#endif

#if defined(TRACE) && HAVE_SYS_TIMES_H && HAVE_TIMES
#define USE_TRACE_TIMES 1
#else
#define USE_TRACE_TIMES 0
#endif

#if HAVE_SYS_TIME_H && HAVE_SYS_TIME_SELECT
#include <sys/time.h>
#endif

#if USE_TRACE_TIMES
#include <sys/times.h>
#endif

#if USE_FUNC_POLL
#elif HAVE_SELECT
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif

#include <ctype.h>

MODULE_ID("$Id: tty_update.c,v 1.314 2022/07/23 22:12:59 tom Exp $")

 
#define CHECK_INTERVAL	5

#define FILL_BCE(sp) (sp->_coloron && !sp->_default_color && !back_color_erase)

static const NCURSES_CH_T blankchar = NewChar(BLANK_TEXT);
static NCURSES_CH_T normal = NewChar(BLANK_TEXT);

 
 

static NCURSES_INLINE NCURSES_CH_T ClrBlank(NCURSES_SP_DCLx WINDOW *win);

#if NCURSES_SP_FUNCS
static int ClrBottom(SCREEN *, int total);
static void ClearScreen(SCREEN *, NCURSES_CH_T blank);
static void ClrUpdate(SCREEN *);
static void DelChar(SCREEN *, int count);
static void InsStr(SCREEN *, NCURSES_CH_T *line, int count);
static void TransformLine(SCREEN *, int const lineno);
#else
static int ClrBottom(int total);
static void ClearScreen(NCURSES_CH_T blank);
static void ClrUpdate(void);
static void DelChar(int count);
static void InsStr(NCURSES_CH_T *line, int count);
static void TransformLine(int const lineno);
#endif

#ifdef POSITION_DEBUG
 

static void
position_check(NCURSES_SP_DCLx int expected_y, int expected_x, const char *legend)
 
{
    char buf[20];
    char *s;
    int y, x;

    if (!_nc_tracing || (expected_y < 0 && expected_x < 0))
	return;

    NCURSES_SP_NAME(_nc_flush) (NCURSES_SP_ARG);
    memset(buf, '\0', sizeof(buf));
    NCURSES_PUTP2_FLUSH("cpr", "\033[6n");	 
    *(s = buf) = 0;
    do {
	int ask = sizeof(buf) - 1 - (s - buf);
	int got = read(0, s, ask);
	if (got == 0)
	    break;
	s += got;
    } while (strchr(buf, 'R') == 0);
    _tracef("probe returned %s", _nc_visbuf(buf));

     
    if (sscanf(buf, "\033[%d;%dR", &y, &x) != 2) {
	_tracef("position probe failed in %s", legend);
    } else {
	if (expected_x < 0)
	    expected_x = x - 1;
	if (expected_y < 0)
	    expected_y = y - 1;
	if (y - 1 != expected_y || x - 1 != expected_x) {
	    NCURSES_SP_NAME(beep) (NCURSES_SP_ARG);
	    NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				    TIPARM_2("\033[%d;%dH",
					     expected_y + 1,
					     expected_x + 1),
				    1, NCURSES_SP_NAME(_nc_outch));
	    _tracef("position seen (%d, %d) doesn't match expected one (%d, %d) in %s",
		    y - 1, x - 1, expected_y, expected_x, legend);
	} else {
	    _tracef("position matches OK in %s", legend);
	}
    }
}
#else
#define position_check(expected_y, expected_x, legend)	 
#endif  

 

static NCURSES_INLINE void
GoTo(NCURSES_SP_DCLx int const row, int const col)
{
    TR(TRACE_MOVE, ("GoTo(%p, %d, %d) from (%d, %d)",
		    (void *) SP_PARM, row, col, SP_PARM->_cursrow, SP_PARM->_curscol));

    position_check(NCURSES_SP_ARGx
		   SP_PARM->_cursrow,
		   SP_PARM->_curscol, "GoTo");

    TINFO_MVCUR(NCURSES_SP_ARGx
		SP_PARM->_cursrow,
		SP_PARM->_curscol,
		row, col);
    position_check(NCURSES_SP_ARGx
		   SP_PARM->_cursrow,
		   SP_PARM->_curscol, "GoTo2");
}

#if !NCURSES_WCWIDTH_GRAPHICS
#define is_wacs_value(ch) (_nc_wacs_width(ch) == 1 && wcwidth(ch) > 1)
#endif  

static NCURSES_INLINE void
PutAttrChar(NCURSES_SP_DCLx CARG_CH_T ch)
{
    int chlen = 1;
    NCURSES_CH_T my_ch;
#if USE_WIDEC_SUPPORT
    PUTC_DATA;
#endif
    NCURSES_CH_T tilde;
    NCURSES_CH_T attr = CHDEREF(ch);

    TR(TRACE_CHARPUT, ("PutAttrChar(%s) at (%d, %d)",
		       _tracech_t(ch),
		       SP_PARM->_cursrow, SP_PARM->_curscol));
#if USE_WIDEC_SUPPORT
     
    if (isWidecExt(CHDEREF(ch))) {
	TR(TRACE_CHARPUT, ("...skip"));
	return;
    }
     
    if ((chlen = _nc_wacs_width(CharOf(CHDEREF(ch)))) <= 0) {
	static const NCURSES_CH_T blank = NewChar(BLANK_TEXT);

	 
	if (is8bits(CharOf(CHDEREF(ch)))
	    && (isprint(CharOf(CHDEREF(ch)))
		|| (SP_PARM->_legacy_coding > 0 && CharOf(CHDEREF(ch)) >= 160)
		|| (SP_PARM->_legacy_coding > 1 && CharOf(CHDEREF(ch)) >= 128)
		|| (AttrOf(attr) & A_ALTCHARSET
		    && ((CharOfD(ch) < ACS_LEN
			 && SP_PARM->_acs_map != 0
			 && SP_PARM->_acs_map[CharOfD(ch)] != 0)
			|| (CharOfD(ch) >= 128))))) {
	    ;
	} else {
	    ch = CHREF(blank);
	    TR(TRACE_CHARPUT, ("forced to blank"));
	}
	chlen = 1;
    }
#endif

    if ((AttrOf(attr) & A_ALTCHARSET)
	&& SP_PARM->_acs_map != 0
	&& ((CharOfD(ch) < ACS_LEN)
#if !NCURSES_WCWIDTH_GRAPHICS
	    || is_wacs_value(CharOfD(ch))
#endif
	)) {
	int c8;
	my_ch = CHDEREF(ch);	 
	c8 = CharOf(my_ch);
#if USE_WIDEC_SUPPORT
	 
	if (SP_PARM->_screen_unicode
	    && _nc_wacs[CharOf(my_ch)].chars[0]) {
	    if (SP_PARM->_screen_acs_map[CharOf(my_ch)]) {
		if (SP_PARM->_screen_acs_fix) {
		    RemAttr(attr, A_ALTCHARSET);
		    my_ch = _nc_wacs[CharOf(my_ch)];
		}
	    } else {
		RemAttr(attr, A_ALTCHARSET);
		my_ch = _nc_wacs[CharOf(my_ch)];
	    }
#if !NCURSES_WCWIDTH_GRAPHICS
	    if (!(AttrOf(attr) & A_ALTCHARSET)) {
		chlen = 1;
	    }
#endif  
	} else
#endif
	if (!SP_PARM->_screen_acs_map[c8]) {
	     
	    chtype temp = UChar(SP_PARM->_acs_map[c8]);
	    if (temp) {
		RemAttr(attr, A_ALTCHARSET);
		SetChar(my_ch, temp, AttrOf(attr));
	    }
	}

	 
	if (AttrOf(attr) & A_ALTCHARSET) {
	    int j = CharOfD(ch);
	    chtype temp = UChar(SP_PARM->_acs_map[j]);

	    if (temp != 0) {
		SetChar(my_ch, temp, AttrOf(attr));
	    } else {
		my_ch = CHDEREF(ch);
		RemAttr(attr, A_ALTCHARSET);
	    }
	}
	ch = CHREF(my_ch);
    }
#if USE_WIDEC_SUPPORT && !NCURSES_WCWIDTH_GRAPHICS
    else if (chlen > 1 && is_wacs_value(CharOfD(ch))) {
	chlen = 1;
    }
#endif
    if (tilde_glitch && (CharOfD(ch) == L('~'))) {
	SetChar(tilde, L('`'), AttrOf(attr));
	ch = CHREF(tilde);
    }

    UpdateAttrs(SP_PARM, attr);
    PUTC(CHDEREF(ch));
#if !USE_WIDEC_SUPPORT
    COUNT_OUTCHARS(1);
#endif
    SP_PARM->_curscol += chlen;
    if (char_padding) {
	NCURSES_PUTP2("char_padding", char_padding);
    }
}

static bool
check_pending(NCURSES_SP_DCL0)
 
{
    bool have_pending = FALSE;

     
    if (SP_PARM->_fifohold != 0)
	return FALSE;

    if (SP_PARM->_checkfd >= 0) {
#if USE_FUNC_POLL
	struct pollfd fds[1];
	fds[0].fd = SP_PARM->_checkfd;
	fds[0].events = POLLIN;
	if (poll(fds, (size_t) 1, 0) > 0) {
	    have_pending = TRUE;
	}
#elif defined(__BEOS__)
	 
	int n = 0;
	int howmany = ioctl(0, 'ichr', &n);
	if (howmany >= 0 && n > 0) {
	    have_pending = TRUE;
	}
#elif HAVE_SELECT
	fd_set fdset;
	struct timeval ktimeout;

	ktimeout.tv_sec =
	    ktimeout.tv_usec = 0;

	FD_ZERO(&fdset);
	FD_SET(SP_PARM->_checkfd, &fdset);
	if (select(SP_PARM->_checkfd + 1, &fdset, NULL, NULL, &ktimeout) != 0) {
	    have_pending = TRUE;
	}
#endif
    }
    if (have_pending) {
	SP_PARM->_fifohold = 5;
	NCURSES_SP_NAME(_nc_flush) (NCURSES_SP_ARG);
    }
    return FALSE;
}

 
static void
PutCharLR(NCURSES_SP_DCLx const ARG_CH_T ch)
{
    if (!auto_right_margin) {
	 
	PutAttrChar(NCURSES_SP_ARGx ch);
    } else if (enter_am_mode && exit_am_mode) {
	int oldcol = SP_PARM->_curscol;
	 
	NCURSES_PUTP2("exit_am_mode", exit_am_mode);

	PutAttrChar(NCURSES_SP_ARGx ch);
	SP_PARM->_curscol = oldcol;
	position_check(NCURSES_SP_ARGx
		       SP_PARM->_cursrow,
		       SP_PARM->_curscol,
		       "exit_am_mode");

	NCURSES_PUTP2("enter_am_mode", enter_am_mode);
    } else if ((enter_insert_mode && exit_insert_mode)
	       || insert_character || parm_ich) {
	GoTo(NCURSES_SP_ARGx
	     screen_lines(SP_PARM) - 1,
	     screen_columns(SP_PARM) - 2);
	PutAttrChar(NCURSES_SP_ARGx ch);
	GoTo(NCURSES_SP_ARGx
	     screen_lines(SP_PARM) - 1,
	     screen_columns(SP_PARM) - 2);
	InsStr(NCURSES_SP_ARGx
	       NewScreen(SP_PARM)->_line[screen_lines(SP_PARM) - 1].text +
	       screen_columns(SP_PARM) - 2, 1);
    }
}

 
static void
wrap_cursor(NCURSES_SP_DCL0)
{
    if (eat_newline_glitch) {
	 
	SP_PARM->_curscol = -1;
	SP_PARM->_cursrow = -1;
    } else if (auto_right_margin) {
	SP_PARM->_curscol = 0;
	SP_PARM->_cursrow++;
	 
	if (!move_standout_mode && AttrOf(SCREEN_ATTRS(SP_PARM))) {
	    TR(TRACE_CHARPUT, ("turning off (%#lx) %s before wrapping",
			       (unsigned long) AttrOf(SCREEN_ATTRS(SP_PARM)),
			       _traceattr(AttrOf(SCREEN_ATTRS(SP_PARM)))));
	    VIDPUTS(SP_PARM, A_NORMAL, 0);
	}
    } else {
	SP_PARM->_curscol--;
    }
    position_check(NCURSES_SP_ARGx
		   SP_PARM->_cursrow,
		   SP_PARM->_curscol,
		   "wrap_cursor");
}

static NCURSES_INLINE void
PutChar(NCURSES_SP_DCLx const ARG_CH_T ch)
 
{
    if (SP_PARM->_cursrow == screen_lines(SP_PARM) - 1 &&
	SP_PARM->_curscol == screen_columns(SP_PARM) - 1) {
	PutCharLR(NCURSES_SP_ARGx ch);
    } else {
	PutAttrChar(NCURSES_SP_ARGx ch);
    }

    if (SP_PARM->_curscol >= screen_columns(SP_PARM))
	wrap_cursor(NCURSES_SP_ARG);

    position_check(NCURSES_SP_ARGx
		   SP_PARM->_cursrow,
		   SP_PARM->_curscol, "PutChar");
}

 
static NCURSES_INLINE bool
can_clear_with(NCURSES_SP_DCLx ARG_CH_T ch)
{
    if (!back_color_erase && SP_PARM->_coloron) {
#if NCURSES_EXT_FUNCS
	int pair;

	if (!SP_PARM->_default_color)
	    return FALSE;
	if (!(isDefaultColor(SP_PARM->_default_fg) &&
	      isDefaultColor(SP_PARM->_default_bg)))
	    return FALSE;
	if ((pair = GetPair(CHDEREF(ch))) != 0) {
	    NCURSES_COLOR_T fg, bg;
	    if (NCURSES_SP_NAME(pair_content) (NCURSES_SP_ARGx
					       (short) pair,
					       &fg, &bg) == ERR
		|| !(isDefaultColor(fg) && isDefaultColor(bg))) {
		return FALSE;
	    }
	}
#else
	if (AttrOfD(ch) & A_COLOR)
	    return FALSE;
#endif
    }
    return (ISBLANK(CHDEREF(ch)) &&
	    (AttrOfD(ch) & ~(NONBLANK_ATTR | A_COLOR)) == BLANK_ATTR);
}

 
static int
EmitRange(NCURSES_SP_DCLx const NCURSES_CH_T *ntext, int num)
{
    int i;

    TR(TRACE_CHARPUT, ("EmitRange %d:%s", num, _nc_viscbuf(ntext, num)));

    if (erase_chars || repeat_char) {
	while (num > 0) {
	    int runcount;
	    NCURSES_CH_T ntext0;

	    while (num > 1 && !CharEq(ntext[0], ntext[1])) {
		PutChar(NCURSES_SP_ARGx CHREF(ntext[0]));
		ntext++;
		num--;
	    }
	    ntext0 = ntext[0];
	    if (num == 1) {
		PutChar(NCURSES_SP_ARGx CHREF(ntext0));
		return 0;
	    }
	    runcount = 2;

	    while (runcount < num && CharEq(ntext[runcount], ntext0))
		runcount++;

	     
	    if (erase_chars
		&& runcount > SP_PARM->_ech_cost + SP_PARM->_cup_ch_cost
		&& can_clear_with(NCURSES_SP_ARGx CHREF(ntext0))) {
		UpdateAttrs(SP_PARM, ntext0);
		NCURSES_PUTP2("erase_chars", TIPARM_1(erase_chars, runcount));

		 
		if (runcount < num) {
		    GoTo(NCURSES_SP_ARGx
			 SP_PARM->_cursrow,
			 SP_PARM->_curscol + runcount);
		} else {
		    return 1;	 
		}
	    } else if (repeat_char != 0 &&
#if BSD_TPUTS
		       !isdigit(UChar(CharOf(ntext0))) &&
#endif
#if USE_WIDEC_SUPPORT
		       (!SP_PARM->_screen_unicode &&
			(CharOf(ntext0) < ((AttrOf(ntext0) & A_ALTCHARSET)
					   ? ACS_LEN
					   : 256))) &&
#endif
		       runcount > SP_PARM->_rep_cost) {
		NCURSES_CH_T temp;
		bool wrap_possible = (SP_PARM->_curscol + runcount >=
				      screen_columns(SP_PARM));
		int rep_count = runcount;

		if (wrap_possible)
		    rep_count--;

		UpdateAttrs(SP_PARM, ntext0);
		temp = ntext0;
		if ((AttrOf(temp) & A_ALTCHARSET) &&
		    SP_PARM->_acs_map != 0 &&
		    (SP_PARM->_acs_map[CharOf(temp)] & A_CHARTEXT) != 0) {
		    SetChar(temp,
			    (SP_PARM->_acs_map[CharOf(ntext0)] & A_CHARTEXT),
			    AttrOf(ntext0) | A_ALTCHARSET);
		}
		NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
					TIPARM_2(repeat_char,
						 CharOf(temp),
						 rep_count),
					1,
					NCURSES_SP_NAME(_nc_outch));
		SP_PARM->_curscol += rep_count;

		if (wrap_possible)
		    PutChar(NCURSES_SP_ARGx CHREF(ntext0));
	    } else {
		for (i = 0; i < runcount; i++)
		    PutChar(NCURSES_SP_ARGx CHREF(ntext[i]));
	    }
	    ntext += runcount;
	    num -= runcount;
	}
	return 0;
    }

    for (i = 0; i < num; i++)
	PutChar(NCURSES_SP_ARGx CHREF(ntext[i]));
    return 0;
}

 
static int
PutRange(NCURSES_SP_DCLx
	 const NCURSES_CH_T *otext,
	 const NCURSES_CH_T *ntext,
	 int row,
	 int first, int last)
{
    int rc;

    TR(TRACE_CHARPUT, ("PutRange(%p, %p, %p, %d, %d, %d)",
		       (void *) SP_PARM,
		       (const void *) otext,
		       (const void *) ntext,
		       row, first, last));

    if (otext != ntext
	&& (last - first + 1) > SP_PARM->_inline_cost) {
	int i, j, same;

	for (j = first, same = 0; j <= last; j++) {
	    if (!same && isWidecExt(otext[j]))
		continue;
	    if (CharEq(otext[j], ntext[j])) {
		same++;
	    } else {
		if (same > SP_PARM->_inline_cost) {
		    EmitRange(NCURSES_SP_ARGx ntext + first, j - same - first);
		    GoTo(NCURSES_SP_ARGx row, first = j);
		}
		same = 0;
	    }
	}
	i = EmitRange(NCURSES_SP_ARGx ntext + first, j - same - first);
	 
	rc = (same == 0 ? i : 1);
    } else {
	rc = EmitRange(NCURSES_SP_ARGx ntext + first, last - first + 1);
    }
    return rc;
}

 
#define MARK_NOCHANGE(win,row) \
		win->_line[row].firstchar = _NOCHANGE; \
		win->_line[row].lastchar = _NOCHANGE; \
		if_USE_SCROLL_HINTS(win->_line[row].oldindex = row)

NCURSES_EXPORT(int)
TINFO_DOUPDATE(NCURSES_SP_DCL0)
{
    int i;
    int nonempty;
#if USE_TRACE_TIMES
    struct tms before, after;
#endif  

    T((T_CALLED("_nc_tinfo:doupdate(%p)"), (void *) SP_PARM));

    _nc_lock_global(update);

    if (SP_PARM == 0) {
	_nc_unlock_global(update);
	returnCode(ERR);
    }
#if !USE_REENTRANT
     
#if NCURSES_SP_FUNCS
    if (SP_PARM == CURRENT_SCREEN) {
#endif
#define SyncScreens(internal,exported) \
	if (internal == 0) internal = exported; \
	if (internal != exported) exported = internal

	SyncScreens(CurScreen(SP_PARM), curscr);
	SyncScreens(NewScreen(SP_PARM), newscr);
	SyncScreens(StdScreen(SP_PARM), stdscr);
#if NCURSES_SP_FUNCS
    }
#endif
#endif  

    if (CurScreen(SP_PARM) == 0
	|| NewScreen(SP_PARM) == 0
	|| StdScreen(SP_PARM) == 0) {
	_nc_unlock_global(update);
	returnCode(ERR);
    }
#ifdef TRACE
    if (USE_TRACEF(TRACE_UPDATE)) {
	if (CurScreen(SP_PARM)->_clear)
	    _tracef("curscr is clear");
	else
	    _tracedump("curscr", CurScreen(SP_PARM));
	_tracedump("newscr", NewScreen(SP_PARM));
	_nc_unlock_global(tracef);
    }
#endif  

    _nc_signal_handler(FALSE);

    if (SP_PARM->_fifohold)
	SP_PARM->_fifohold--;

#if USE_SIZECHANGE
    if ((SP_PARM->_endwin == ewSuspend)
	|| _nc_handle_sigwinch(SP_PARM)) {
	 
	_nc_update_screensize(SP_PARM);
    }
#endif

    if (SP_PARM->_endwin == ewSuspend) {

	T(("coming back from shell mode"));
	NCURSES_SP_NAME(reset_prog_mode) (NCURSES_SP_ARG);

	NCURSES_SP_NAME(_nc_mvcur_resume) (NCURSES_SP_ARG);
	NCURSES_SP_NAME(_nc_screen_resume) (NCURSES_SP_ARG);
	SP_PARM->_mouse_resume(SP_PARM);

	SP_PARM->_endwin = ewRunning;
    }
#if USE_TRACE_TIMES
     
    RESET_OUTCHARS();
    (void) times(&before);
#endif  

     
#if USE_XMC_SUPPORT
    if (magic_cookie_glitch > 0) {
	int j, k;
	attr_t rattr = A_NORMAL;

	for (i = 0; i < screen_lines(SP_PARM); i++) {
	    for (j = 0; j < screen_columns(SP_PARM); j++) {
		bool failed = FALSE;
		NCURSES_CH_T *thisline = NewScreen(SP_PARM)->_line[i].text;
		attr_t thisattr = AttrOf(thisline[j]) & SP_PARM->_xmc_triggers;
		attr_t turnon = thisattr & ~rattr;

		 
		if (turnon == 0) {
		    rattr = thisattr;
		    continue;
		}

		TR(TRACE_ATTRS, ("At (%d, %d): from %s...", i, j, _traceattr(rattr)));
		TR(TRACE_ATTRS, ("...to %s", _traceattr(turnon)));

		 
#define SAFE(scr,a)	(!((a) & (scr)->_xmc_triggers))
		if (ISBLANK(thisline[j]) && SAFE(SP_PARM, turnon)) {
		    RemAttr(thisline[j], turnon);
		    continue;
		}

		 
		for (k = 1; k <= magic_cookie_glitch; k++) {
		    if (j - k < 0
			|| !ISBLANK(thisline[j - k])
			|| !SAFE(SP_PARM, AttrOf(thisline[j - k]))) {
			failed = TRUE;
			TR(TRACE_ATTRS, ("No room at start in %d,%d%s%s",
					 i, j - k,
					 (ISBLANK(thisline[j - k])
					  ? ""
					  : ":nonblank"),
					 (SAFE(SP_PARM, AttrOf(thisline[j - k]))
					  ? ""
					  : ":unsafe")));
			break;
		    }
		}
		if (!failed) {
		    bool end_onscreen = FALSE;
		    int m, n = j;

		     
		    for (m = i; m < screen_lines(SP_PARM); m++) {
			for (; n < screen_columns(SP_PARM); n++) {
			    attr_t testattr =
			    AttrOf(NewScreen(SP_PARM)->_line[m].text[n]);
			    if ((testattr & SP_PARM->_xmc_triggers) == rattr) {
				end_onscreen = TRUE;
				TR(TRACE_ATTRS,
				   ("Range attributed with %s ends at (%d, %d)",
				    _traceattr(turnon), m, n));
				goto foundit;
			    }
			}
			n = 0;
		    }
		    TR(TRACE_ATTRS,
		       ("Range attributed with %s ends offscreen",
			_traceattr(turnon)));
		  foundit:;

		    if (end_onscreen) {
			NCURSES_CH_T *lastline =
			NewScreen(SP_PARM)->_line[m].text;

			 
			while (n >= 0
			       && ISBLANK(lastline[n])
			       && SAFE(SP_PARM, AttrOf(lastline[n]))) {
			    RemAttr(lastline[n--], turnon);
			}

			 
			for (k = 1; k <= magic_cookie_glitch; k++) {
			    if (n + k >= screen_columns(SP_PARM)
				|| !ISBLANK(lastline[n + k])
				|| !SAFE(SP_PARM, AttrOf(lastline[n + k]))) {
				failed = TRUE;
				TR(TRACE_ATTRS,
				   ("No room at end in %d,%d%s%s",
				    i, j - k,
				    (ISBLANK(lastline[n + k])
				     ? ""
				     : ":nonblank"),
				    (SAFE(SP_PARM, AttrOf(lastline[n + k]))
				     ? ""
				     : ":unsafe")));
				break;
			    }
			}
		    }
		}

		if (failed) {
		    int p, q = j;

		    TR(TRACE_ATTRS,
		       ("Clearing %s beginning at (%d, %d)",
			_traceattr(turnon), i, j));

		     
		    for (p = i; p < screen_lines(SP_PARM); p++) {
			for (; q < screen_columns(SP_PARM); q++) {
			    attr_t testattr = AttrOf(newscr->_line[p].text[q]);
			    if ((testattr & SP_PARM->_xmc_triggers) == rattr)
				goto foundend;
			    RemAttr(NewScreen(SP_PARM)->_line[p].text[q], turnon);
			}
			q = 0;
		    }
		  foundend:;
		} else {
		    TR(TRACE_ATTRS,
		       ("Cookie space for %s found before (%d, %d)",
			_traceattr(turnon), i, j));

		     
		    for (k = 1; k <= magic_cookie_glitch; k++)
			AddAttr(thisline[j - k], turnon);
		}

		rattr = thisattr;
	    }
	}

#ifdef TRACE
	 
	if (USE_TRACEF(TRACE_UPDATE)) {
	    _tracef("After magic-cookie check...");
	    _tracedump("newscr", NewScreen(SP_PARM));
	    _nc_unlock_global(tracef);
	}
#endif  
    }
#endif  

    nonempty = 0;
    if (CurScreen(SP_PARM)->_clear || NewScreen(SP_PARM)->_clear) {	 
	ClrUpdate(NCURSES_SP_ARG);
	CurScreen(SP_PARM)->_clear = FALSE;	 
	NewScreen(SP_PARM)->_clear = FALSE;	 
    } else {
	int changedlines = CHECK_INTERVAL;

	if (check_pending(NCURSES_SP_ARG))
	    goto cleanup;

	nonempty = min(screen_lines(SP_PARM), NewScreen(SP_PARM)->_maxy + 1);

	if (SP_PARM->_scrolling) {
	    NCURSES_SP_NAME(_nc_scroll_optimize) (NCURSES_SP_ARG);
	}

	nonempty = ClrBottom(NCURSES_SP_ARGx nonempty);

	TR(TRACE_UPDATE, ("Transforming lines, nonempty %d", nonempty));
	for (i = 0; i < nonempty; i++) {
	     
	    if (changedlines == CHECK_INTERVAL) {
		if (check_pending(NCURSES_SP_ARG))
		    goto cleanup;
		changedlines = 0;
	    }

	     
	    if (NewScreen(SP_PARM)->_line[i].firstchar != _NOCHANGE
		|| CurScreen(SP_PARM)->_line[i].firstchar != _NOCHANGE) {
		TransformLine(NCURSES_SP_ARGx i);
		changedlines++;
	    }

	     
	    if (i <= NewScreen(SP_PARM)->_maxy) {
		MARK_NOCHANGE(NewScreen(SP_PARM), i);
	    }
	    if (i <= CurScreen(SP_PARM)->_maxy) {
		MARK_NOCHANGE(CurScreen(SP_PARM), i);
	    }
	}
    }

     
    for (i = nonempty; i <= NewScreen(SP_PARM)->_maxy; i++) {
	MARK_NOCHANGE(NewScreen(SP_PARM), i);
    }
    for (i = nonempty; i <= CurScreen(SP_PARM)->_maxy; i++) {
	MARK_NOCHANGE(CurScreen(SP_PARM), i);
    }

    if (!NewScreen(SP_PARM)->_leaveok) {
	CurScreen(SP_PARM)->_curx = NewScreen(SP_PARM)->_curx;
	CurScreen(SP_PARM)->_cury = NewScreen(SP_PARM)->_cury;

	GoTo(NCURSES_SP_ARGx CurScreen(SP_PARM)->_cury, CurScreen(SP_PARM)->_curx);
    }

  cleanup:
     
#if USE_XMC_SUPPORT
    if (magic_cookie_glitch != 0)
#endif
	UpdateAttrs(SP_PARM, normal);

    NCURSES_SP_NAME(_nc_flush) (NCURSES_SP_ARG);
    WINDOW_ATTRS(CurScreen(SP_PARM)) = WINDOW_ATTRS(NewScreen(SP_PARM));

#if USE_TRACE_TIMES
    (void) times(&after);
    TR(TRACE_TIMES,
       ("Update cost: %ld chars, %ld clocks system time, %ld clocks user time",
	_nc_outchars,
	(long) (after.tms_stime - before.tms_stime),
	(long) (after.tms_utime - before.tms_utime)));
#endif  

    _nc_signal_handler(TRUE);

    _nc_unlock_global(update);
    returnCode(OK);
}

#if NCURSES_SP_FUNCS && !defined(USE_TERM_DRIVER)
NCURSES_EXPORT(int)
doupdate(void)
{
    return TINFO_DOUPDATE(CURRENT_SCREEN);
}
#endif

 
#define BCE_ATTRS (A_NORMAL|A_COLOR)
#define BCE_BKGD(sp,win) (((win) == CurScreen(sp) ? StdScreen(sp) : (win))->_nc_bkgd)

static NCURSES_INLINE NCURSES_CH_T
ClrBlank(NCURSES_SP_DCLx WINDOW *win)
{
    NCURSES_CH_T blank = blankchar;
    if (back_color_erase)
	AddAttr(blank, (AttrOf(BCE_BKGD(SP_PARM, win)) & BCE_ATTRS));
    return blank;
}

 

static void
ClrUpdate(NCURSES_SP_DCL0)
{
    TR(TRACE_UPDATE, (T_CALLED("ClrUpdate")));
    if (0 != SP_PARM) {
	int i;
	NCURSES_CH_T blank = ClrBlank(NCURSES_SP_ARGx StdScreen(SP_PARM));
	int nonempty = min(screen_lines(SP_PARM),
			   NewScreen(SP_PARM)->_maxy + 1);

	ClearScreen(NCURSES_SP_ARGx blank);

	TR(TRACE_UPDATE, ("updating screen from scratch"));

	nonempty = ClrBottom(NCURSES_SP_ARGx nonempty);

	for (i = 0; i < nonempty; i++)
	    TransformLine(NCURSES_SP_ARGx i);
    }
    TR(TRACE_UPDATE, (T_RETURN("")));
}

 

static void
ClrToEOL(NCURSES_SP_DCLx NCURSES_CH_T blank, int needclear)
{
    if (CurScreen(SP_PARM) != 0
	&& SP_PARM->_cursrow >= 0) {
	int j;

	for (j = SP_PARM->_curscol; j < screen_columns(SP_PARM); j++) {
	    if (j >= 0) {
		NCURSES_CH_T *cp =
		&(CurScreen(SP_PARM)->_line[SP_PARM->_cursrow].text[j]);

		if (!CharEq(*cp, blank)) {
		    *cp = blank;
		    needclear = TRUE;
		}
	    }
	}
    }

    if (needclear) {
	UpdateAttrs(SP_PARM, blank);
	if (clr_eol && SP_PARM->_el_cost <= (screen_columns(SP_PARM) - SP_PARM->_curscol)) {
	    NCURSES_PUTP2("clr_eol", clr_eol);
	} else {
	    int count = (screen_columns(SP_PARM) - SP_PARM->_curscol);
	    while (count-- > 0)
		PutChar(NCURSES_SP_ARGx CHREF(blank));
	}
    }
}

 

static void
ClrToEOS(NCURSES_SP_DCLx NCURSES_CH_T blank)
{
    int row, col;

    row = SP_PARM->_cursrow;
    col = SP_PARM->_curscol;

    if (row < 0)
	row = 0;
    if (col < 0)
	col = 0;

    UpdateAttrs(SP_PARM, blank);
    TPUTS_TRACE("clr_eos");
    NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
			    clr_eos,
			    screen_lines(SP_PARM) - row,
			    NCURSES_SP_NAME(_nc_outch));

    while (col < screen_columns(SP_PARM))
	CurScreen(SP_PARM)->_line[row].text[col++] = blank;

    for (row++; row < screen_lines(SP_PARM); row++) {
	for (col = 0; col < screen_columns(SP_PARM); col++)
	    CurScreen(SP_PARM)->_line[row].text[col] = blank;
    }
}

 
static int
ClrBottom(NCURSES_SP_DCLx int total)
{
    int top = total;
    int last = min(screen_columns(SP_PARM), NewScreen(SP_PARM)->_maxx + 1);
    NCURSES_CH_T blank = NewScreen(SP_PARM)->_line[total - 1].text[last - 1];

    if (clr_eos && can_clear_with(NCURSES_SP_ARGx CHREF(blank))) {
	int row;

	for (row = total - 1; row >= 0; row--) {
	    int col;
	    bool ok;

	    for (col = 0, ok = TRUE; ok && col < last; col++) {
		ok = (CharEq(NewScreen(SP_PARM)->_line[row].text[col], blank));
	    }
	    if (!ok)
		break;

	    for (col = 0; ok && col < last; col++) {
		ok = (CharEq(CurScreen(SP_PARM)->_line[row].text[col], blank));
	    }
	    if (!ok)
		top = row;
	}

	 
	if (top < total) {
	    GoTo(NCURSES_SP_ARGx top, 0);
	    ClrToEOS(NCURSES_SP_ARGx blank);
	    if (SP_PARM->oldhash && SP_PARM->newhash) {
		for (row = top; row < screen_lines(SP_PARM); row++)
		    SP_PARM->oldhash[row] = SP_PARM->newhash[row];
	    }
	}
    }
    return top;
}

#if USE_XMC_SUPPORT
#if USE_WIDEC_SUPPORT
#define check_xmc_transition(sp, a, b)					\
    ((((a)->attr ^ (b)->attr) & ~((a)->attr) & (sp)->_xmc_triggers) != 0)
#define xmc_turn_on(sp,a,b) check_xmc_transition(sp,&(a), &(b))
#else
#define xmc_turn_on(sp,a,b) ((((a)^(b)) & ~(a) & (sp)->_xmc_triggers) != 0)
#endif

#define xmc_new(sp,r,c) NewScreen(sp)->_line[r].text[c]
#define xmc_turn_off(sp,a,b) xmc_turn_on(sp,b,a)
#endif  

 

static void
TransformLine(NCURSES_SP_DCLx int const lineno)
{
    int firstChar, oLastChar, nLastChar;
    NCURSES_CH_T *newLine = NewScreen(SP_PARM)->_line[lineno].text;
    NCURSES_CH_T *oldLine = CurScreen(SP_PARM)->_line[lineno].text;
    int n;
    bool attrchanged = FALSE;

    TR(TRACE_UPDATE, (T_CALLED("TransformLine(%p, %d)"), (void *) SP_PARM, lineno));

     
    if (SP_PARM->oldhash && SP_PARM->newhash)
	SP_PARM->oldhash[lineno] = SP_PARM->newhash[lineno];

     
    if (SP_PARM->_coloron) {
	int oldPair;
	int newPair;

	for (n = 0; n < screen_columns(SP_PARM); n++) {
	    if (!CharEq(newLine[n], oldLine[n])) {
		oldPair = GetPair(oldLine[n]);
		newPair = GetPair(newLine[n]);
		if (oldPair != newPair
		    && unColor(oldLine[n]) == unColor(newLine[n])) {
		    if (oldPair < SP_PARM->_pair_alloc
			&& newPair < SP_PARM->_pair_alloc
			&& (isSamePair(SP_PARM->_color_pairs[oldPair],
				       SP_PARM->_color_pairs[newPair]))) {
			SetPair(oldLine[n], GetPair(newLine[n]));
		    }
		}
	    }
	}
    }

    if (ceol_standout_glitch && clr_eol) {
	firstChar = 0;
	while (firstChar < screen_columns(SP_PARM)) {
	    if (!SameAttrOf(newLine[firstChar], oldLine[firstChar])) {
		attrchanged = TRUE;
		break;
	    }
	    firstChar++;
	}
    }

    firstChar = 0;

    if (attrchanged) {		 
	GoTo(NCURSES_SP_ARGx lineno, firstChar);
	ClrToEOL(NCURSES_SP_ARGx
		 ClrBlank(NCURSES_SP_ARGx
			  CurScreen(SP_PARM)), FALSE);
	PutRange(NCURSES_SP_ARGx
		 oldLine, newLine, lineno, 0,
		 screen_columns(SP_PARM) - 1);
#if USE_XMC_SUPPORT

	 
    } else if (magic_cookie_glitch > 0) {
	GoTo(NCURSES_SP_ARGx lineno, firstChar);
	for (n = 0; n < screen_columns(SP_PARM); n++) {
	    int m = n + magic_cookie_glitch;

	     
	    if (ISBLANK(newLine[n])
		&& ((n > 0
		     && xmc_turn_on(SP_PARM, newLine[n - 1], newLine[n]))
		    || (n == 0
			&& lineno > 0
			&& xmc_turn_on(SP_PARM,
				       xmc_new(SP_PARM, lineno - 1,
					       screen_columns(SP_PARM) - 1),
				       newLine[n])))) {
		n = m;
	    }

	    PutChar(NCURSES_SP_ARGx CHREF(newLine[n]));

	     
	    if (!ISBLANK(newLine[n])
		&& ((n + 1 < screen_columns(SP_PARM)
		     && xmc_turn_off(SP_PARM, newLine[n], newLine[n + 1]))
		    || (n + 1 >= screen_columns(SP_PARM)
			&& lineno + 1 < screen_lines(SP_PARM)
			&& xmc_turn_off(SP_PARM,
					newLine[n],
					xmc_new(SP_PARM, lineno + 1, 0))))) {
		n = m;
	    }

	}
#endif
    } else {
	NCURSES_CH_T blank;

	 
	blank = newLine[0];
	if (clr_bol && can_clear_with(NCURSES_SP_ARGx CHREF(blank))) {
	    int oFirstChar, nFirstChar;

	    for (oFirstChar = 0;
		 oFirstChar < screen_columns(SP_PARM);
		 oFirstChar++)
		if (!CharEq(oldLine[oFirstChar], blank))
		    break;
	    for (nFirstChar = 0;
		 nFirstChar < screen_columns(SP_PARM);
		 nFirstChar++)
		if (!CharEq(newLine[nFirstChar], blank))
		    break;

	    if (nFirstChar == oFirstChar) {
		firstChar = nFirstChar;
		 
		while (firstChar < screen_columns(SP_PARM)
		       && CharEq(newLine[firstChar], oldLine[firstChar]))
		    firstChar++;
	    } else if (oFirstChar > nFirstChar) {
		firstChar = nFirstChar;
	    } else {		 
		firstChar = oFirstChar;
		if (SP_PARM->_el1_cost < nFirstChar - oFirstChar) {
		    if (nFirstChar >= screen_columns(SP_PARM)
			&& SP_PARM->_el_cost <= SP_PARM->_el1_cost) {
			GoTo(NCURSES_SP_ARGx lineno, 0);
			UpdateAttrs(SP_PARM, blank);
			NCURSES_PUTP2("clr_eol", clr_eol);
		    } else {
			GoTo(NCURSES_SP_ARGx lineno, nFirstChar - 1);
			UpdateAttrs(SP_PARM, blank);
			NCURSES_PUTP2("clr_bol", clr_bol);
		    }

		    while (firstChar < nFirstChar)
			oldLine[firstChar++] = blank;
		}
	    }
	} else {
	     
	    while (firstChar < screen_columns(SP_PARM)
		   && CharEq(newLine[firstChar], oldLine[firstChar]))
		firstChar++;
	}
	 
	if (firstChar >= screen_columns(SP_PARM)) {
	    TR(TRACE_UPDATE, (T_RETURN("")));
	    return;
	}

	blank = newLine[screen_columns(SP_PARM) - 1];

	if (!can_clear_with(NCURSES_SP_ARGx CHREF(blank))) {
	     
	    nLastChar = screen_columns(SP_PARM) - 1;

	    while (nLastChar > firstChar
		   && CharEq(newLine[nLastChar], oldLine[nLastChar]))
		nLastChar--;

	    if (nLastChar >= firstChar) {
		GoTo(NCURSES_SP_ARGx lineno, firstChar);
		PutRange(NCURSES_SP_ARGx
			 oldLine,
			 newLine,
			 lineno,
			 firstChar,
			 nLastChar);
		memcpy(oldLine + firstChar,
		       newLine + firstChar,
		       (unsigned) (nLastChar - firstChar + 1) * sizeof(NCURSES_CH_T));
	    }
	    TR(TRACE_UPDATE, (T_RETURN("")));
	    return;
	}

	 
	oLastChar = screen_columns(SP_PARM) - 1;
	while (oLastChar > firstChar && CharEq(oldLine[oLastChar], blank))
	    oLastChar--;

	 
	nLastChar = screen_columns(SP_PARM) - 1;
	while (nLastChar > firstChar && CharEq(newLine[nLastChar], blank))
	    nLastChar--;

	if ((nLastChar == firstChar)
	    && (SP_PARM->_el_cost < (oLastChar - nLastChar))) {
	    GoTo(NCURSES_SP_ARGx lineno, firstChar);
	    if (!CharEq(newLine[firstChar], blank))
		PutChar(NCURSES_SP_ARGx CHREF(newLine[firstChar]));
	    ClrToEOL(NCURSES_SP_ARGx blank, FALSE);
	} else if ((nLastChar != oLastChar)
		   && (!CharEq(newLine[nLastChar], oldLine[oLastChar])
		       || !(SP_PARM->_nc_sp_idcok
			    && NCURSES_SP_NAME(has_ic) (NCURSES_SP_ARG)))) {
	    GoTo(NCURSES_SP_ARGx lineno, firstChar);
	    if ((oLastChar - nLastChar) > SP_PARM->_el_cost) {
		if (PutRange(NCURSES_SP_ARGx
			     oldLine,
			     newLine,
			     lineno,
			     firstChar,
			     nLastChar)) {
		    GoTo(NCURSES_SP_ARGx lineno, nLastChar + 1);
		}
		ClrToEOL(NCURSES_SP_ARGx blank, FALSE);
	    } else {
		n = max(nLastChar, oLastChar);
		PutRange(NCURSES_SP_ARGx
			 oldLine,
			 newLine,
			 lineno,
			 firstChar,
			 n);
	    }
	} else {
	    int nLastNonblank = nLastChar;
	    int oLastNonblank = oLastChar;

	     
	     
	    while (CharEq(newLine[nLastChar], oldLine[oLastChar])) {
		 
		if (isWidecExt(newLine[nLastChar]) &&
		    !CharEq(newLine[nLastChar - 1], oldLine[oLastChar - 1]))
		    break;
		nLastChar--;
		oLastChar--;
		if (nLastChar == -1 || oLastChar == -1)
		    break;
	    }

	    n = min(oLastChar, nLastChar);
	    if (n >= firstChar) {
		GoTo(NCURSES_SP_ARGx lineno, firstChar);
		PutRange(NCURSES_SP_ARGx
			 oldLine,
			 newLine,
			 lineno,
			 firstChar,
			 n);
	    }

	    if (oLastChar < nLastChar) {
		int m = max(nLastNonblank, oLastNonblank);
#if USE_WIDEC_SUPPORT
		if (n) {
		    while (isWidecExt(newLine[n + 1]) && n) {
			--n;
			--oLastChar;	 
		    }
		} else if (n >= firstChar &&
			   isWidecBase(newLine[n])) {
		    while (isWidecExt(newLine[n + 1])) {
			++n;
			++oLastChar;	 
		    }
		}
#endif
		GoTo(NCURSES_SP_ARGx lineno, n + 1);
		if ((nLastChar < nLastNonblank)
		    || InsCharCost(SP_PARM, nLastChar - oLastChar) > (m - n)) {
		    PutRange(NCURSES_SP_ARGx
			     oldLine,
			     newLine,
			     lineno,
			     n + 1,
			     m);
		} else {
		    InsStr(NCURSES_SP_ARGx &newLine[n + 1], nLastChar - oLastChar);
		}
	    } else if (oLastChar > nLastChar) {
		GoTo(NCURSES_SP_ARGx lineno, n + 1);
		if (DelCharCost(SP_PARM, oLastChar - nLastChar)
		    > SP_PARM->_el_cost + nLastNonblank - (n + 1)) {
		    if (PutRange(NCURSES_SP_ARGx oldLine, newLine, lineno,
				 n + 1, nLastNonblank)) {
			GoTo(NCURSES_SP_ARGx lineno, nLastNonblank + 1);
		    }
		    ClrToEOL(NCURSES_SP_ARGx blank, FALSE);
		} else {
		     
		    UpdateAttrs(SP_PARM, blank);
		    DelChar(NCURSES_SP_ARGx oLastChar - nLastChar);
		}
	    }
	}
    }

     
    if (screen_columns(SP_PARM) > firstChar)
	memcpy(oldLine + firstChar,
	       newLine + firstChar,
	       (unsigned) (screen_columns(SP_PARM) - firstChar) * sizeof(NCURSES_CH_T));
    TR(TRACE_UPDATE, (T_RETURN("")));
    return;
}

 

static void
ClearScreen(NCURSES_SP_DCLx NCURSES_CH_T blank)
{
    int i, j;
    bool fast_clear = (clear_screen || clr_eos || clr_eol);

    TR(TRACE_UPDATE, ("ClearScreen() called"));

#if NCURSES_EXT_FUNCS
    if (SP_PARM->_coloron
	&& !SP_PARM->_default_color) {
	NCURSES_SP_NAME(_nc_do_color) (NCURSES_SP_ARGx
				       (short) GET_SCREEN_PAIR(SP_PARM),
				       0,
				       FALSE,
				       NCURSES_SP_NAME(_nc_outch));
	if (!back_color_erase) {
	    fast_clear = FALSE;
	}
    }
#endif

    if (fast_clear) {
	if (clear_screen) {
	    UpdateAttrs(SP_PARM, blank);
	    NCURSES_PUTP2("clear_screen", clear_screen);
	    SP_PARM->_cursrow = SP_PARM->_curscol = 0;
	    position_check(NCURSES_SP_ARGx
			   SP_PARM->_cursrow,
			   SP_PARM->_curscol,
			   "ClearScreen");
	} else if (clr_eos) {
	    SP_PARM->_cursrow = SP_PARM->_curscol = -1;
	    GoTo(NCURSES_SP_ARGx 0, 0);
	    UpdateAttrs(SP_PARM, blank);
	    TPUTS_TRACE("clr_eos");
	    NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				    clr_eos,
				    screen_lines(SP_PARM),
				    NCURSES_SP_NAME(_nc_outch));
	} else if (clr_eol) {
	    SP_PARM->_cursrow = SP_PARM->_curscol = -1;
	    UpdateAttrs(SP_PARM, blank);
	    for (i = 0; i < screen_lines(SP_PARM); i++) {
		GoTo(NCURSES_SP_ARGx i, 0);
		NCURSES_PUTP2("clr_eol", clr_eol);
	    }
	    GoTo(NCURSES_SP_ARGx 0, 0);
	}
    } else {
	UpdateAttrs(SP_PARM, blank);
	for (i = 0; i < screen_lines(SP_PARM); i++) {
	    GoTo(NCURSES_SP_ARGx i, 0);
	    for (j = 0; j < screen_columns(SP_PARM); j++)
		PutChar(NCURSES_SP_ARGx CHREF(blank));
	}
	GoTo(NCURSES_SP_ARGx 0, 0);
    }

    for (i = 0; i < screen_lines(SP_PARM); i++) {
	for (j = 0; j < screen_columns(SP_PARM); j++)
	    CurScreen(SP_PARM)->_line[i].text[j] = blank;
    }

    TR(TRACE_UPDATE, ("screen cleared"));
}

 

static void
InsStr(NCURSES_SP_DCLx NCURSES_CH_T *line, int count)
{
    TR(TRACE_UPDATE, ("InsStr(%p, %p,%d) called",
		      (void *) SP_PARM,
		      (void *) line, count));

     
     
    if (parm_ich) {
	TPUTS_TRACE("parm_ich");
	NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				TIPARM_1(parm_ich, count),
				1,
				NCURSES_SP_NAME(_nc_outch));
	while (count > 0) {
	    PutAttrChar(NCURSES_SP_ARGx CHREF(*line));
	    line++;
	    count--;
	}
    } else if (enter_insert_mode && exit_insert_mode) {
	NCURSES_PUTP2("enter_insert_mode", enter_insert_mode);
	while (count > 0) {
	    PutAttrChar(NCURSES_SP_ARGx CHREF(*line));
	    if (insert_padding) {
		NCURSES_PUTP2("insert_padding", insert_padding);
	    }
	    line++;
	    count--;
	}
	NCURSES_PUTP2("exit_insert_mode", exit_insert_mode);
    } else {
	while (count > 0) {
	    NCURSES_PUTP2("insert_character", insert_character);
	    PutAttrChar(NCURSES_SP_ARGx CHREF(*line));
	    if (insert_padding) {
		NCURSES_PUTP2("insert_padding", insert_padding);
	    }
	    line++;
	    count--;
	}
    }
    position_check(NCURSES_SP_ARGx
		   SP_PARM->_cursrow,
		   SP_PARM->_curscol, "InsStr");
}

 

static void
DelChar(NCURSES_SP_DCLx int count)
{
    TR(TRACE_UPDATE, ("DelChar(%p, %d) called, position = (%ld,%ld)",
		      (void *) SP_PARM, count,
		      (long) NewScreen(SP_PARM)->_cury,
		      (long) NewScreen(SP_PARM)->_curx));

    if (parm_dch) {
	TPUTS_TRACE("parm_dch");
	NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				TIPARM_1(parm_dch, count),
				1,
				NCURSES_SP_NAME(_nc_outch));
    } else {
	int n;

	for (n = 0; n < count; n++) {
	    NCURSES_PUTP2("delete_character", delete_character);
	}
    }
}

 

 
static int
scroll_csr_forward(NCURSES_SP_DCLx
		   int n,
		   int top,
		   int bot,
		   int miny,
		   int maxy,
		   NCURSES_CH_T blank)
{
    int i;

    if (n == 1 && scroll_forward && top == miny && bot == maxy) {
	GoTo(NCURSES_SP_ARGx bot, 0);
	UpdateAttrs(SP_PARM, blank);
	NCURSES_PUTP2("scroll_forward", scroll_forward);
    } else if (n == 1 && delete_line && bot == maxy) {
	GoTo(NCURSES_SP_ARGx top, 0);
	UpdateAttrs(SP_PARM, blank);
	NCURSES_PUTP2("delete_line", delete_line);
    } else if (parm_index && top == miny && bot == maxy) {
	GoTo(NCURSES_SP_ARGx bot, 0);
	UpdateAttrs(SP_PARM, blank);
	TPUTS_TRACE("parm_index");
	NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				TIPARM_1(parm_index, n),
				n,
				NCURSES_SP_NAME(_nc_outch));
    } else if (parm_delete_line && bot == maxy) {
	GoTo(NCURSES_SP_ARGx top, 0);
	UpdateAttrs(SP_PARM, blank);
	TPUTS_TRACE("parm_delete_line");
	NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				TIPARM_1(parm_delete_line, n),
				n,
				NCURSES_SP_NAME(_nc_outch));
    } else if (scroll_forward && top == miny && bot == maxy) {
	GoTo(NCURSES_SP_ARGx bot, 0);
	UpdateAttrs(SP_PARM, blank);
	for (i = 0; i < n; i++) {
	    NCURSES_PUTP2("scroll_forward", scroll_forward);
	}
    } else if (delete_line && bot == maxy) {
	GoTo(NCURSES_SP_ARGx top, 0);
	UpdateAttrs(SP_PARM, blank);
	for (i = 0; i < n; i++) {
	    NCURSES_PUTP2("delete_line", delete_line);
	}
    } else
	return ERR;

#if NCURSES_EXT_FUNCS
    if (FILL_BCE(SP_PARM)) {
	int j;
	for (i = 0; i < n; i++) {
	    GoTo(NCURSES_SP_ARGx bot - i, 0);
	    for (j = 0; j < screen_columns(SP_PARM); j++)
		PutChar(NCURSES_SP_ARGx CHREF(blank));
	}
    }
#endif
    return OK;
}

 
 
static int
scroll_csr_backward(NCURSES_SP_DCLx
		    int n,
		    int top,
		    int bot,
		    int miny,
		    int maxy,
		    NCURSES_CH_T blank)
{
    int i;

    if (n == 1 && scroll_reverse && top == miny && bot == maxy) {
	GoTo(NCURSES_SP_ARGx top, 0);
	UpdateAttrs(SP_PARM, blank);
	NCURSES_PUTP2("scroll_reverse", scroll_reverse);
    } else if (n == 1 && insert_line && bot == maxy) {
	GoTo(NCURSES_SP_ARGx top, 0);
	UpdateAttrs(SP_PARM, blank);
	NCURSES_PUTP2("insert_line", insert_line);
    } else if (parm_rindex && top == miny && bot == maxy) {
	GoTo(NCURSES_SP_ARGx top, 0);
	UpdateAttrs(SP_PARM, blank);
	TPUTS_TRACE("parm_rindex");
	NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				TIPARM_1(parm_rindex, n),
				n,
				NCURSES_SP_NAME(_nc_outch));
    } else if (parm_insert_line && bot == maxy) {
	GoTo(NCURSES_SP_ARGx top, 0);
	UpdateAttrs(SP_PARM, blank);
	TPUTS_TRACE("parm_insert_line");
	NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				TIPARM_1(parm_insert_line, n),
				n,
				NCURSES_SP_NAME(_nc_outch));
    } else if (scroll_reverse && top == miny && bot == maxy) {
	GoTo(NCURSES_SP_ARGx top, 0);
	UpdateAttrs(SP_PARM, blank);
	for (i = 0; i < n; i++) {
	    NCURSES_PUTP2("scroll_reverse", scroll_reverse);
	}
    } else if (insert_line && bot == maxy) {
	GoTo(NCURSES_SP_ARGx top, 0);
	UpdateAttrs(SP_PARM, blank);
	for (i = 0; i < n; i++) {
	    NCURSES_PUTP2("insert_line", insert_line);
	}
    } else
	return ERR;

#if NCURSES_EXT_FUNCS
    if (FILL_BCE(SP_PARM)) {
	int j;
	for (i = 0; i < n; i++) {
	    GoTo(NCURSES_SP_ARGx top + i, 0);
	    for (j = 0; j < screen_columns(SP_PARM); j++)
		PutChar(NCURSES_SP_ARGx CHREF(blank));
	}
    }
#endif
    return OK;
}

 
 
static int
scroll_idl(NCURSES_SP_DCLx int n, int del, int ins, NCURSES_CH_T blank)
{
    int i;

    if (!((parm_delete_line || delete_line) && (parm_insert_line || insert_line)))
	return ERR;

    GoTo(NCURSES_SP_ARGx del, 0);
    UpdateAttrs(SP_PARM, blank);
    if (n == 1 && delete_line) {
	NCURSES_PUTP2("delete_line", delete_line);
    } else if (parm_delete_line) {
	TPUTS_TRACE("parm_delete_line");
	NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				TIPARM_1(parm_delete_line, n),
				n,
				NCURSES_SP_NAME(_nc_outch));
    } else {			 
	for (i = 0; i < n; i++) {
	    NCURSES_PUTP2("delete_line", delete_line);
	}
    }

    GoTo(NCURSES_SP_ARGx ins, 0);
    UpdateAttrs(SP_PARM, blank);
    if (n == 1 && insert_line) {
	NCURSES_PUTP2("insert_line", insert_line);
    } else if (parm_insert_line) {
	TPUTS_TRACE("parm_insert_line");
	NCURSES_SP_NAME(tputs) (NCURSES_SP_ARGx
				TIPARM_1(parm_insert_line, n),
				n,
				NCURSES_SP_NAME(_nc_outch));
    } else {			 
	for (i = 0; i < n; i++) {
	    NCURSES_PUTP2("insert_line", insert_line);
	}
    }

    return OK;
}

 
NCURSES_EXPORT(int)
NCURSES_SP_NAME(_nc_scrolln) (NCURSES_SP_DCLx
			      int n,
			      int top,
			      int bot,
			      int maxy)
 
{
    NCURSES_CH_T blank;
    int i;
    bool cursor_saved = FALSE;
    int res;

    TR(TRACE_MOVE, ("_nc_scrolln(%p, %d, %d, %d, %d)",
		    (void *) SP_PARM, n, top, bot, maxy));

    if (!IsValidScreen(SP_PARM))
	return (ERR);

    blank = ClrBlank(NCURSES_SP_ARGx StdScreen(SP_PARM));

#if USE_XMC_SUPPORT
     
    if (magic_cookie_glitch > 0) {
	return (ERR);
    }
#endif

    if (n > 0) {		 
	 
	res = scroll_csr_forward(NCURSES_SP_ARGx n, top, bot, 0, maxy, blank);

	if (res == ERR && change_scroll_region) {
	    if ((((n == 1 && scroll_forward) || parm_index)
		 && (SP_PARM->_cursrow == bot || SP_PARM->_cursrow == bot - 1))
		&& save_cursor && restore_cursor) {
		cursor_saved = TRUE;
		NCURSES_PUTP2("save_cursor", save_cursor);
	    }
	    NCURSES_PUTP2("change_scroll_region",
			  TIPARM_2(change_scroll_region, top, bot));
	    if (cursor_saved) {
		NCURSES_PUTP2("restore_cursor", restore_cursor);
	    } else {
		SP_PARM->_cursrow = SP_PARM->_curscol = -1;
	    }

	    res = scroll_csr_forward(NCURSES_SP_ARGx n, top, bot, top, bot, blank);

	    NCURSES_PUTP2("change_scroll_region",
			  TIPARM_2(change_scroll_region, 0, maxy));
	    SP_PARM->_cursrow = SP_PARM->_curscol = -1;
	}

	if (res == ERR && SP_PARM->_nc_sp_idlok)
	    res = scroll_idl(NCURSES_SP_ARGx n, top, bot - n + 1, blank);

	 
	if (res != ERR
	    && (non_dest_scroll_region || (memory_below && bot == maxy))) {
	    static const NCURSES_CH_T blank2 = NewChar(BLANK_TEXT);
	    if (bot == maxy && clr_eos) {
		GoTo(NCURSES_SP_ARGx bot - n + 1, 0);
		ClrToEOS(NCURSES_SP_ARGx blank2);
	    } else {
		for (i = 0; i < n; i++) {
		    GoTo(NCURSES_SP_ARGx bot - i, 0);
		    ClrToEOL(NCURSES_SP_ARGx blank2, FALSE);
		}
	    }
	}

    } else {			 
	res = scroll_csr_backward(NCURSES_SP_ARGx -n, top, bot, 0, maxy, blank);

	if (res == ERR && change_scroll_region) {
	    if (top != 0
		&& (SP_PARM->_cursrow == top ||
		    SP_PARM->_cursrow == top - 1)
		&& save_cursor && restore_cursor) {
		cursor_saved = TRUE;
		NCURSES_PUTP2("save_cursor", save_cursor);
	    }
	    NCURSES_PUTP2("change_scroll_region",
			  TIPARM_2(change_scroll_region, top, bot));
	    if (cursor_saved) {
		NCURSES_PUTP2("restore_cursor", restore_cursor);
	    } else {
		SP_PARM->_cursrow = SP_PARM->_curscol = -1;
	    }

	    res = scroll_csr_backward(NCURSES_SP_ARGx
				      -n, top, bot, top, bot, blank);

	    NCURSES_PUTP2("change_scroll_region",
			  TIPARM_2(change_scroll_region, 0, maxy));
	    SP_PARM->_cursrow = SP_PARM->_curscol = -1;
	}

	if (res == ERR && SP_PARM->_nc_sp_idlok)
	    res = scroll_idl(NCURSES_SP_ARGx -n, bot + n + 1, top, blank);

	 
	if (res != ERR
	    && (non_dest_scroll_region || (memory_above && top == 0))) {
	    static const NCURSES_CH_T blank2 = NewChar(BLANK_TEXT);
	    for (i = 0; i < -n; i++) {
		GoTo(NCURSES_SP_ARGx i + top, 0);
		ClrToEOL(NCURSES_SP_ARGx blank2, FALSE);
	    }
	}
    }

    if (res == ERR)
	return (ERR);

    _nc_scroll_window(CurScreen(SP_PARM), n,
		      (NCURSES_SIZE_T) top,
		      (NCURSES_SIZE_T) bot,
		      blank);

     
    NCURSES_SP_NAME(_nc_scroll_oldhash) (NCURSES_SP_ARGx n, top, bot);

    return (OK);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
_nc_scrolln(int n, int top, int bot, int maxy)
{
    return NCURSES_SP_NAME(_nc_scrolln) (CURRENT_SCREEN, n, top, bot, maxy);
}
#endif

NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_screen_resume) (NCURSES_SP_DCL0)
{
    assert(SP_PARM);

     
    SetAttr(SCREEN_ATTRS(SP_PARM), A_NORMAL);
    NewScreen(SP_PARM)->_clear = TRUE;

     
    if (SP_PARM->_coloron || SP_PARM->_color_defs)
	NCURSES_SP_NAME(_nc_reset_colors) (NCURSES_SP_ARG);

     
    if (SP_PARM->_color_defs < 0 && !SP_PARM->_direct_color.value) {
	int n;
	SP_PARM->_color_defs = -(SP_PARM->_color_defs);
	for (n = 0; n < SP_PARM->_color_defs; ++n) {
	    if (SP_PARM->_color_table[n].init) {
		_nc_init_color(SP_PARM,
			       n,
			       SP_PARM->_color_table[n].r,
			       SP_PARM->_color_table[n].g,
			       SP_PARM->_color_table[n].b);
	    }
	}
    }

    if (exit_attribute_mode)
	NCURSES_PUTP2("exit_attribute_mode", exit_attribute_mode);
    else {
	 
	if (exit_alt_charset_mode)
	    NCURSES_PUTP2("exit_alt_charset_mode", exit_alt_charset_mode);
	if (exit_standout_mode)
	    NCURSES_PUTP2("exit_standout_mode", exit_standout_mode);
	if (exit_underline_mode)
	    NCURSES_PUTP2("exit_underline_mode", exit_underline_mode);
    }
    if (exit_insert_mode)
	NCURSES_PUTP2("exit_insert_mode", exit_insert_mode);
    if (enter_am_mode && exit_am_mode) {
	if (auto_right_margin) {
	    NCURSES_PUTP2("enter_am_mode", enter_am_mode);
	} else {
	    NCURSES_PUTP2("exit_am_mode", exit_am_mode);
	}
    }
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_screen_resume(void)
{
    NCURSES_SP_NAME(_nc_screen_resume) (CURRENT_SCREEN);
}
#endif

NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_screen_init) (NCURSES_SP_DCL0)
{
    NCURSES_SP_NAME(_nc_screen_resume) (NCURSES_SP_ARG);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_screen_init(void)
{
    NCURSES_SP_NAME(_nc_screen_init) (CURRENT_SCREEN);
}
#endif

 
NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_screen_wrap) (NCURSES_SP_DCL0)
{
    if (SP_PARM != 0) {

	UpdateAttrs(SP_PARM, normal);
#if NCURSES_EXT_FUNCS
	if (SP_PARM->_coloron
	    && !SP_PARM->_default_color) {
	    static const NCURSES_CH_T blank = NewChar(BLANK_TEXT);
	    SP_PARM->_default_color = TRUE;
	    NCURSES_SP_NAME(_nc_do_color) (NCURSES_SP_ARGx
					   -1,
					   0,
					   FALSE,
					   NCURSES_SP_NAME(_nc_outch));
	    SP_PARM->_default_color = FALSE;

	    TINFO_MVCUR(NCURSES_SP_ARGx
			SP_PARM->_cursrow,
			SP_PARM->_curscol,
			screen_lines(SP_PARM) - 1,
			0);

	    ClrToEOL(NCURSES_SP_ARGx blank, TRUE);
	}
#endif
	if (SP_PARM->_color_defs) {
	    NCURSES_SP_NAME(_nc_reset_colors) (NCURSES_SP_ARG);
	}
    }
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_screen_wrap(void)
{
    NCURSES_SP_NAME(_nc_screen_wrap) (CURRENT_SCREEN);
}
#endif

#if USE_XMC_SUPPORT
NCURSES_EXPORT(void)
NCURSES_SP_NAME(_nc_do_xmc_glitch) (NCURSES_SP_DCLx attr_t previous)
{
    if (SP_PARM != 0) {
	attr_t chg = XMC_CHANGES(previous ^ AttrOf(SCREEN_ATTRS(SP_PARM)));

	while (chg != 0) {
	    if (chg & 1) {
		SP_PARM->_curscol += magic_cookie_glitch;
		if (SP_PARM->_curscol >= SP_PARM->_columns)
		    wrap_cursor(NCURSES_SP_ARG);
		TR(TRACE_UPDATE, ("bumped to %d,%d after cookie",
				  SP_PARM->_cursrow, SP_PARM->_curscol));
	    }
	    chg >>= 1;
	}
    }
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(void)
_nc_do_xmc_glitch(attr_t previous)
{
    NCURSES_SP_NAME(_nc_do_xmc_glitch) (CURRENT_SCREEN, previous);
}
#endif

#endif  
