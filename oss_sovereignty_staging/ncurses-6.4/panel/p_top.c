 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_top.c,v 1.8 2020/05/24 01:40:20 anonymous.maarten Exp $")

PANEL_EXPORT(int)
top_panel(PANEL * pan)
{
  T((T_CALLED("top_panel(%p)"), (void *)pan));
  returnCode(show_panel(pan));
}
