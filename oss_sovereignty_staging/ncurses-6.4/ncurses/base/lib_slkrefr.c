 

 

 
#include <curses.priv.h>

#ifndef CUR
#define CUR SP_TERMTYPE
#endif

MODULE_ID("$Id: lib_slkrefr.c,v 1.32 2021/09/04 10:54:35 tom Exp $")

#ifdef USE_TERM_DRIVER
#define NumLabels    InfoOf(SP_PARM).numlabels
#else
#define NumLabels    num_labels
#endif

 
static void
slk_paint_info(WINDOW *win)
{
    SCREEN *sp = _nc_screen_of(win);

    if (win && sp && (sp->slk_format == 4)) {
	int i;

	(void) mvwhline(win, 0, 0, 0, getmaxx(win));
	wmove(win, 0, 0);

	for (i = 0; i < sp->_slk->maxlab; i++) {
	    mvwprintw(win, 0, sp->_slk->ent[i].ent_x, "F%d", i + 1);
	}
    }
}

 
static void
slk_intern_refresh(SCREEN *sp)
{
    int i;
    int fmt;
    SLK *slk;
    int numlab;

    if (sp == 0)
	return;

    slk = sp->_slk;
    fmt = sp->slk_format;
    numlab = NumLabels;

    if (slk->hidden)
	return;

    for (i = 0; i < slk->labcnt; i++) {
	if (slk->dirty || slk->ent[i].dirty) {
	    if (slk->ent[i].visible) {
		if (numlab > 0 && SLK_STDFMT(fmt)) {
#ifdef USE_TERM_DRIVER
		    CallDriver_2(sp, td_hwlabel, i + 1, slk->ent[i].form_text);
#else
		    if (i < num_labels) {
			NCURSES_PUTP2("plab_norm",
				      TPARM_2(plab_norm,
					      i + 1,
					      slk->ent[i].form_text));
		    }
#endif
		} else {
		    if (fmt == 4)
			slk_paint_info(slk->win);
		    wmove(slk->win, SLK_LINES(fmt) - 1, slk->ent[i].ent_x);
		    (void) wattrset(slk->win, (int) AttrOf(slk->attr));
		    waddstr(slk->win, slk->ent[i].form_text);
		     
		    (void) wattrset(slk->win, (int) WINDOW_ATTRS(StdScreen(sp)));
		}
	    }
	    slk->ent[i].dirty = FALSE;
	}
    }
    slk->dirty = FALSE;

    if (numlab > 0) {
#ifdef USE_TERM_DRIVER
	CallDriver_1(sp, td_hwlabelOnOff, slk->hidden ? FALSE : TRUE);
#else
	if (slk->hidden) {
	    NCURSES_PUTP2("label_off", label_off);
	} else {
	    NCURSES_PUTP2("label_on", label_on);
	}
#endif
    }
}

 
NCURSES_EXPORT(int)
NCURSES_SP_NAME(slk_noutrefresh) (NCURSES_SP_DCL0)
{
    T((T_CALLED("slk_noutrefresh(%p)"), (void *) SP_PARM));

    if (SP_PARM == 0 || SP_PARM->_slk == 0)
	returnCode(ERR);
    if (SP_PARM->_slk->hidden)
	returnCode(OK);
    slk_intern_refresh(SP_PARM);

    returnCode(wnoutrefresh(SP_PARM->_slk->win));
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
slk_noutrefresh(void)
{
    return NCURSES_SP_NAME(slk_noutrefresh) (CURRENT_SCREEN);
}
#endif

 
NCURSES_EXPORT(int)
NCURSES_SP_NAME(slk_refresh) (NCURSES_SP_DCL0)
{
    T((T_CALLED("slk_refresh(%p)"), (void *) SP_PARM));

    if (SP_PARM == 0 || SP_PARM->_slk == 0)
	returnCode(ERR);
    if (SP_PARM->_slk->hidden)
	returnCode(OK);
    slk_intern_refresh(SP_PARM);

    returnCode(wrefresh(SP_PARM->_slk->win));
}

#if NCURSES_SP_FUNCS
NCURSES_EXPORT(int)
slk_refresh(void)
{
    return NCURSES_SP_NAME(slk_refresh) (CURRENT_SCREEN);
}
#endif
