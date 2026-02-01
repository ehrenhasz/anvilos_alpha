 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_item_use.c,v 1.21 2020/12/12 00:38:08 tom Exp $")

 
MENU_EXPORT(int)
set_item_userptr(ITEM *item, void *userptr)
{
  T((T_CALLED("set_item_userptr(%p,%p)"), (void *)item, (void *)userptr));
  Normalize_Item(item)->userptr = userptr;
  RETURN(E_OK);
}

 
MENU_EXPORT(void *)
item_userptr(const ITEM *item)
{
  T((T_CALLED("item_userptr(%p)"), (const void *)item));
  returnVoidPtr(Normalize_Item(item)->userptr);
}

 
