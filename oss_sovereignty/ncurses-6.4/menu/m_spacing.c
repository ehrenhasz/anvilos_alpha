 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_spacing.c,v 1.22 2020/12/12 00:38:14 tom Exp $")

#define MAX_SPC_DESC ((TABSIZE) ? (TABSIZE) : 8)
#define MAX_SPC_COLS ((TABSIZE) ? (TABSIZE) : 8)
#define MAX_SPC_ROWS (3)

 
MENU_EXPORT(int)
set_menu_spacing(MENU *menu, int s_desc, int s_row, int s_col)
{
  MENU *m;			 

  T((T_CALLED("set_menu_spacing(%p,%d,%d,%d)"),
     (void *)menu, s_desc, s_row, s_col));

  m = Normalize_Menu(menu);

  assert(m);
  if (m->status & _POSTED)
    RETURN(E_POSTED);

  if (((s_desc < 0) || (s_desc > MAX_SPC_DESC)) ||
      ((s_row < 0) || (s_row > MAX_SPC_ROWS)) ||
      ((s_col < 0) || (s_col > MAX_SPC_COLS)))
    RETURN(E_BAD_ARGUMENT);

  m->spc_desc = (short)(s_desc ? s_desc : 1);
  m->spc_rows = (short)(s_row ? s_row : 1);
  m->spc_cols = (short)(s_col ? s_col : 1);
  _nc_Calculate_Item_Length_and_Width(m);

  RETURN(E_OK);
}

 
MENU_EXPORT(int)
menu_spacing(const MENU *menu, int *s_desc, int *s_row, int *s_col)
{
  const MENU *m;		 

  T((T_CALLED("menu_spacing(%p,%p,%p,%p)"),
     (const void *)menu,
     (void *)s_desc,
     (void *)s_row,
     (void *)s_col));

  m = Normalize_Menu(menu);

  assert(m);
  if (s_desc)
    *s_desc = m->spc_desc;
  if (s_row)
    *s_row = m->spc_rows;
  if (s_col)
    *s_col = m->spc_cols;

  RETURN(E_OK);
}

 
