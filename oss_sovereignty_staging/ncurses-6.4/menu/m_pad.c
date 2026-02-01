 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_pad.c,v 1.17 2021/06/17 21:20:30 tom Exp $")

 
#define Refresh_Menu(menu) \
   if ( (menu) && ((menu)->status & _POSTED) )\
   {\
      _nc_Draw_Menu( menu );\
      _nc_Show_Menu( menu ); \
   }

 
MENU_EXPORT(int)
set_menu_pad(MENU *menu, int pad)
{
  bool do_refresh = (menu != (MENU *)0);

  T((T_CALLED("set_menu_pad(%p,%d)"), (void *)menu, pad));

  if (!isprint(UChar(pad)))
    RETURN(E_BAD_ARGUMENT);

  Normalize_Menu(menu);
  menu->pad = (unsigned char)pad;

  if (do_refresh)
    Refresh_Menu(menu);

  RETURN(E_OK);
}

 
MENU_EXPORT(int)
menu_pad(const MENU *menu)
{
  T((T_CALLED("menu_pad(%p)"), (const void *)menu));
  returnCode(Normalize_Menu(menu)->pad);
}

 
