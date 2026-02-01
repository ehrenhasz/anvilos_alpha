 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_item_vis.c,v 1.20 2021/06/17 21:20:30 tom Exp $")

 
MENU_EXPORT(bool)
item_visible(const ITEM *item)
{
  MENU *menu;

  T((T_CALLED("item_visible(%p)"), (const void *)item));
  if (item &&
      (menu = item->imenu) &&
      (menu->status & _POSTED) &&
      ((menu->toprow + menu->arows) > (item->y)) &&
      (item->y >= menu->toprow))
    returnBool(TRUE);
  else
    returnBool(FALSE);
}

 
