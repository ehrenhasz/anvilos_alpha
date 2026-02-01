 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_below.c,v 1.11 2020/05/24 01:40:20 anonymous.maarten Exp $")

#if NCURSES_SP_FUNCS
PANEL_EXPORT(PANEL *)
ceiling_panel(SCREEN * sp)
{
  T((T_CALLED("ceiling_panel(%p)"), (void *)sp));
  if (sp)
    {
      struct panelhook *ph = NCURSES_SP_NAME(_nc_panelhook) (sp);

       
      returnPanel(EMPTY_STACK()? (PANEL *) 0 : _nc_top_panel);
    }
  else
    {
      if (0 == CURRENT_SCREEN)
	returnPanel(0);
      else
	returnPanel(ceiling_panel(CURRENT_SCREEN));
    }
}
#endif

PANEL_EXPORT(PANEL *)
panel_below(const PANEL * pan)
{
  PANEL *result;

  T((T_CALLED("panel_below(%p)"), (const void *)pan));
  if (pan)
    {
      GetHook(pan);
       
      result = Is_Pseudo(pan->below) ? (PANEL *) 0 : pan->below;
    }
  else
    {
#if NCURSES_SP_FUNCS
      result = ceiling_panel(CURRENT_SCREEN);
#else
       
      result = EMPTY_STACK()? (PANEL *) 0 : _nc_top_panel;
#endif
    }
  returnPanel(result);
}
