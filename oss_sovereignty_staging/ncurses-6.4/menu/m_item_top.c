 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_item_top.c,v 1.16 2021/06/17 21:11:08 tom Exp $")

 
MENU_EXPORT(int)
set_top_row(MENU *menu, int row)
{
  T((T_CALLED("set_top_row(%p,%d)"), (void *)menu, row));

  if (menu)
    {
      if (menu->status & _IN_DRIVER)
	RETURN(E_BAD_STATE);
      if (menu->items == (ITEM **)0)
	RETURN(E_NOT_CONNECTED);

      if ((row < 0) || (row > (menu->rows - menu->arows)))
	RETURN(E_BAD_ARGUMENT);
    }
  else
    RETURN(E_BAD_ARGUMENT);

  if (row != menu->toprow)
    {
      ITEM *item;

      if (menu->status & _LINK_NEEDED)
	_nc_Link_Items(menu);

      item = menu->items[(menu->opt & O_ROWMAJOR) ? (row * menu->cols) : row];
      assert(menu->pattern);
      Reset_Pattern(menu);
      _nc_New_TopRow_and_CurrentItem(menu, row, item);
    }

  RETURN(E_OK);
}

 
MENU_EXPORT(int)
top_row(const MENU *menu)
{
  T((T_CALLED("top_row(%p)"), (const void *)menu));
  if (menu && menu->items && *(menu->items))
    {
      assert((menu->toprow >= 0) && (menu->toprow < menu->rows));
      returnCode(menu->toprow);
    }
  else
    returnCode(ERR);
}

 
