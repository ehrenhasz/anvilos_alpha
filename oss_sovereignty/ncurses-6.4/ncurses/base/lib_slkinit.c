 

 

 
#include <curses.priv.h>

MODULE_ID("$Id: lib_slkinit.c,v 1.16 2022/07/09 18:58:58 tom Exp $")

#ifdef USE_SP_RIPOFF
#define SoftkeyFormat SP_PARM->slk_format
#else
#define SoftkeyFormat _nc_globals.slk_format
#endif

NCURSES_EXPORT(int)
NCURSES_SP_NAME(slk_init) (NCURSES_SP_DCLx int format)
{
    int code = ERR;

    START_TRACE();
    T((T_CALLED("slk_init(%p,%d)"), (void *) SP_PARM, format));

    if (format >= 0
	&& format <= 3
#ifdef USE_SP_RIPOFF
	&& SP_PARM
	&& SP_PARM->_prescreen
#endif
	&& !SoftkeyFormat) {
	SoftkeyFormat = 1 + format;
	code = NCURSES_SP_NAME(_nc_ripoffline) (NCURSES_SP_ARGx
						-SLK_LINES(SoftkeyFormat),
						_nc_slk_initialize);
    }
    returnCode(code);
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
slk_init(int format)
{
    int rc;

    _nc_init_pthreads();
    _nc_lock_global(prescreen);
    START_TRACE();
    rc = NCURSES_SP_NAME(slk_init) (CURRENT_SCREEN_PRE, format);
    _nc_unlock_global(prescreen);

    return rc;
}
#endif
