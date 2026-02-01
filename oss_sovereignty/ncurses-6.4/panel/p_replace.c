 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_replace.c,v 1.13 2020/05/24 01:40:20 anonymous.maarten Exp $")

PANEL_EXPORT(int)
replace_panel(PANEL * pan, WINDOW *win)
{
  int rc = ERR;

  T((T_CALLED("replace_panel(%p,%p)"), (void *)pan, (void *)win));

  if (pan)
    {
      GetHook(pan);
      if (IS_LINKED(pan))
	{
	  Touchpan(pan);
	  PANEL_UPDATE(pan, (PANEL *) 0);
	}
      pan->win = win;
      rc = OK;
    }
  returnCode(rc);
}
