 

 

#include <curses.priv.h>

MODULE_ID("$Id: keyok.c,v 1.17 2021/06/17 21:26:02 tom Exp $")

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(keyok) (NCURSES_SP_DCLx int c, bool flag)
{
    int code = ERR;

    if (HasTerminal(SP_PARM)) {
	T((T_CALLED("keyok(%p, %d,%d)"), (void *) SP_PARM, c, flag));
#ifdef USE_TERM_DRIVER
	code = CallDriver_2(sp, td_kyOk, c, flag);
#else
	if (c >= 0) {
	    int count = 0;
	    char *s;
	    unsigned ch = (unsigned) c;

	    if (flag) {
		while ((s = _nc_expand_try(SP_PARM->_key_ok,
					   ch, &count, (size_t) 0)) != 0) {
		    if (_nc_remove_key(&(SP_PARM->_key_ok), ch)) {
			code = _nc_add_to_try(&(SP_PARM->_keytry), s, ch);
			free(s);
			count = 0;
			if (code != OK)
			    break;
		    } else {
			free(s);
		    }
		}
	    } else {
		while ((s = _nc_expand_try(SP_PARM->_keytry,
					   ch, &count, (size_t) 0)) != 0) {
		    if (_nc_remove_key(&(SP_PARM->_keytry), ch)) {
			code = _nc_add_to_try(&(SP_PARM->_key_ok), s, ch);
			free(s);
			count = 0;
			if (code != OK)
			    break;
		    } else {
			free(s);
		    }
		}
	    }
	}
#endif
    }
    returnCode(code);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
keyok(int c, bool flag)
{
    return NCURSES_SP_NAME(keyok) (CURRENT_SCREEN, c, flag);
}
#endif
