 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_items.c,v 1.21 2021/06/17 21:20:30 tom Exp $")

 
MENU_EXPORT(int)
set_menu_items(MENU *menu, ITEM **items)
{
  T((T_CALLED("set_menu_items(%p,%p)"), (void *)menu, (void *)items));

  if (!menu || (items && !(*items)))
    RETURN(E_BAD_ARGUMENT);

  if (menu->status & _POSTED)
    RETURN(E_POSTED);

  if (menu->items)
    _nc_Disconnect_Items(menu);

  if (items)
    {
      if (!_nc_Connect_Items(menu, items))
	RETURN(E_CONNECTED);
    }

  menu->items = items;
  RETURN(E_OK);
}

 
MENU_EXPORT(ITEM **)
menu_items(const MENU *menu)
{
  T((T_CALLED("menu_items(%p)"), (const void *)menu));
  returnItemPtr(menu ? menu->items : (ITEM **)0);
}

 
MENU_EXPORT(int)
item_count(const MENU *menu)
{
  T((T_CALLED("item_count(%p)"), (const void *)menu));
  returnCode(menu ? menu->nitems : -1);
}

 
