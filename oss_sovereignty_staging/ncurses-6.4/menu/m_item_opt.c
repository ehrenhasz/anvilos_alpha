 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_item_opt.c,v 1.22 2021/06/17 21:20:30 tom Exp $")

 
MENU_EXPORT(int)
set_item_opts(ITEM *item, Item_Options opts)
{
  T((T_CALLED("set_menu_opts(%p,%d)"), (void *)item, opts));

  opts &= ALL_ITEM_OPTS;

  if (opts & ~ALL_ITEM_OPTS)
    RETURN(E_BAD_ARGUMENT);

  if (item)
    {
      if (item->opt != opts)
	{
	  MENU *menu = item->imenu;

	  item->opt = opts;

	  if ((!(opts & O_SELECTABLE)) && item->value)
	    item->value = FALSE;

	  if (menu && (menu->status & _POSTED))
	    {
	      Move_And_Post_Item(menu, item);
	      _nc_Show_Menu(menu);
	    }
	}
    }
  else
    _nc_Default_Item.opt = opts;

  RETURN(E_OK);
}

 
MENU_EXPORT(int)
item_opts_off(ITEM *item, Item_Options opts)
{
  ITEM *citem = item;		 

  T((T_CALLED("item_opts_off(%p,%d)"), (void *)item, opts));

  if (opts & ~ALL_ITEM_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Item(citem);
      opts = citem->opt & ~(opts & ALL_ITEM_OPTS);
      returnCode(set_item_opts(item, opts));
    }
}

 
MENU_EXPORT(int)
item_opts_on(ITEM *item, Item_Options opts)
{
  ITEM *citem = item;		 

  T((T_CALLED("item_opts_on(%p,%d)"), (void *)item, opts));

  opts &= ALL_ITEM_OPTS;
  if (opts & ~ALL_ITEM_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Item(citem);
      opts = citem->opt | opts;
      returnCode(set_item_opts(item, opts));
    }
}

 
MENU_EXPORT(Item_Options)
item_opts(const ITEM *item)
{
  T((T_CALLED("item_opts(%p)"), (const void *)item));
  returnItemOpts(ALL_ITEM_OPTS & Normalize_Item(item)->opt);
}

 
