 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_hidden.c,v 1.11 2020/05/24 01:40:20 anonymous.maarten Exp $")

PANEL_EXPORT(int)
panel_hidden(const PANEL * pan)
{
  int rc = ERR;

  T((T_CALLED("panel_hidden(%p)"), (const void *)pan));
  if (pan)
    {
      GetHook(pan);
      rc = (IS_LINKED(pan) ? FALSE : TRUE);
    }
  returnCode(rc);
}
