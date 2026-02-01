 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_win.c,v 1.21 2021/06/17 21:20:30 tom Exp $")

 
MENU_EXPORT(int)
set_menu_win(MENU *menu, WINDOW *win)
{
  T((T_CALLED("set_menu_win(%p,%p)"), (void *)menu, (void *)win));

  if (menu)
    {
      if (menu->status & _POSTED)
	RETURN(E_POSTED);
      else
#if NCURSES_SP_FUNCS
	{
	   
	  SCREEN *sp = _nc_screen_of(menu->userwin);

	  menu->userwin = win ? win : sp->_stdscr;
	  _nc_Calculate_Item_Length_and_Width(menu);
	}
#else
	menu->userwin = win;
#endif
    }
  else
    _nc_Default_Menu.userwin = win;

  RETURN(E_OK);
}

 
MENU_EXPORT(WINDOW *)
menu_win(const MENU *menu)
{
  const MENU *m = Normalize_Menu(menu);

  T((T_CALLED("menu_win(%p)"), (const void *)menu));
  returnWin(Get_Menu_UserWin(m));
}

 
