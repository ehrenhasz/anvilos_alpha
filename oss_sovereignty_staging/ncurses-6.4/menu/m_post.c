 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_post.c,v 1.38 2022/09/24 09:38:44 tom Exp $")

 
MENU_EXPORT(void)
_nc_Post_Item(const MENU *menu, const ITEM *item)
{
  int i;
  chtype ch;
  int item_x, item_y;
  int count = 0;
  bool isfore = FALSE, isback = FALSE, isgrey = FALSE;
  int name_len;

  assert(menu->win);

  getyx(menu->win, item_y, item_x);

   
  wattron(menu->win, (int)menu->back);
  if (item->value || (item == menu->curitem))
    {
      if (menu->marklen)
	{
	   
	  if (!(menu->opt & O_ONEVALUE) && item->value && item != menu->curitem)
	    {
	      wattron(menu->win, (int)menu->fore);
	      isfore = TRUE;
	    }
	  waddstr(menu->win, menu->mark);
	  if (isfore)
	    {
	      wattron(menu->win, (int)menu->fore);
	      isfore = FALSE;
	    }
	}
    }
  else				 
    for (ch = ' ', i = menu->marklen; i > 0; i--)
      waddch(menu->win, ch);
  wattroff(menu->win, (int)menu->back);
  count += menu->marklen;

   
  if (!(item->opt & O_SELECTABLE))
    {
      wattron(menu->win, (int)menu->grey);
      isgrey = TRUE;
    }
  else
    {
      if (item->value || item == menu->curitem)
	{
	  wattron(menu->win, (int)menu->fore);
	  isfore = TRUE;
	}
      else
	{
	  wattron(menu->win, (int)menu->back);
	  isback = TRUE;
	}
    }

  waddnstr(menu->win, item->name.str, item->name.length);
  name_len = _nc_Calculate_Text_Width(&(item->name));
  for (ch = ' ', i = menu->namelen - name_len; i > 0; i--)
    {
      waddch(menu->win, ch);
    }
  count += menu->namelen;

   
  if ((menu->opt & O_SHOWDESC) && menu->desclen > 0)
    {
      int m = menu->spc_desc / 2;
      int cy = -1, cx = -1;
      int desc_len;

      for (ch = ' ', i = 0; i < menu->spc_desc; i++)
	{
	  if (i == m)
	    {
	      waddch(menu->win, menu->pad);
	      getyx(menu->win, cy, cx);
	    }
	  else
	    waddch(menu->win, ch);
	}
      if (item->description.length)
	waddnstr(menu->win, item->description.str, item->description.length);
      desc_len = _nc_Calculate_Text_Width(&(item->description));
      for (ch = ' ', i = menu->desclen - desc_len; i > 0; i--)
	{
	  waddch(menu->win, ch);
	}
      count += menu->desclen + menu->spc_desc;

      if (menu->spc_rows > 1)
	{
	  int j, k, ncy, ncx;

	  assert(cx >= 0 && cy >= 0);
	  getyx(menu->win, ncy, ncx);
	  if (isgrey)
	    wattroff(menu->win, (int)menu->grey);
	  else if (isfore)
	    wattroff(menu->win, (int)menu->fore);
	  wattron(menu->win, (int)menu->back);
	  for (j = 1; j < menu->spc_rows; j++)
	    {
	      if ((item_y + j) < getmaxy(menu->win))
		{
		  wmove(menu->win, item_y + j, item_x);
		  for (k = 0; k < count; k++)
		    waddch(menu->win, ' ');
		}
	      if ((cy + j) < getmaxy(menu->win))
		(void)mvwaddch(menu->win, cy + j, cx - 1, menu->pad);
	    }
	  wmove(menu->win, ncy, ncx);
	  if (!isback)
	    wattroff(menu->win, (int)menu->back);
	}
    }

   
  if (isfore)
    wattroff(menu->win, (int)menu->fore);
  if (isback)
    wattroff(menu->win, (int)menu->back);
  if (isgrey)
    wattroff(menu->win, (int)menu->grey);
}

 
MENU_EXPORT(void)
_nc_Draw_Menu(const MENU *menu)
{
  ITEM *item = menu->items[0];
  ITEM *lastvert;
  ITEM *hitem;
  chtype s_bkgd;

  assert(item && menu->win);

  s_bkgd = getbkgd(menu->win);
  wbkgdset(menu->win, menu->back);
  werase(menu->win);
  wbkgdset(menu->win, s_bkgd);

  lastvert = (menu->opt & O_NONCYCLIC) ? (ITEM *)0 : item;

  if (item != NULL)
    {
      int y = 0;

      do
	{
	  ITEM *lasthor;

	  wmove(menu->win, y, 0);

	  hitem = item;
	  lasthor = (menu->opt & O_NONCYCLIC) ? (ITEM *)0 : hitem;

	  do
	    {
	      _nc_Post_Item(menu, hitem);

	      wattron(menu->win, (int)menu->back);
	      if (((hitem = hitem->right) != lasthor) && hitem)
		{
		  int i, j, cy, cx;
		  chtype ch = ' ';

		  getyx(menu->win, cy, cx);
		  for (j = 0; j < menu->spc_rows; j++)
		    {
		      wmove(menu->win, cy + j, cx);
		      for (i = 0; i < menu->spc_cols; i++)
			{
			  waddch(menu->win, ch);
			}
		    }
		  wmove(menu->win, cy, cx + menu->spc_cols);
		}
	    }
	  while (hitem && (hitem != lasthor));
	  wattroff(menu->win, (int)menu->back);

	  item = item->down;
	  y += menu->spc_rows;

	}
      while (item && (item != lastvert));
    }
}

 
MENU_EXPORT(int)
post_menu(MENU *menu)
{
  T((T_CALLED("post_menu(%p)"), (void *)menu));

  if (!menu)
    RETURN(E_BAD_ARGUMENT);

  if (menu->status & _IN_DRIVER)
    RETURN(E_BAD_STATE);

  if (menu->status & _POSTED)
    RETURN(E_POSTED);

  if (menu->items && *(menu->items))
    {
      int h = 1 + menu->spc_rows * (menu->rows - 1);

      WINDOW *win = Get_Menu_Window(menu);
      int maxy = getmaxy(win);

      if ((menu->win = newpad(h, menu->width)))
	{
	  int y = (maxy >= h) ? h : maxy;

	  if (y >= menu->height)
	    y = menu->height;
	  if (!(menu->sub = subpad(menu->win, y, menu->width, 0, 0)))
	    RETURN(E_SYSTEM_ERROR);
	}
      else
	RETURN(E_SYSTEM_ERROR);

      if (menu->status & _LINK_NEEDED)
	_nc_Link_Items(menu);
    }
  else
    RETURN(E_NOT_CONNECTED);

  SetStatus(menu, _POSTED);

  if (!(menu->opt & O_ONEVALUE))
    {
      ITEM **items;

      for (items = menu->items; *items; items++)
	{
	  (*items)->value = FALSE;
	}
    }

  _nc_Draw_Menu(menu);

  Call_Hook(menu, menuinit);
  Call_Hook(menu, iteminit);

  _nc_Show_Menu(menu);

  RETURN(E_OK);
}

 
MENU_EXPORT(int)
unpost_menu(MENU *menu)
{
  WINDOW *win;

  T((T_CALLED("unpost_menu(%p)"), (void *)menu));

  if (!menu)
    RETURN(E_BAD_ARGUMENT);

  if (menu->status & _IN_DRIVER)
    RETURN(E_BAD_STATE);

  if (!(menu->status & _POSTED))
    RETURN(E_NOT_POSTED);

  Call_Hook(menu, itemterm);
  Call_Hook(menu, menuterm);

  win = Get_Menu_Window(menu);
  werase(win);
  wsyncup(win);

  assert(menu->sub);
  delwin(menu->sub);
  menu->sub = (WINDOW *)0;

  assert(menu->win);
  delwin(menu->win);
  menu->win = (WINDOW *)0;

  ClrStatus(menu, _POSTED);

  RETURN(E_OK);
}

 
