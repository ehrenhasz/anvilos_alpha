 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: p_user.c,v 1.11 2021/06/17 21:20:30 tom Exp $")

PANEL_EXPORT(int)
set_panel_userptr(PANEL * pan, NCURSES_CONST void *uptr)
{
  T((T_CALLED("set_panel_userptr(%p,%p)"), (void *)pan, (NCURSES_CONST void *)uptr));
  if (!pan)
    returnCode(ERR);
  pan->user = uptr;
  returnCode(OK);
}

PANEL_EXPORT(NCURSES_CONST void *)
panel_userptr(const PANEL * pan)
{
  T((T_CALLED("panel_userptr(%p)"), (const void *)pan));
  returnCVoidPtr(pan ? pan->user : (NCURSES_CONST void *)0);
}
