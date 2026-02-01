 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_item_cur.c,v 1.22 2021/06/17 21:20:30 tom Exp $")

 
MENU_EXPORT(int)
set_current_item(MENU *menu, ITEM *item)
{
  T((T_CALLED("set_current_item(%p,%p)"), (void *)menu, (void *)item));

  if (menu && item && (item->imenu == menu))
    {
      if (menu->status & _IN_DRIVER)
	RETURN(E_BAD_STATE);

      assert(menu->curitem);
      if (item != menu->curitem)
	{
	  if (menu->status & _LINK_NEEDED)
	    {
	       
	      _nc_Link_Items(menu);
	    }
	  assert(menu->pattern);
	  Reset_Pattern(menu);
	   
	  Adjust_Current_Item(menu, menu->toprow, item);
	}
    }
  else
    RETURN(E_BAD_ARGUMENT);

  RETURN(E_OK);
}

 
MENU_EXPORT(ITEM *)
current_item(const MENU *menu)
{
  T((T_CALLED("current_item(%p)"), (const void *)menu));
  returnItem((menu && menu->items) ? menu->curitem : (ITEM *)0);
}

 
MENU_EXPORT(int)
item_index(const ITEM *item)
{
  T((T_CALLED("item_index(%p)"), (const void *)item));
  returnCode((item && item->imenu) ? item->index : ERR);
}

 
