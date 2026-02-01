 

 

 

#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_options.c,v 1.82 2021/02/14 00:17:35 tom Exp $")

NCURSES_EXPORT(int)
idlok(WINDOW *win, bool flag)
{
    int res = ERR;
    T((T_CALLED("idlok(%p,%d)"), (void *) win, flag));

    if (win) {
	SCREEN *sp = _nc_screen_of(win);
	if (sp != 0
#ifdef USE_TERM_DRIVER
	    && IsTermInfo(sp)
#endif
	    ) {
	    sp->_nc_sp_idlok =
		win->_idlok = (flag && (NCURSES_SP_NAME(has_il) (NCURSES_SP_ARG)
					|| change_scroll_region));
	    res = OK;
	}
    }
    returnCode(res);
}

NCURSES_EXPORT(void)
idcok(WINDOW *win, bool flag)
{
    T((T_CALLED("idcok(%p,%d)"), (void *) win, flag));

    if (win) {
	SCREEN *sp = _nc_screen_of(win);
	sp->_nc_sp_idcok = win->_idcok = (flag && NCURSES_SP_NAME(has_ic) (NCURSES_SP_ARG));
    }
    returnVoid;
}

NCURSES_EXPORT(int)
NCURSES_SP_NAME(halfdelay) (NCURSES_SP_DCLx int t)
{
    T((T_CALLED("halfdelay(%p,%d)"), (void *) SP_PARM, t));

    if (t < 1 || t > 255 || !SP_PARM || !IsValidTIScreen(SP_PARM))
	returnCode(ERR);

    NCURSES_SP_NAME(cbreak) (NCURSES_SP_ARG);
    SP_PARM->_cbreak = t + 1;
    returnCode(OK);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
halfdelay(int t)
{
    return NCURSES_SP_NAME(halfdelay) (CURRENT_SCREEN, t);
}
#endif

NCURSES_EXPORT(int)
nodelay(WINDOW *win, bool flag)
{
    T((T_CALLED("nodelay(%p,%d)"), (void *) win, flag));

    if (win) {
	if (flag == TRUE)
	    win->_delay = 0;
	else
	    win->_delay = -1;
	returnCode(OK);
    } else
	returnCode(ERR);
}

NCURSES_EXPORT(int)
notimeout(WINDOW *win, bool f)
{
    T((T_CALLED("notimeout(%p,%d)"), (void *) win, f));

    if (win) {
	win->_notimeout = f;
	returnCode(OK);
    } else
	returnCode(ERR);
}

NCURSES_EXPORT(void)
wtimeout(WINDOW *win, int delay)
{
    T((T_CALLED("wtimeout(%p,%d)"), (void *) win, delay));

    if (win) {
	win->_delay = delay;
    }
    returnVoid;
}

NCURSES_EXPORT(int)
keypad(WINDOW *win, bool flag)
{
    T((T_CALLED("keypad(%p,%d)"), (void *) win, flag));

    if (win) {
	win->_use_keypad = flag;
	returnCode(_nc_keypad(_nc_screen_of(win), flag));
    } else
	returnCode(ERR);
}

NCURSES_EXPORT(int)
meta(WINDOW *win GCC_UNUSED, bool flag)
{
    int result = ERR;
    SCREEN *sp = (win == 0) ? CURRENT_SCREEN : _nc_screen_of(win);

     
    T((T_CALLED("meta(%p,%d)"), (void *) win, flag));

     

    if (sp != 0) {
	sp->_use_meta = flag;
#ifdef USE_TERM_DRIVER
	if (IsTermInfo(sp)) {
	    if (flag) {
		NCURSES_PUTP2("meta_on", meta_on);
	    } else {
		NCURSES_PUTP2("meta_off", meta_off);
	    }
	}
#else
	if (flag) {
	    NCURSES_PUTP2("meta_on", meta_on);
	} else {
	    NCURSES_PUTP2("meta_off", meta_off);
	}
#endif
	result = OK;
    }
    returnCode(result);
}

 

NCURSES_EXPORT(int)
NCURSES_SP_NAME(curs_set) (NCURSES_SP_DCLx int vis)
{
    int code = ERR;
    T((T_CALLED("curs_set(%p,%d)"), (void *) SP_PARM, vis));

    if (SP_PARM != 0 && vis >= 0 && vis <= 2) {
	int cursor = SP_PARM->_cursor;
	if (vis == cursor) {
	    code = cursor;
	} else {
#ifdef USE_TERM_DRIVER
	    code = CallDriver_1(SP_PARM, td_cursorSet, vis);
#else
	    if (IsValidTIScreen(SP_PARM)) {
		switch (vis) {
		case 2:
		    code = NCURSES_PUTP2_FLUSH("cursor_visible",
					       cursor_visible);
		    break;
		case 1:
		    code = NCURSES_PUTP2_FLUSH("cursor_normal",
					       cursor_normal);
		    break;
		case 0:
		    code = NCURSES_PUTP2_FLUSH("cursor_invisible",
					       cursor_invisible);
		    break;
		}
	    } else {
		code = ERR;
	    }
#endif
	    if (code != ERR)
		code = (cursor == -1 ? 1 : cursor);
	    SP_PARM->_cursor = vis;
	}
    }
    returnCode(code);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
curs_set(int vis)
{
    return (NCURSES_SP_NAME(curs_set) (CURRENT_SCREEN, vis));
}
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(typeahead) (NCURSES_SP_DCLx int fd)
{
    T((T_CALLED("typeahead(%p, %d)"), (void *) SP_PARM, fd));
    if (SP_PARM && IsValidTIScreen(SP_PARM)) {
	SP_PARM->_checkfd = fd;
	returnCode(OK);
    } else {
	returnCode(ERR);
    }
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
typeahead(int fd)
{
    return NCURSES_SP_NAME(typeahead) (CURRENT_SCREEN, fd);
}
#endif

 

#if NCURSES_EXT_FUNCS
static int
has_key_internal(int keycode, TRIES * tp)
{
    if (tp == 0)
	return (FALSE);
    else if (tp->value == keycode)
	return (TRUE);
    else
	return (has_key_internal(keycode, tp->child)
		|| has_key_internal(keycode, tp->sibling));
}

#ifdef USE_TERM_DRIVER
NCURSES_EXPORT(int)
TINFO_HAS_KEY(SCREEN *sp, int keycode)
{
    return IsValidTIScreen(sp) ?
	has_key_internal(keycode, sp->_keytry) : 0;
}
#else
NCURSES_EXPORT(int)
NCURSES_SP_NAME(has_key) (NCURSES_SP_DCLx int keycode)
{
    T((T_CALLED("has_key(%p,%d)"), (void *) SP_PARM, keycode));
    returnCode(SP != 0 ? has_key_internal(keycode, SP_PARM->_keytry) : FALSE);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
has_key(int keycode)
{
    return NCURSES_SP_NAME(has_key) (CURRENT_SCREEN, keycode);
}
#endif
#endif
#endif  

NCURSES_EXPORT(int)
NCURSES_SP_NAME(_nc_putp_flush) (NCURSES_SP_DCLx
				 const char *name, const char *value)
{
    int rc = NCURSES_PUTP2(name, value);
    if (rc != ERR) {
	_nc_flush();
    }
    return rc;
}

#if 0 && NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
_nc_putp_flush(const char *name, const char *value)
{
    return NCURSES_SP_NAME(_nc_putp_flush) (CURRENT_SCREEN, name, value);
}
#endif

 
NCURSES_EXPORT(int)
_nc_keypad(SCREEN *sp, int flag)
{
    int rc = ERR;

    if (sp != 0) {
#ifdef USE_PTHREADS
	 
	if (_nc_use_pthreads && sp != CURRENT_SCREEN) {
	    SCREEN *save_sp;

	     
	    _nc_lock_global(curses);
	    save_sp = CURRENT_SCREEN;
	    _nc_set_screen(sp);
	    rc = _nc_keypad(sp, flag);
	    _nc_set_screen(save_sp);
	    _nc_unlock_global(curses);
	} else
#endif
	{
#ifdef USE_TERM_DRIVER
	    rc = CallDriver_1(sp, td_kpad, flag);
	    if (rc == OK)
		sp->_keypad_on = flag;
#else
	    if (flag) {
		(void) NCURSES_PUTP2_FLUSH("keypad_xmit", keypad_xmit);
	    } else if (keypad_local) {
		(void) NCURSES_PUTP2_FLUSH("keypad_local", keypad_local);
	    }

	    if (flag && !sp->_tried) {
		_nc_init_keytry(sp);
		sp->_tried = TRUE;
	    }
	    sp->_keypad_on = flag;
	    rc = OK;
#endif
	}
    }
    return (rc);
}
