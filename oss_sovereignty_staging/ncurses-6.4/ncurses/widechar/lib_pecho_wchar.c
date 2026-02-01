 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_pecho_wchar.c,v 1.4 2021/10/23 17:07:56 tom Exp $")

NCURSES_EXPORT(int)
pecho_wchar(WINDOW *pad, const cchar_t *wch)
{
    T((T_CALLED("pecho_wchar(%p, %s)"), (void *) pad, _tracech_t(wch)));

    if (pad == 0)
	returnCode(ERR);

    if (!IS_PAD(pad))
	returnCode(wecho_wchar(pad, wch));

    wadd_wch(pad, wch);
    prefresh(pad, pad->_pad._pad_y,
	     pad->_pad._pad_x,
	     pad->_pad._pad_top,
	     pad->_pad._pad_left,
	     pad->_pad._pad_bottom,
	     pad->_pad._pad_right);

    returnCode(OK);
}
