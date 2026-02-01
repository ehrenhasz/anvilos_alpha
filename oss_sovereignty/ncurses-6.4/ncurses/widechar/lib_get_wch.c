 

 

 

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: lib_get_wch.c,v 1.26 2021/04/17 16:12:54 tom Exp $")

NCURSES_EXPORT(int)
wget_wch(WINDOW *win, wint_t *result)
{
    SCREEN *sp;
    int code;
    int value = 0;
#ifndef state_unused
    mbstate_t state;
#endif

    T((T_CALLED("wget_wch(%p)"), (void *) win));

     
    _nc_lock_global(curses);
    sp = _nc_screen_of(win);

    if (sp != 0) {
	size_t count = 0;

	for (;;) {
	    char buffer[(MB_LEN_MAX * 9) + 1];	 

	    T(("reading %d of %d", (int) count + 1, (int) sizeof(buffer)));
	    code = _nc_wgetch(win, &value, TRUE EVENTLIST_2nd((_nc_eventlist
							       *) 0));
	    if (code == ERR) {
		break;
	    } else if (code == KEY_CODE_YES) {
		 
		if (count != 0) {
		    safe_ungetch(SP_PARM, value);
		    code = ERR;
		}
		break;
	    } else if (count + 1 >= sizeof(buffer)) {
		safe_ungetch(SP_PARM, value);
		code = ERR;
		break;
	    } else {
		int status;

		buffer[count++] = (char) UChar(value);
		reset_mbytes(state);
		status = count_mbytes(buffer, count, state);
		if (status >= 0) {
		    wchar_t wch;
		    reset_mbytes(state);
		    if (check_mbytes(wch, buffer, count, state) != status) {
			code = ERR;	 
			safe_ungetch(SP_PARM, value);
		    }
		    value = wch;
		    break;
		}
	    }
	}
    } else {
	code = ERR;
    }

    if (result != 0)
	*result = (wint_t) value;

    _nc_unlock_global(curses);
    T(("result %#o", value));
    returnCode(code);
}
