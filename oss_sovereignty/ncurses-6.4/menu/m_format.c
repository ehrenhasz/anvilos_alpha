 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_format.c,v 1.22 2021/03/27 23:46:29 tom Exp $")

#define minimum(a,b) ((a)<(b) ? (a): (b))

 
MENU_EXPORT(int)
set_menu_format(MENU *menu, int rows, int cols)
{

  T((T_CALLED("set_menu_format(%p,%d,%d)"), (void *)menu, rows, cols));

  if (rows < 0 || cols < 0)
    RETURN(E_BAD_ARGUMENT);

  if (menu)
    {
      int total_rows, total_cols;

      if (menu->status & _POSTED)
	RETURN(E_POSTED);

      if (!(menu->items))
	RETURN(E_NOT_CONNECTED);

      if (rows == 0)
	rows = menu->frows;
      if (cols == 0)
	cols = menu->fcols;

      if (menu->pattern)
	Reset_Pattern(menu);

      menu->frows = (short)rows;
      menu->fcols = (short)cols;

      assert(rows > 0 && cols > 0);
      total_rows = (menu->nitems - 1) / cols + 1;
      total_cols = (menu->opt & O_ROWMAJOR) ?
	minimum(menu->nitems, cols) :
	(menu->nitems - 1) / total_rows + 1;

      menu->rows = (short)total_rows;
      menu->cols = (short)total_cols;
      menu->arows = (short)minimum(total_rows, rows);
      menu->toprow = 0;
      menu->curitem = *(menu->items);
      assert(menu->curitem);
      SetStatus(menu, _LINK_NEEDED);
      _nc_Calculate_Item_Length_and_Width(menu);
    }
  else
    {
      if (rows > 0)
	_nc_Default_Menu.frows = (short)rows;
      if (cols > 0)
	_nc_Default_Menu.fcols = (short)cols;
    }

  RETURN(E_OK);
}

 
MENU_EXPORT(void)
menu_format(const MENU *menu, int *rows, int *cols)
{
  if (rows)
    *rows = Normalize_Menu(menu)->frows;
  if (cols)
    *cols = Normalize_Menu(menu)->fcols;
}

 
