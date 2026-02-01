 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_bottom.c,v 1.17 2021/06/17 21:20:30 tom Exp $")

PANEL_EXPORT(int)
bottom_panel(PANEL * pan)
{
  int err = OK;

  T((T_CALLED("bottom_panel(%p)"), (void *)pan));
  if (pan)
    {
      GetHook(pan);
      if (!Is_Bottom(pan))
	{

	  dBug(("--> bottom_panel %s", USER_PTR(pan->user, 1)));

	  HIDE_PANEL(pan, err, OK);
	  assert(_nc_bottom_panel == _nc_stdscr_pseudo_panel);

	  dStack("<lb%d>", 1, pan);

	  pan->below = _nc_bottom_panel;
	  pan->above = _nc_bottom_panel->above;
	  if (pan->above)
	    pan->above->below = pan;
	  _nc_bottom_panel->above = pan;

	  dStack("<lb%d>", 9, pan);
	}
    }
  else
    err = ERR;

  returnCode(err);
}
