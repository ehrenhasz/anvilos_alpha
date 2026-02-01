 

 

 

#include <curses.priv.h>

MODULE_ID("$Id: lib_ungetch.c,v 1.17 2020/02/02 23:34:34 tom Exp $")

#include <fifo_defs.h>

#ifdef TRACE
NCURSES_EXPORT(void)
_nc_fifo_dump(SCREEN *sp)
{
    int i;
    T(("head = %d, tail = %d, peek = %d", head, tail, peek));
    for (i = 0; i < 10; i++)
	T(("char %d = %s", i, _nc_tracechar(sp, sp->_fifo[i])));
}
#endif  

NCURSES_EXPORT(int)
safe_ungetch(SCREEN *sp, int ch)
{
    int rc = ERR;

    T((T_CALLED("ungetch(%p,%s)"), (void *) sp, _nc_tracechar(sp, ch)));

    if (sp != 0 && tail >= 0) {
	if (head < 0) {
	    head = 0;
	    t_inc();
	    peek = tail;	 
	} else {
	    h_dec();
	}

	sp->_fifo[head] = ch;
	T(("ungetch %s ok", _nc_tracechar(sp, ch)));
#ifdef TRACE
	if (USE_TRACEF(TRACE_IEVENT)) {
	    _nc_fifo_dump(sp);
	    _nc_unlock_global(tracef);
	}
#endif
	rc = OK;
    }
    returnCode(rc);
}

NCURSES_EXPORT(int)
ungetch(int ch)
{
    return safe_ungetch(CURRENT_SCREEN, ch);
}
