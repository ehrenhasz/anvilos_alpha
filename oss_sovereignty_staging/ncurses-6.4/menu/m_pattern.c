 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_pattern.c,v 1.20 2021/06/17 21:20:30 tom Exp $")

 
MENU_EXPORT(char *)
menu_pattern(const MENU *menu)
{
  static char empty[] = "";

  T((T_CALLED("menu_pattern(%p)"), (const void *)menu));
  returnPtr(menu ? (menu->pattern ? menu->pattern : empty) : 0);
}

 
MENU_EXPORT(int)
set_menu_pattern(MENU *menu, const char *p)
{
  ITEM *matchitem;
  int matchpos;

  T((T_CALLED("set_menu_pattern(%p,%s)"), (void *)menu, _nc_visbuf(p)));

  if (!menu || !p)
    RETURN(E_BAD_ARGUMENT);

  if (!(menu->items))
    RETURN(E_NOT_CONNECTED);

  if (menu->status & _IN_DRIVER)
    RETURN(E_BAD_STATE);

  Reset_Pattern(menu);

  if (!(*p))
    {
      pos_menu_cursor(menu);
      RETURN(E_OK);
    }

  if (menu->status & _LINK_NEEDED)
    _nc_Link_Items(menu);

  matchpos = menu->toprow;
  matchitem = menu->curitem;
  assert(matchitem);

  while (*p)
    {
      if (!isprint(UChar(*p)) ||
	  (_nc_Match_Next_Character_In_Item_Name(menu, *p, &matchitem) != E_OK))
	{
	  Reset_Pattern(menu);
	  pos_menu_cursor(menu);
	  RETURN(E_NO_MATCH);
	}
      p++;
    }

   
  Adjust_Current_Item(menu, matchpos, matchitem);
  RETURN(E_OK);
}

 
