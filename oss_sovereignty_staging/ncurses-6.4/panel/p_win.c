 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_win.c,v 1.8 2020/05/24 01:40:20 anonymous.maarten Exp $")

PANEL_EXPORT(WINDOW *)
panel_window(const PANEL * pan)
{
  T((T_CALLED("panel_window(%p)"), (const void *)pan));
  returnWin(pan ? pan->win : (WINDOW *)0);
}
