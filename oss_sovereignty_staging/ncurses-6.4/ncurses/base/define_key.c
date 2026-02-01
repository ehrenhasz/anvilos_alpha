 

 

#include <curses.priv.h>

MODULE_ID("$Id: define_key.c,v 1.22 2020/02/02 23:34:34 tom Exp $")

NCURSES_EXPORT(int)
NCURSES_SP_NAME(define_key) (NCURSES_SP_DCLx const char *str, int keycode)
{
    int code = ERR;

    T((T_CALLED("define_key(%p, %s,%d)"), (void *) SP_PARM, _nc_visbuf(str), keycode));
    if (SP_PARM == 0 || !HasTInfoTerminal(SP_PARM)) {
	code = ERR;
    } else if (keycode > 0) {
	unsigned ukey = (unsigned) keycode;

#ifdef USE_TERM_DRIVER
#define CallHasKey(keycode) CallDriver_1(SP_PARM, td_kyExist, keycode)
#else
#define CallHasKey(keycode) NCURSES_SP_NAME(has_key)(NCURSES_SP_ARGx keycode)
#endif

	if (str != 0) {
	    NCURSES_SP_NAME(define_key) (NCURSES_SP_ARGx str, 0);
	} else if (CallHasKey(keycode)) {
	    while (_nc_remove_key(&(SP_PARM->_keytry), ukey))
		code = OK;
	}
	if (str != 0) {
	    if (NCURSES_SP_NAME(key_defined) (NCURSES_SP_ARGx str) == 0) {
		if (_nc_add_to_try(&(SP_PARM->_keytry), str, ukey) == OK) {
		    code = OK;
		} else {
		    code = ERR;
		}
	    } else {
		code = ERR;
	    }
	}
    } else {
	while (_nc_remove_string(&(SP_PARM->_keytry), str))
	    code = OK;
    }
    returnCode(code);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
define_key(const char *str, int keycode)
{
    return NCURSES_SP_NAME(define_key) (CURRENT_SCREEN, str, keycode);
}
#endif
