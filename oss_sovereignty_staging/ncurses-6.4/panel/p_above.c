 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_above.c,v 1.11 2020/05/24 01:40:20 anonymous.maarten Exp $")

#if NCURSES_SP_FUNCS
PANEL_EXPORT(PANEL *)
ground_panel(SCREEN * sp)
{
  T((T_CALLED("ground_panel(%p)"), (void *)sp));
  if (sp)
    {
      struct panelhook *ph = NCURSES_SP_NAME(_nc_panelhook) (sp);

      if (_nc_bottom_panel)	 
	returnPanel(_nc_bottom_panel->above);
      else
	returnPanel(0);
    }
  else
    {
      if (0 == CURRENT_SCREEN)
	returnPanel(0);
      else
	returnPanel(ground_panel(CURRENT_SCREEN));
    }
}
#endif

PANEL_EXPORT(PANEL *)
panel_above(const PANEL * pan)
{
  PANEL *result;

  T((T_CALLED("panel_above(%p)"), (const void *)pan));
  if (pan)
    result = pan->above;
  else
    {
#if NCURSES_SP_FUNCS
      result = ground_panel(CURRENT_SCREEN);
#else
       
      result = EMPTY_STACK()? (PANEL *) 0 : _nc_bottom_panel->above;
#endif
    }
  returnPanel(result);
}
