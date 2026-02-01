 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_opts.c,v 1.23 2020/12/12 00:38:08 tom Exp $")

 
MENU_EXPORT(int)
set_menu_opts(MENU *menu, Menu_Options opts)
{
  T((T_CALLED("set_menu_opts(%p,%d)"), (void *)menu, opts));

  opts &= ALL_MENU_OPTS;

  if (opts & ~ALL_MENU_OPTS)
    RETURN(E_BAD_ARGUMENT);

  if (menu)
    {
      if (menu->status & _POSTED)
	RETURN(E_POSTED);

      if ((opts & O_ROWMAJOR) != (menu->opt & O_ROWMAJOR))
	{
	   
	  if (menu->items && menu->items[0])
	    {
	      menu->toprow = 0;
	      menu->curitem = menu->items[0];
	      assert(menu->curitem);
	      set_menu_format(menu, menu->frows, menu->fcols);
	    }
	}

      menu->opt = opts;

      if (opts & O_ONEVALUE)
	{
	  ITEM **item;

	  if (((item = menu->items) != (ITEM **)0))
	    for (; *item; item++)
	      (*item)->value = FALSE;
	}

      if (opts & O_SHOWDESC)	 
	_nc_Calculate_Item_Length_and_Width(menu);
    }
  else
    _nc_Default_Menu.opt = opts;

  RETURN(E_OK);
}

 
MENU_EXPORT(int)
menu_opts_off(MENU *menu, Menu_Options opts)
{
  MENU *cmenu = menu;		 

  T((T_CALLED("menu_opts_off(%p,%d)"), (void *)menu, opts));

  opts &= ALL_MENU_OPTS;
  if (opts & ~ALL_MENU_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Menu(cmenu);
      opts = cmenu->opt & ~opts;
      returnCode(set_menu_opts(menu, opts));
    }
}

 
MENU_EXPORT(int)
menu_opts_on(MENU *menu, Menu_Options opts)
{
  MENU *cmenu = menu;		 

  T((T_CALLED("menu_opts_on(%p,%d)"), (void *)menu, opts));

  opts &= ALL_MENU_OPTS;
  if (opts & ~ALL_MENU_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Menu(cmenu);
      opts = cmenu->opt | opts;
      returnCode(set_menu_opts(menu, opts));
    }
}

 
MENU_EXPORT(Menu_Options)
menu_opts(const MENU *menu)
{
  T((T_CALLED("menu_opts(%p)"), (const void *)menu));
  returnMenuOpts(ALL_MENU_OPTS & Normalize_Menu(menu)->opt);
}

 
