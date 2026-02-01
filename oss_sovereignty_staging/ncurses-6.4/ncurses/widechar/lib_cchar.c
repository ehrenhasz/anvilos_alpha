 

 

#include <curses.priv.h>
#include <wchar.h>

MODULE_ID("$Id: lib_cchar.c,v 1.38 2022/07/27 08:03:16 tom Exp $")

 
NCURSES_EXPORT(int)
setcchar(cchar_t *wcval,
	 const wchar_t *wch,
	 const attr_t attrs,
	 NCURSES_PAIRS_T pair_arg,
	 const void *opts)
{
    int code = OK;
    int color_pair = pair_arg;
    unsigned len;

    TR(TRACE_CCALLS, (T_CALLED("setcchar(%p,%s,attrs=%lu,pair=%d,%p)"),
		      (void *) wcval, _nc_viswbuf(wch),
		      (unsigned long) attrs, color_pair, opts));

    set_extended_pair(opts, color_pair);
    if (wch == NULL
	|| ((len = (unsigned) wcslen(wch)) > 1 && _nc_wacs_width(wch[0]) < 0)
	|| color_pair < 0) {
	code = ERR;
    } else {
	unsigned i;

	if (len > CCHARW_MAX)
	    len = CCHARW_MAX;

	 
	for (i = 1; i < len; ++i) {
	    if (_nc_wacs_width(wch[i]) != 0) {
		len = i;
		break;
	    }
	}

	memset(wcval, 0, sizeof(*wcval));

	if (len != 0) {
	    SetAttr(*wcval, attrs);
	    SetPair(CHDEREF(wcval), color_pair);
	    memcpy(&wcval->chars, wch, len * sizeof(wchar_t));
	    TR(TRACE_CCALLS, ("copy %d wchars, first is %s", len,
			      _tracecchar_t(wcval)));
	}
    }

    TR(TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}

NCURSES_EXPORT(int)
getcchar(const cchar_t *wcval,
	 wchar_t *wch,
	 attr_t *attrs,
	 NCURSES_PAIRS_T *pair_arg,
	 void *opts)
{
    int code = ERR;

    TR(TRACE_CCALLS, (T_CALLED("getcchar(%p,%p,%p,%p,%p)"),
		      (const void *) wcval,
		      (void *) wch,
		      (void *) attrs,
		      (void *) pair_arg,
		      opts));

#if !NCURSES_EXT_COLORS
    if (opts != NULL) {
	;			 
    } else
#endif
    if (wcval != NULL) {
	wchar_t *wp;
	int len;

#if HAVE_WMEMCHR
	len = ((wp = wmemchr(wcval->chars, L'\0', (size_t) CCHARW_MAX))
	       ? (int) (wp - wcval->chars)
	       : CCHARW_MAX);
#else
	len = wcsnlen(wcval->chars, CCHARW_MAX);
#endif
	if (wch == NULL) {
	     
	    code = (len < CCHARW_MAX) ? (len + 1) : CCHARW_MAX;
	} else if (attrs == 0 || pair_arg == 0) {
	    code = ERR;
	} else if (len >= 0) {
	    int color_pair;

	    TR(TRACE_CCALLS, ("copy %d wchars, first is %s", len,
			      _tracecchar_t(wcval)));
	    *attrs = AttrOf(*wcval) & A_ATTRIBUTES;
	    color_pair = GetPair(*wcval);
	    get_extended_pair(opts, color_pair);
	    *pair_arg = limit_PAIRS(color_pair);
	    wmemcpy(wch, wcval->chars, (size_t) len);
	    wch[len] = L'\0';
	    if (*pair_arg >= 0)
		code = OK;
	}
    }

    TR(TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}
