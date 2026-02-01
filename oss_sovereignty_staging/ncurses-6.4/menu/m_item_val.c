 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_item_val.c,v 1.20 2021/06/17 21:11:08 tom Exp $")

 
MENU_EXPORT(int)
set_item_value(ITEM *item, bool value)
{
  T((T_CALLED("set_item_value(%p,%d)"), (void *)item, value));
  if (item)
    {
      MENU *menu = item->imenu;

      if ((!(item->opt & O_SELECTABLE)) ||
	  (menu && (menu->opt & O_ONEVALUE)))
	RETURN(E_REQUEST_DENIED);

      if (item->value ^ value)
	{
	  item->value = value ? TRUE : FALSE;
	  if (menu)
	    {
	      if (menu->status & _POSTED)
		{
		  Move_And_Post_Item(menu, item);
		  _nc_Show_Menu(menu);
		}
	    }
	}
    }
  else
    _nc_Default_Item.value = value;

  RETURN(E_OK);
}

 
MENU_EXPORT(bool)
item_value(const ITEM *item)
{
  T((T_CALLED("item_value(%p)"), (const void *)item));
  returnBool((Normalize_Item(item)->value) ? TRUE : FALSE);
}

 
