 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_move.c,v 1.13 2020/05/24 01:40:20 anonymous.maarten Exp $")

PANEL_EXPORT(int)
move_panel(PANEL * pan, int starty, int startx)
{
  int rc = ERR;

  T((T_CALLED("move_panel(%p,%d,%d)"), (void *)pan, starty, startx));

  if (pan)
    {
      GetHook(pan);
      if (IS_LINKED(pan))
	{
	  Touchpan(pan);
	  PANEL_UPDATE(pan, (PANEL *) 0);
	}
      rc = mvwin(pan->win, starty, startx);
    }
  returnCode(rc);
}
