 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_global.c,v 1.33 2021/03/27 23:46:29 tom Exp $")

static char mark[] = "-";
 
MENU_EXPORT_VAR(MENU) _nc_Default_Menu = {
  16,				   
  1,				   
  16,				   
  1,			           
  16,				   
  1,				   
  16,				   
  0,				   
  0,				   
  1,				   
  1,				   
  1,                               
  1,                               
  1,                               
  (char *)0,			   
  0,				   
  (WINDOW *)0,			   
  (WINDOW *)0,			   
  (WINDOW *)0,			   
  (WINDOW *)0,			   
  (ITEM **)0,			   
  0,				   
  (ITEM *)0,			   
  0,				   
  (chtype)A_REVERSE,		   
  (chtype)A_NORMAL,		   
  (chtype)A_UNDERLINE,		   
  ' ',  			   
  (Menu_Hook)0,			   
  (Menu_Hook)0,			   
  (Menu_Hook)0,			   
  (Menu_Hook)0,			   
  (void *)0,			   
  mark,				   
  ALL_MENU_OPTS,                   
  0			           
};

MENU_EXPORT_VAR(ITEM) _nc_Default_Item = {
  { (char *)0, 0 },		   
  { (char *)0, 0 },		   
  (MENU *)0,		           
  (char *)0,			   
  ALL_ITEM_OPTS,		   
  0,				   
  0,				   
  0,				   
  FALSE,			   
  (ITEM *)0,		           
  (ITEM *)0,		           
  (ITEM *)0,		           
  (ITEM *)0		           
  };
 

 
NCURSES_INLINE static void
ComputeMaximum_NameDesc_Lengths(MENU *menu)
{
  unsigned MaximumNameLength = 0;
  unsigned MaximumDescriptionLength = 0;
  ITEM **items;

  assert(menu && menu->items);
  for (items = menu->items; *items; items++)
    {
      unsigned check = (unsigned)_nc_Calculate_Text_Width(&((*items)->name));

      if (check > MaximumNameLength)
	MaximumNameLength = check;

      check = (unsigned)_nc_Calculate_Text_Width(&((*items)->description));
      if (check > MaximumDescriptionLength)
	MaximumDescriptionLength = check;
    }

  menu->namelen = (short)MaximumNameLength;
  menu->desclen = (short)MaximumDescriptionLength;
  T(("ComputeMaximum_NameDesc_Lengths %d,%d", menu->namelen, menu->desclen));
}

 
NCURSES_INLINE static void
ResetConnectionInfo(MENU *menu, ITEM **items)
{
  ITEM **item;

  assert(menu && items);
  for (item = items; *item; item++)
    {
      (*item)->index = 0;
      (*item)->imenu = (MENU *)0;
    }
  if (menu->pattern)
    free(menu->pattern);
  menu->pattern = (char *)0;
  menu->pindex = 0;
  menu->items = (ITEM **)0;
  menu->nitems = 0;
}

 
MENU_EXPORT(bool)
_nc_Connect_Items(MENU *menu, ITEM **items)
{
  unsigned int ItemCount = 0;

  if (menu && items)
    {
      ITEM **item;

      for (item = items; *item; item++)
	{
	  if ((*item)->imenu)
	    {
	       
	      break;
	    }
	}
      if (!(*item))
	 
	{
	  for (item = items; *item; item++)
	    {
	      if (menu->opt & O_ONEVALUE)
		{
		  (*item)->value = FALSE;
		}
	      (*item)->index = (short)ItemCount++;
	      (*item)->imenu = menu;
	    }
	}
    }
  else
    return (FALSE);

  if (ItemCount != 0)
    {
      menu->items = items;
      menu->nitems = (short)ItemCount;
      ComputeMaximum_NameDesc_Lengths(menu);
      if ((menu->pattern = typeMalloc(char, (unsigned)(1 + menu->namelen))))
	{
	  Reset_Pattern(menu);
	  set_menu_format(menu, menu->frows, menu->fcols);
	  menu->curitem = *items;
	  menu->toprow = 0;
	  return (TRUE);
	}
    }

   
  ResetConnectionInfo(menu, items);
  return (FALSE);
}

 
MENU_EXPORT(void)
_nc_Disconnect_Items(MENU *menu)
{
  if (menu && menu->items)
    ResetConnectionInfo(menu, menu->items);
}

 
MENU_EXPORT(int)
_nc_Calculate_Text_Width(const TEXT *item   )
{
#if USE_WIDEC_SUPPORT
  int result = item->length;

  T((T_CALLED("_nc_menu_text_width(%p)"), (const void *)item));
  if (result != 0 && item->str != 0)
    {
      int count = (int)mbstowcs(0, item->str, 0);
      wchar_t *temp = 0;

      if (count > 0
	  && (temp = typeMalloc(wchar_t, 2 + count)) != 0)
	{
	  int n;

	  result = 0;
	  mbstowcs(temp, item->str, (unsigned)count);
	  for (n = 0; n < count; ++n)
	    {
	      int test = wcwidth(temp[n]);

	      if (test <= 0)
		test = 1;
	      result += test;
	    }
	  free(temp);
	}
    }
  returnCode(result);
#else
  return item->length;
#endif
}

 
#if USE_WIDEC_SUPPORT
static int
calculate_actual_width(MENU *menu, bool name)
{
  int width = 0;

  assert(menu && menu->items);

  if (menu->items != 0)
    {
      ITEM **items;

      for (items = menu->items; *items; items++)
	{
	  int check = (name
		       ? _nc_Calculate_Text_Width(&((*items)->name))
		       : _nc_Calculate_Text_Width(&((*items)->description)));

	  if (check > width)
	    width = check;
	}
    }
  else
    {
      width = (name ? menu->namelen : menu->desclen);
    }

  T(("calculate_actual_width %s = %d/%d",
     name ? "name" : "desc",
     width,
     name ? menu->namelen : menu->desclen));
  return width;
}
#else
#define calculate_actual_width(menu, name) (name ? menu->namelen : menu->desclen)
#endif

 
MENU_EXPORT(void)
_nc_Calculate_Item_Length_and_Width(MENU *menu)
{
  int l;

  assert(menu);

  menu->height = (short)(1 + menu->spc_rows * (menu->arows - 1));

  l = calculate_actual_width(menu, TRUE);
  l += menu->marklen;

  if ((menu->opt & O_SHOWDESC) && (menu->desclen > 0))
    {
      l += calculate_actual_width(menu, FALSE);
      l += menu->spc_desc;
    }

  menu->itemlen = (short)l;
  l *= menu->cols;
  l += (menu->cols - 1) * menu->spc_cols;	 
  menu->width = (short)l;

  T(("_nc_CalculateItem_Length_and_Width columns %d, item %d, width %d",
     menu->cols,
     menu->itemlen,
     menu->width));
}

 
MENU_EXPORT(void)
_nc_Link_Items(MENU *menu)
{
  if (menu && menu->items && *(menu->items))
    {
      int i;
      ITEM *item;
      int Number_Of_Items = menu->nitems;
      int col = 0, row = 0;
      int Last_in_Row;
      int Last_in_Column;
      bool cycle = (menu->opt & O_NONCYCLIC) ? FALSE : TRUE;

      ClrStatus(menu, _LINK_NEEDED);

      if (menu->opt & O_ROWMAJOR)
	{
	  int Number_Of_Columns = menu->cols;

	  for (i = 0; i < Number_Of_Items; i++)
	    {
	      item = menu->items[i];

	      Last_in_Row = row * Number_Of_Columns + (Number_Of_Columns - 1);

	      item->left = (col) ?
	       
		menu->items[i - 1] :
		(cycle ? menu->items[(Last_in_Row >= Number_Of_Items) ?
				     Number_Of_Items - 1 :
				     Last_in_Row] :
		 (ITEM *)0);

	      item->right = ((col < (Number_Of_Columns - 1)) &&
			     ((i + 1) < Number_Of_Items)
		)?
		menu->items[i + 1] :
		(cycle ? menu->items[row * Number_Of_Columns] :
		 (ITEM *)0
		);

	      Last_in_Column = (menu->rows - 1) * Number_Of_Columns + col;

	      item->up = (row) ? menu->items[i - Number_Of_Columns] :
		(cycle ? menu->items[(Last_in_Column >= Number_Of_Items) ?
				     Number_Of_Items - 1 :
				     Last_in_Column] :
		 (ITEM *)0);

	      item->down = ((i + Number_Of_Columns) < Number_Of_Items)
		?
		menu->items[i + Number_Of_Columns] :
		(cycle ? menu->items[(row + 1) < menu->rows ?
				     Number_Of_Items - 1 : col] :
		 (ITEM *)0);
	      item->x = (short)col;
	      item->y = (short)row;
	      if (++col == Number_Of_Columns)
		{
		  row++;
		  col = 0;
		}
	    }
	}
      else
	{
	  int Number_Of_Rows = menu->rows;
	  int j;

	  for (j = 0; j < Number_Of_Items; j++)
	    {
	      item = menu->items[i = (col * Number_Of_Rows + row)];

	      Last_in_Column = (menu->cols - 1) * Number_Of_Rows + row;

	      item->left = (col) ?
		menu->items[i - Number_Of_Rows] :
		(cycle ? (Last_in_Column >= Number_Of_Items) ?
		 menu->items[Last_in_Column - Number_Of_Rows] :
		 menu->items[Last_in_Column] :
		 (ITEM *)0);

	      item->right = ((i + Number_Of_Rows) < Number_Of_Items)
		?
		menu->items[i + Number_Of_Rows] :
		(cycle ? menu->items[row] : (ITEM *)0);

	      Last_in_Row = col * Number_Of_Rows + (Number_Of_Rows - 1);

	      item->up = (row) ?
		menu->items[i - 1] :
		(cycle ?
		 menu->items[(Last_in_Row >= Number_Of_Items) ?
			     Number_Of_Items - 1 :
			     Last_in_Row] :
		 (ITEM *)0);

	      item->down = (row < (Number_Of_Rows - 1))
		?
		(menu->items[((i + 1) < Number_Of_Items) ?
			     i + 1 :
			     (col - 1) * Number_Of_Rows + row + 1]) :
		(cycle ?
		 menu->items[col * Number_Of_Rows] :
		 (ITEM *)0
		);

	      item->x = (short)col;
	      item->y = (short)row;
	      if ((++row) == Number_Of_Rows)
		{
		  col++;
		  row = 0;
		}
	    }
	}
    }
}

 
MENU_EXPORT(void)
_nc_Show_Menu(const MENU *menu)
{
  assert(menu);
  if ((menu->status & _POSTED) && !(menu->status & _IN_DRIVER))
    {
      WINDOW *win;
      int maxy, maxx;

       
      assert(menu->sub);
      mvderwin(menu->sub, menu->spc_rows * menu->toprow, 0);
      win = Get_Menu_Window(menu);

      maxy = getmaxy(win);
      maxx = getmaxx(win);

      if (menu->height < maxy)
	maxy = menu->height;
      if (menu->width < maxx)
	maxx = menu->width;

      copywin(menu->sub, win, 0, 0, 0, 0, maxy - 1, maxx - 1, 0);
      pos_menu_cursor(menu);
    }
}

 
MENU_EXPORT(void)
_nc_New_TopRow_and_CurrentItem(
				MENU *menu,
				int new_toprow,
				ITEM *new_current_item)
{
  assert(menu);
  if (menu->status & _POSTED)
    {
      ITEM *cur_item;
      bool mterm_called = FALSE;
      bool iterm_called = FALSE;

      if (new_current_item != menu->curitem)
	{
	  Call_Hook(menu, itemterm);
	  iterm_called = TRUE;
	}
      if (new_toprow != menu->toprow)
	{
	  Call_Hook(menu, menuterm);
	  mterm_called = TRUE;
	}

      cur_item = menu->curitem;
      assert(cur_item);
      menu->toprow = (short)(((menu->rows - menu->frows) >= 0)
			     ? min(menu->rows - menu->frows, new_toprow)
			     : 0);
      menu->curitem = new_current_item;

      if (mterm_called)
	{
	  Call_Hook(menu, menuinit);
	}
      if (iterm_called)
	{
	   
	  Move_To_Current_Item(menu, cur_item);
	  Call_Hook(menu, iteminit);
	}
      if (mterm_called || iterm_called)
	{
	  _nc_Show_Menu(menu);
	}
      else
	pos_menu_cursor(menu);
    }
  else
    {				 
      menu->toprow = (short)(((menu->rows - menu->frows) >= 0)
			     ? min(menu->rows - menu->frows, new_toprow)
			     : 0);
      menu->curitem = new_current_item;
    }
}

 
