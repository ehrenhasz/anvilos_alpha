 

 

 

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_traceatr.c,v 1.95 2022/06/11 22:40:56 tom Exp $")

#define COLOR_OF(c) ((c < 0) ? "default" : (c > 7 ? color_of(c) : colors[c].name))

#define TRACE_BUF_SIZE(num) (_nc_globals.tracebuf_ptr[num].size)
#define COLOR_BUF_SIZE(num) (sizeof(my_buffer[num]))

#ifdef TRACE

static const char l_brace[] = StringOf(L_BRACE);
static const char r_brace[] = StringOf(R_BRACE);

#ifndef USE_TERMLIB

#define my_buffer _nc_globals.traceatr_color_buf
#define my_select _nc_globals.traceatr_color_sel
#define my_cached _nc_globals.traceatr_color_last

static char *
color_of(int c)
{
    if (c != my_cached) {
	my_cached = c;
	my_select = !my_select;
	if (isDefaultColor(c))
	    _nc_STRCPY(my_buffer[my_select], "default",
		       COLOR_BUF_SIZE(my_select));
	else
	    _nc_SPRINTF(my_buffer[my_select],
			_nc_SLIMIT(COLOR_BUF_SIZE(my_select))
			"color%d", c);
    }
    return my_buffer[my_select];
}

#undef my_buffer
#undef my_select
#endif  

NCURSES_EXPORT(char *)
_traceattr2(int bufnum, chtype newmode)
{
#define DATA(name) { name, { #name } }
    static const struct {
	unsigned int val;
	const char name[14];
    } names[] =
    {
	DATA(A_STANDOUT),
	    DATA(A_UNDERLINE),
	    DATA(A_REVERSE),
	    DATA(A_BLINK),
	    DATA(A_DIM),
	    DATA(A_BOLD),
	    DATA(A_ALTCHARSET),
	    DATA(A_INVIS),
	    DATA(A_PROTECT),
	    DATA(A_CHARTEXT),
	    DATA(A_NORMAL),
	    DATA(A_COLOR),
#if USE_ITALIC
	    DATA(A_ITALIC),
#endif
    }
#ifndef USE_TERMLIB
    ,
	colors[] =
    {
	DATA(COLOR_BLACK),
	    DATA(COLOR_RED),
	    DATA(COLOR_GREEN),
	    DATA(COLOR_YELLOW),
	    DATA(COLOR_BLUE),
	    DATA(COLOR_MAGENTA),
	    DATA(COLOR_CYAN),
	    DATA(COLOR_WHITE),
    }
#endif  
    ;
#undef DATA
    char *result = _nc_trace_buf(bufnum, (size_t) BUFSIZ);

    if (result != 0) {
	size_t n;
	unsigned save_nc_tracing = _nc_tracing;

	_nc_tracing = 0;

	_nc_STRCPY(result, l_brace, TRACE_BUF_SIZE(bufnum));

	for (n = 0; n < SIZEOF(names); n++) {

	    if ((newmode & names[n].val) != 0) {
		if (result[1] != '\0')
		    (void) _nc_trace_bufcat(bufnum, "|");
		result = _nc_trace_bufcat(bufnum, names[n].name);

		if (names[n].val == A_COLOR) {
		    char temp[80];
		    short pairnum = (short) PairNumber(newmode);
#ifdef USE_TERMLIB
		     
		    _nc_SPRINTF(temp, _nc_SLIMIT(sizeof(temp))
				"{%d}", pairnum);
#else
		    NCURSES_COLOR_T fg, bg;

		    if (pair_content(pairnum, &fg, &bg) == OK) {
			_nc_SPRINTF(temp, _nc_SLIMIT(sizeof(temp))
				    "{%d = {%s, %s}}",
				    pairnum,
				    COLOR_OF(fg),
				    COLOR_OF(bg));
		    } else {
			_nc_SPRINTF(temp, _nc_SLIMIT(sizeof(temp))
				    "{%d}", pairnum);
		    }
#endif
		    result = _nc_trace_bufcat(bufnum, temp);
		}
	    }
	}
	if (ChAttrOf(newmode) == A_NORMAL) {
	    if (result != 0 && result[1] != '\0')
		(void) _nc_trace_bufcat(bufnum, "|");
	    (void) _nc_trace_bufcat(bufnum, "A_NORMAL");
	}

	_nc_tracing = save_nc_tracing;
	result = _nc_trace_bufcat(bufnum, r_brace);
    }
    return result;
}

NCURSES_EXPORT(char *)
_traceattr(attr_t newmode)
{
    return _traceattr2(0, newmode);
}

 
NCURSES_EXPORT(int)
_nc_retrace_int_attr_t(attr_t code)
{
    T((T_RETURN("%s"), _traceattr(code)));
    return (int) code;
}

 
NCURSES_EXPORT(attr_t)
_nc_retrace_attr_t(attr_t code)
{
    T((T_RETURN("%s"), _traceattr(code)));
    return code;
}

const char *
_nc_altcharset_name(attr_t attr, chtype ch)
{
#define DATA(code, name) { code, { #name } }
    typedef struct {
	unsigned int val;
	const char name[13];
    } ALT_NAMES;
#if NCURSES_SP_FUNCS
    SCREEN *sp = CURRENT_SCREEN;
#endif
    static const ALT_NAMES names[] =
    {
	DATA('l', ACS_ULCORNER),	 
	DATA('m', ACS_LLCORNER),	 
	DATA('k', ACS_URCORNER),	 
	DATA('j', ACS_LRCORNER),	 
	DATA('t', ACS_LTEE),	 
	DATA('u', ACS_RTEE),	 
	DATA('v', ACS_BTEE),	 
	DATA('w', ACS_TTEE),	 
	DATA('q', ACS_HLINE),	 
	DATA('x', ACS_VLINE),	 
	DATA('n', ACS_PLUS),	 
	DATA('o', ACS_S1),	 
	DATA('s', ACS_S9),	 
	DATA('`', ACS_DIAMOND),	 
	DATA('a', ACS_CKBOARD),	 
	DATA('f', ACS_DEGREE),	 
	DATA('g', ACS_PLMINUS),	 
	DATA('~', ACS_BULLET),	 
	DATA(',', ACS_LARROW),	 
	DATA('+', ACS_RARROW),	 
	DATA('.', ACS_DARROW),	 
	DATA('-', ACS_UARROW),	 
	DATA('h', ACS_BOARD),	 
	DATA('i', ACS_LANTERN),	 
	DATA('0', ACS_BLOCK),	 
	DATA('p', ACS_S3),	 
	DATA('r', ACS_S7),	 
	DATA('y', ACS_LEQUAL),	 
	DATA('z', ACS_GEQUAL),	 
	DATA('{', ACS_PI),	 
	DATA('|', ACS_NEQUAL),	 
	DATA('}', ACS_STERLING),	 
    };
#undef DATA

    const char *result = 0;

#if NCURSES_SP_FUNCS
    (void) sp;
#endif
    if (SP_PARM != 0 && (attr & A_ALTCHARSET) && (acs_chars != 0)) {
	char *cp;
	char *found = 0;

	for (cp = acs_chars; cp[0] && cp[1]; cp += 2) {
	    if (ChCharOf(UChar(cp[1])) == ChCharOf(ch)) {
		found = cp;
		 
	    }
	}

	if (found != 0) {
	    size_t n;

	    ch = ChCharOf(UChar(*found));
	    for (n = 0; n < SIZEOF(names); ++n) {
		if (names[n].val == ch) {
		    result = names[n].name;
		    break;
		}
	    }
	}
    }
    return result;
}

NCURSES_EXPORT(char *)
_tracechtype2(int bufnum, chtype ch)
{
    char *result = _nc_trace_buf(bufnum, (size_t) BUFSIZ);

    if (result != 0) {
	const char *found;
	attr_t attr = ChAttrOf(ch);

	_nc_STRCPY(result, l_brace, TRACE_BUF_SIZE(bufnum));
	if ((found = _nc_altcharset_name(attr, ch)) != 0) {
	    (void) _nc_trace_bufcat(bufnum, found);
	    attr &= ~A_ALTCHARSET;
	} else
	    (void) _nc_trace_bufcat(bufnum,
				    _nc_tracechar(CURRENT_SCREEN,
						  (int) ChCharOf(ch)));

	if (attr != A_NORMAL) {
	    (void) _nc_trace_bufcat(bufnum, " | ");
	    (void) _nc_trace_bufcat(bufnum,
				    _traceattr2(bufnum + 20, attr));
	}

	result = _nc_trace_bufcat(bufnum, r_brace);
    }
    return result;
}

NCURSES_EXPORT(char *)
_tracechtype(chtype ch)
{
    return _tracechtype2(0, ch);
}

 
NCURSES_EXPORT(chtype)
_nc_retrace_chtype(chtype code)
{
    T((T_RETURN("%s"), _tracechtype(code)));
    return code;
}

#if USE_WIDEC_SUPPORT
NCURSES_EXPORT(char *)
_tracecchar_t2(int bufnum, const cchar_t *ch)
{
    char *result = _nc_trace_buf(bufnum, (size_t) BUFSIZ);

    if (result != 0) {
	_nc_STRCPY(result, l_brace, TRACE_BUF_SIZE(bufnum));
	if (ch != 0) {
	    const char *found;
	    attr_t attr = AttrOfD(ch);

	    if ((found = _nc_altcharset_name(attr, (chtype) CharOfD(ch))) != 0) {
		(void) _nc_trace_bufcat(bufnum, found);
		attr &= ~A_ALTCHARSET;
	    } else if (isWidecExt(CHDEREF(ch))) {
		(void) _nc_trace_bufcat(bufnum, "{NAC}");
		attr &= ~A_CHARTEXT;
	    } else {
		PUTC_DATA;
		int n;

		(void) _nc_trace_bufcat(bufnum, "{ ");
		for (PUTC_i = 0; PUTC_i < CCHARW_MAX; ++PUTC_i) {
		    PUTC_ch = ch->chars[PUTC_i];
		    if (PUTC_ch == L'\0') {
			if (PUTC_i == 0)
			    (void) _nc_trace_bufcat(bufnum, "\\000");
			break;
		    }
		    PUTC_INIT;
		    PUTC_n = (int) wcrtomb(PUTC_buf, ch->chars[PUTC_i], &PUT_st);
		    if (PUTC_n <= 0) {
			if (PUTC_ch != L'\0') {
			     
			    (void) _nc_trace_bufcat(bufnum,
						    _nc_tracechar(CURRENT_SCREEN,
								  UChar(ch->chars[PUTC_i])));
			}
			break;
		    } else if (ch->chars[PUTC_i] > 255) {
			char temp[80];
			_nc_SPRINTF(temp, _nc_SLIMIT(sizeof(temp))
				    "{%d:\\u%lx}",
				    _nc_wacs_width(ch->chars[PUTC_i]),
				    (unsigned long) ch->chars[PUTC_i]);
			(void) _nc_trace_bufcat(bufnum, temp);
			attr &= ~A_CHARTEXT;	 
		    } else {
			for (n = 0; n < PUTC_n; n++) {
			    if (n)
				(void) _nc_trace_bufcat(bufnum, ", ");
			    (void) _nc_trace_bufcat(bufnum,
						    _nc_tracechar(CURRENT_SCREEN,
								  UChar(PUTC_buf[n])));
			}
		    }
		}
		(void) _nc_trace_bufcat(bufnum, " }");
	    }
	    if (attr != A_NORMAL) {
		(void) _nc_trace_bufcat(bufnum, " | ");
		(void) _nc_trace_bufcat(bufnum, _traceattr2(bufnum + 20, attr));
	    }
#if NCURSES_EXT_COLORS
	     
	    if (ch->ext_color != PairNumber(attr)) {
		char temp[80];
		_nc_SPRINTF(temp, _nc_SLIMIT(sizeof(temp))
			    " X_COLOR{%d:%d}", ch->ext_color, PairNumber(attr));
		(void) _nc_trace_bufcat(bufnum, temp);
	    }
#endif
	}

	result = _nc_trace_bufcat(bufnum, r_brace);
    }
    return result;
}

NCURSES_EXPORT(char *)
_tracecchar_t(const cchar_t *ch)
{
    return _tracecchar_t2(0, ch);
}
#endif

#else
EMPTY_MODULE(_nc_lib_traceatr)
#endif  
