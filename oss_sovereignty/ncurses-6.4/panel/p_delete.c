 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_delete.c,v 1.17 2020/12/26 18:25:34 tom Exp $")

PANEL_EXPORT(int)
del_panel(PANEL *pan)
{
  int err = OK;

  T((T_CALLED("del_panel(%p)"), (void *)pan));
  if (pan)
    {
      GetHook(pan);
      HIDE_PANEL(pan, err, OK);
      dBug(("...discard ptr=%s", USER_PTR(pan->user, 1)));
      dBug(("...deleted pan=%p", (void *)pan));
      free((void *)pan);
    }
  else
    err = ERR;

  returnCode(err);
}
