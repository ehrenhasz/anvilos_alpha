 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_userptr.c,v 1.21 2020/12/12 00:38:14 tom Exp $")

 
MENU_EXPORT(int)
set_menu_userptr(MENU *menu, void *userptr)
{
  T((T_CALLED("set_menu_userptr(%p,%p)"), (void *)menu, (void *)userptr));
  Normalize_Menu(menu)->userptr = userptr;
  RETURN(E_OK);
}

 
MENU_EXPORT(void *)
menu_userptr(const MENU *menu)
{
  T((T_CALLED("menu_userptr(%p)"), (const void *)menu));
  returnVoidPtr(Normalize_Menu(menu)->userptr);
}

 
