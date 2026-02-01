 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_item_nam.c,v 1.19 2021/06/17 21:20:30 tom Exp $")

 
MENU_EXPORT(const char *)
item_name(const ITEM *item)
{
  T((T_CALLED("item_name(%p)"), (const void *)item));
  returnCPtr((item) ? item->name.str : (char *)0);
}

 
MENU_EXPORT(const char *)
item_description(const ITEM *item)
{
  T((T_CALLED("item_description(%p)"), (const void *)item));
  returnCPtr((item) ? item->description.str : (char *)0);
}

 
