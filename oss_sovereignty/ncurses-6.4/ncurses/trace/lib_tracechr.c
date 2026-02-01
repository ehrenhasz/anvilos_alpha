 

 

 
#include <curses.priv.h>

#include <ctype.h>

MODULE_ID("$Id: lib_tracechr.c,v 1.23 2020/02/02 23:34:34 tom Exp $")

#ifdef TRACE

#define MyBufSize sizeof(_nc_globals.tracechr_buf)

NCURSES_EXPORT(char *)
_nc_tracechar(SCREEN *sp, int ch)
{
    NCURSES_CONST char *name;
    char *MyBuffer = ((sp != 0)
		      ? sp->tracechr_buf
		      : _nc_globals.tracechr_buf);

    if (ch > KEY_MIN || ch < 0) {
	name = safe_keyname(SP_PARM, ch);
	if (name == 0 || *name == '\0')
	    name = "NULL";
	_nc_SPRINTF(MyBuffer, _nc_SLIMIT(MyBufSize)
		    "'%.30s' = %#03o", name, ch);
    } else if (!is8bits(ch) || !isprint(UChar(ch))) {
	 
	_nc_SPRINTF(MyBuffer, _nc_SLIMIT(MyBufSize)
		    "%#03o", ch);
    } else {
	name = safe_unctrl(SP_PARM, (chtype) ch);
	if (name == 0 || *name == 0)
	    name = "null";	 
	_nc_SPRINTF(MyBuffer, _nc_SLIMIT(MyBufSize)
		    "'%.30s' = %#03o", name, ch);
    }
    return (MyBuffer);
}

NCURSES_EXPORT(char *)
_tracechar(int ch)
{
    return _nc_tracechar(CURRENT_SCREEN, ch);
}
#else
EMPTY_MODULE(_nc_lib_tracechr)
#endif
