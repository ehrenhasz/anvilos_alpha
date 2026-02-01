 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_new.c,v 1.27 2021/06/17 21:26:02 tom Exp $")

 
MENU_EXPORT(MENU *)
NCURSES_SP_NAME(new_menu) (NCURSES_SP_DCLx ITEM **items)
{
  int err = E_SYSTEM_ERROR;
  MENU *menu = typeCalloc(MENU, 1);

  T((T_CALLED("new_menu(%p,%p)"), (void *)SP_PARM, (void *)items));
  if (menu)
    {
      T((T_CREATE("menu %p"), (void *)menu));
      *menu = _nc_Default_Menu;
      menu->status = 0;
      menu->rows = menu->frows;
      menu->cols = menu->fcols;
#if NCURSES_SP_FUNCS
       
      menu->userwin = SP_PARM->_stdscr;
      menu->usersub = SP_PARM->_stdscr;
#endif
      if (items && *items)
	{
	  if (!_nc_Connect_Items(menu, items))
	    {
	      err = E_NOT_CONNECTED;
	      free(menu);
	      menu = (MENU *)0;
	    }
	  else
	    err = E_OK;
	}
    }

  if (!menu)
    SET_ERROR(err);

  returnMenu(menu);
}

 
#if NCURSES_SP_FUNCS
MENU_EXPORT(MENU *)
new_menu(ITEM **items)
{
  return NCURSES_SP_NAME(new_menu) (CURRENT_SCREEN, items);
}
#endif

 
MENU_EXPORT(int)
free_menu(MENU *menu)
{
  T((T_CALLED("free_menu(%p)"), (void *)menu));
  if (!menu)
    RETURN(E_BAD_ARGUMENT);

  if (menu->status & _POSTED)
    RETURN(E_POSTED);

  if (menu->items)
    _nc_Disconnect_Items(menu);

  if ((menu->status & _MARK_ALLOCATED) && menu->mark)
    free(menu->mark);

  free(menu);
  RETURN(E_OK);
}

 
