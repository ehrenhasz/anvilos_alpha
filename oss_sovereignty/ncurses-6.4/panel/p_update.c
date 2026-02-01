 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_update.c,v 1.13 2020/05/24 01:40:20 anonymous.maarten Exp $")

PANEL_EXPORT(void)
NCURSES_SP_NAME(update_panels) (NCURSES_SP_DCL0)
{
  PANEL *pan;

  T((T_CALLED("update_panels(%p)"), (void *)SP_PARM));
  dBug(("--> update_panels"));

  if (SP_PARM)
    {
      GetScreenHook(SP_PARM);

      pan = _nc_bottom_panel;
      while (pan && pan->above)
	{
	  PANEL_UPDATE(pan, pan->above);
	  pan = pan->above;
	}

      pan = _nc_bottom_panel;
      while (pan)
	{
	  Wnoutrefresh(pan);
	  pan = pan->above;
	}
    }

  returnVoid;
}

#if NCURSES_SP_FUNCS
PANEL_EXPORT(void)
update_panels(void)
{
  NCURSES_SP_NAME(update_panels) (CURRENT_SCREEN);
}
#endif
