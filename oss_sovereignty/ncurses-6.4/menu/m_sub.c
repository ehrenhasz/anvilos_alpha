 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_sub.c,v 1.16 2021/06/17 21:20:30 tom Exp $")

 
MENU_EXPORT(int)
set_menu_sub(MENU *menu, WINDOW *win)
{
  T((T_CALLED("set_menu_sub(%p,%p)"), (void *)menu, (void *)win));

  if (menu)
    {
      if (menu->status & _POSTED)
	RETURN(E_POSTED);
      else
#if NCURSES_SP_FUNCS
	{
	   
	  SCREEN *sp = _nc_screen_of(menu->usersub);

	  menu->usersub = win ? win : sp->_stdscr;
	  _nc_Calculate_Item_Length_and_Width(menu);
	}
#else
	menu->usersub = win;
#endif
    }
  else
    _nc_Default_Menu.usersub = win;

  RETURN(E_OK);
}

 
MENU_EXPORT(WINDOW *)
menu_sub(const MENU *menu)
{
  const MENU *m = Normalize_Menu(menu);

  T((T_CALLED("menu_sub(%p)"), (const void *)menu));
  returnWin(Get_Menu_Window(m));
}

 
