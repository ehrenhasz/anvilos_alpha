 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_scale.c,v 1.14 2021/06/17 21:20:30 tom Exp $")

 
MENU_EXPORT(int)
scale_menu(const MENU *menu, int *rows, int *cols)
{
  T((T_CALLED("scale_menu(%p,%p,%p)"),
     (const void *)menu,
     (void *)rows,
     (void *)cols));

  if (!menu)
    RETURN(E_BAD_ARGUMENT);

  if (menu->items && *(menu->items))
    {
      if (rows)
	*rows = menu->height;
      if (cols)
	*cols = menu->width;
      RETURN(E_OK);
    }
  else
    RETURN(E_NOT_CONNECTED);
}

 
