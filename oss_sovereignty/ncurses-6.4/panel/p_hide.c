 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_hide.c,v 1.14 2020/09/26 18:02:35 tom Exp $")

PANEL_EXPORT(int)
hide_panel(register PANEL * pan)
{
  int err = ERR;

  T((T_CALLED("hide_panel(%p)"), (void *)pan));

  if (pan)
    {
      GetHook(pan);

      dBug(("--> hide_panel %s", USER_PTR(pan->user, 1)));
      dStack("<u%d>", 1, pan);

      HIDE_PANEL(pan, err, ERR);

      err = OK;

      dStack("<u%d>", 9, pan);
    }
  returnCode(err);
}
