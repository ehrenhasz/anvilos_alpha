 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_cursor.c,v 1.26 2021/03/27 23:46:29 tom Exp $")

 
MENU_EXPORT(int)
_nc_menu_cursor_pos(const MENU *menu, const ITEM *item, int *pY, int *pX)
{
  if (!menu || !pX || !pY)
    return (E_BAD_ARGUMENT);
  else
    {
      if ((ITEM *)0 == item)
	item = menu->curitem;
      assert(item != (ITEM *)0);

      if (!(menu->status & _POSTED))
	return (E_NOT_POSTED);

      *pX = item->x * (menu->spc_cols + menu->itemlen);
      *pY = (item->y - menu->toprow) * menu->spc_rows;
    }
  return (E_OK);
}

 
MENU_EXPORT(int)
pos_menu_cursor(const MENU *menu)
{
  int x = 0, y = 0;
  int err = _nc_menu_cursor_pos(menu, (ITEM *)0, &y, &x);

  T((T_CALLED("pos_menu_cursor(%p)"), (const void *)menu));

  if (E_OK == err)
    {
      WINDOW *win = Get_Menu_UserWin(menu);
      WINDOW *sub = menu->usersub ? menu->usersub : win;

      assert(win && sub);

      if ((menu->opt & O_SHOWMATCH) && (menu->pindex > 0))
	x += (menu->pindex + menu->marklen - 1);

      wmove(sub, y, x);

      if (win != sub)
	{
	  wcursyncup(sub);
	  wsyncup(sub);
	  untouchwin(sub);
	}
    }
  RETURN(err);
}

 
