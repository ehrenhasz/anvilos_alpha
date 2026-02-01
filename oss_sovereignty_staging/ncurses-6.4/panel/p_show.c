 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_show.c,v 1.17 2021/06/17 21:20:30 tom Exp $")

PANEL_EXPORT(int)
show_panel(PANEL * pan)
{
  int err = ERR;

  T((T_CALLED("show_panel(%p)"), (void *)pan));

  if (pan)
    {
      GetHook(pan);

      if (Is_Top(pan))
	returnCode(OK);

      dBug(("--> show_panel %s", USER_PTR(pan->user, 1)));

      HIDE_PANEL(pan, err, OK);

      dStack("<lt%d>", 1, pan);
      assert(_nc_bottom_panel == _nc_stdscr_pseudo_panel);

      _nc_top_panel->above = pan;
      pan->below = _nc_top_panel;
      pan->above = (PANEL *) 0;
      _nc_top_panel = pan;

      err = OK;

      dStack("<lt%d>", 9, pan);
    }
  returnCode(err);
}
