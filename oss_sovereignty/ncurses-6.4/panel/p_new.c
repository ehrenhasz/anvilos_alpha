 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_new.c,v 1.24 2021/10/23 15:12:06 tom Exp $")

#ifdef TRACE
static char *stdscr_id;
static char *new_id;

static PANEL *
AllocPanel(const char *name)
{
  PANEL *result = typeMalloc(PANEL, 1);

  _tracef("create :%s %p", name, (void *)result);
  return result;
}
#define InitUser(name) \
	if (!name ## _id) \
	    name ## _id = strdup(#name); \
	pan->user = name ## _id; \
	_tracef("create :user_ptr %p", pan->user)
#else
#define AllocPanel(name) typeMalloc(PANEL, 1)
#define InitUser(name) \
	  pan->user = (void *)0
#endif

 
static PANEL *
root_panel(NCURSES_SP_DCL0)
{
#if NCURSES_SP_FUNCS
  struct panelhook *ph = NCURSES_SP_NAME(_nc_panelhook) (sp);

#elif NO_LEAKS
  struct panelhook *ph = _nc_panelhook();
#endif

  if (_nc_stdscr_pseudo_panel == (PANEL *)0)
    {

      assert(SP_PARM && SP_PARM->_stdscr && !_nc_bottom_panel && !_nc_top_panel);
#if NO_LEAKS
      ph->destroy = del_panel;
#endif
      _nc_stdscr_pseudo_panel = AllocPanel("root_panel");
      if (_nc_stdscr_pseudo_panel != 0)
	{
	  PANEL *pan = _nc_stdscr_pseudo_panel;
	  WINDOW *win = SP_PARM->_stdscr;

	  pan->win = win;
	  pan->below = (PANEL *)0;
	  pan->above = (PANEL *)0;
	  InitUser(stdscr);
	  _nc_bottom_panel = _nc_top_panel = pan;
	}
    }
  return _nc_stdscr_pseudo_panel;
}

PANEL_EXPORT(PANEL *)
new_panel(WINDOW *win)
{
  PANEL *pan = (PANEL *)0;

  GetWindowHook(win);

  T((T_CALLED("new_panel(%p)"), (void *)win));

  if (!win)
    returnPanel(pan);

  if (!_nc_stdscr_pseudo_panel)
    (void)root_panel(NCURSES_SP_ARG);
  assert(_nc_stdscr_pseudo_panel);

  if ((pan = AllocPanel("new_panel")) != NULL)
    {
      pan->win = win;
      pan->above = (PANEL *)0;
      pan->below = (PANEL *)0;
      InitUser(new);
      (void)show_panel(pan);
    }
  returnPanel(pan);
}
