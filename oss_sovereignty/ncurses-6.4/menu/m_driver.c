 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_driver.c,v 1.37 2021/03/27 23:46:29 tom Exp $")

 

 
#define Remove_Character_From_Pattern(menu) \
  (menu)->pattern[--((menu)->pindex)] = '\0'

 
#define Add_Character_To_Pattern(menu,ch) \
  { (menu)->pattern[((menu)->pindex)++] = (char) (ch);\
    (menu)->pattern[(menu)->pindex] = '\0'; }

 
static bool
Is_Sub_String(
	       bool IgnoreCaseFlag,
	       const char *part,
	       const char *string
)
{
  assert(part && string);
  if (IgnoreCaseFlag)
    {
      while (*string && *part)
	{
	  if (toupper(UChar(*string++)) != toupper(UChar(*part)))
	    break;
	  part++;
	}
    }
  else
    {
      while (*string && *part)
	if (*part != *string++)
	  break;
      part++;
    }
  return ((*part) ? FALSE : TRUE);
}

 
MENU_EXPORT(int)
_nc_Match_Next_Character_In_Item_Name
(MENU *menu, int ch, ITEM **item)
{
  bool found = FALSE, passed = FALSE;
  int idx, last;

  T((T_CALLED("_nc_Match_Next_Character(%p,%d,%p)"),
     (void *)menu, ch, (void *)item));

  assert(menu && item && *item);
  idx = (*item)->index;

  if (ch && ch != BS)
    {
       
      if ((menu->pindex + 1) > menu->namelen)
	RETURN(E_NO_MATCH);

      Add_Character_To_Pattern(menu, ch);
       
      if (--idx < 0)
	idx = menu->nitems - 1;
    }

  last = idx;			 

  do
    {
      if (ch == BS)
	{			 
	  if (--idx < 0)
	    idx = menu->nitems - 1;
	}
      else
	{			 
	  if (++idx >= menu->nitems)
	    idx = 0;
	}
      if (Is_Sub_String((bool)((menu->opt & O_IGNORECASE) != 0),
			menu->pattern,
			menu->items[idx]->name.str)
	)
	found = TRUE;
      else
	passed = TRUE;
    }
  while (!found && (idx != last));

  if (found)
    {
      if (!((idx == (*item)->index) && passed))
	{
	  *item = menu->items[idx];
	  RETURN(E_OK);
	}
       
      assert(ch == 0 || ch == BS);
    }
  else
    {
      if (ch && ch != BS && menu->pindex > 0)
	{
	   
	  Remove_Character_From_Pattern(menu);
	}
    }
  RETURN(E_NO_MATCH);
}

 
MENU_EXPORT(int)
menu_driver(MENU *menu, int c)
{
#define NAVIGATE(dir) \
  if (!item->dir)\
     result = E_REQUEST_DENIED;\
  else\
     item = item->dir

  int result = E_OK;
  ITEM *item;
  int my_top_row;

  T((T_CALLED("menu_driver(%p,%d)"), (void *)menu, c));

  if (!menu)
    RETURN(E_BAD_ARGUMENT);

  if (menu->status & _IN_DRIVER)
    RETURN(E_BAD_STATE);
  if (!(menu->status & _POSTED))
    RETURN(E_NOT_POSTED);

  item = menu->curitem;

  my_top_row = menu->toprow;
  assert(item);

  if ((c > KEY_MAX) && (c <= MAX_MENU_COMMAND))
    {
      int rdiff;

      if (!((c == REQ_BACK_PATTERN)
	    || (c == REQ_NEXT_MATCH) || (c == REQ_PREV_MATCH)))
	{
	  assert(menu->pattern);
	  Reset_Pattern(menu);
	}

      switch (c)
	{
	case REQ_LEFT_ITEM:
	     
	  NAVIGATE(left);
	  break;

	case REQ_RIGHT_ITEM:
	     
	  NAVIGATE(right);
	  break;

	case REQ_UP_ITEM:
	     
	  NAVIGATE(up);
	  break;

	case REQ_DOWN_ITEM:
	     
	  NAVIGATE(down);
	  break;

	case REQ_SCR_ULINE:
	     
	  if (my_top_row == 0 || !(item->up))
	    result = E_REQUEST_DENIED;
	  else
	    {
	      --my_top_row;
	      item = item->up;
	    }
	  break;

	case REQ_SCR_DLINE:
	     
	  if ((my_top_row + menu->arows >= menu->rows) || !(item->down))
	    {
	       
	      result = E_REQUEST_DENIED;
	    }
	  else
	    {
	      my_top_row++;
	      item = item->down;
	    }
	  break;

	case REQ_SCR_DPAGE:
	     
	  rdiff = menu->rows - (menu->arows + my_top_row);
	  if (rdiff > menu->arows)
	    rdiff = menu->arows;
	  if (rdiff <= 0)
	    result = E_REQUEST_DENIED;
	  else
	    {
	      my_top_row += rdiff;
	      while (rdiff-- > 0 && item != 0 && item->down != 0)
		item = item->down;
	    }
	  break;

	case REQ_SCR_UPAGE:
	     
	  rdiff = (menu->arows < my_top_row) ? menu->arows : my_top_row;
	  if (rdiff <= 0)
	    result = E_REQUEST_DENIED;
	  else
	    {
	      my_top_row -= rdiff;
	      while (rdiff-- > 0 && item != 0 && item->up != 0)
		item = item->up;
	    }
	  break;

	case REQ_FIRST_ITEM:
	     
	  item = menu->items[0];
	  break;

	case REQ_LAST_ITEM:
	     
	  item = menu->items[menu->nitems - 1];
	  break;

	case REQ_NEXT_ITEM:
	     
	  if ((item->index + 1) >= menu->nitems)
	    {
	      if (menu->opt & O_NONCYCLIC)
		result = E_REQUEST_DENIED;
	      else
		item = menu->items[0];
	    }
	  else
	    item = menu->items[item->index + 1];
	  break;

	case REQ_PREV_ITEM:
	     
	  if (item->index <= 0)
	    {
	      if (menu->opt & O_NONCYCLIC)
		result = E_REQUEST_DENIED;
	      else
		item = menu->items[menu->nitems - 1];
	    }
	  else
	    item = menu->items[item->index - 1];
	  break;

	case REQ_TOGGLE_ITEM:
	     
	  if (menu->opt & O_ONEVALUE)
	    {
	      result = E_REQUEST_DENIED;
	    }
	  else
	    {
	      if (menu->curitem->opt & O_SELECTABLE)
		{
		  menu->curitem->value = !menu->curitem->value;
		  Move_And_Post_Item(menu, menu->curitem);
		  _nc_Show_Menu(menu);
		}
	      else
		result = E_NOT_SELECTABLE;
	    }
	  break;

	case REQ_CLEAR_PATTERN:
	     
	   
	  break;

	case REQ_BACK_PATTERN:
	     
	  if (menu->pindex > 0)
	    {
	      assert(menu->pattern);
	      Remove_Character_From_Pattern(menu);
	      pos_menu_cursor(menu);
	    }
	  else
	    result = E_REQUEST_DENIED;
	  break;

	case REQ_NEXT_MATCH:
	     
	  assert(menu->pattern);
	  if (menu->pattern[0])
	    result = _nc_Match_Next_Character_In_Item_Name(menu, 0, &item);
	  else
	    {
	      if ((item->index + 1) < menu->nitems)
		item = menu->items[item->index + 1];
	      else
		{
		  if (menu->opt & O_NONCYCLIC)
		    result = E_REQUEST_DENIED;
		  else
		    item = menu->items[0];
		}
	    }
	  break;

	case REQ_PREV_MATCH:
	     
	  assert(menu->pattern);
	  if (menu->pattern[0])
	    result = _nc_Match_Next_Character_In_Item_Name(menu, BS, &item);
	  else
	    {
	      if (item->index)
		item = menu->items[item->index - 1];
	      else
		{
		  if (menu->opt & O_NONCYCLIC)
		    result = E_REQUEST_DENIED;
		  else
		    item = menu->items[menu->nitems - 1];
		}
	    }
	  break;

	default:
	     
	  result = E_UNKNOWN_COMMAND;
	  break;
	}
    }
  else
    {				 
      if (!(c & ~((int)MAX_REGULAR_CHARACTER)) && isprint(UChar(c)))
	result = _nc_Match_Next_Character_In_Item_Name(menu, c, &item);
#ifdef NCURSES_MOUSE_VERSION
      else if (KEY_MOUSE == c)
	{
	  MEVENT event;
	  WINDOW *uwin = Get_Menu_UserWin(menu);

	  getmouse(&event);
	  if ((event.bstate & (BUTTON1_CLICKED |
			       BUTTON1_DOUBLE_CLICKED |
			       BUTTON1_TRIPLE_CLICKED))
	      && wenclose(uwin, event.y, event.x))
	    {			 
	      WINDOW *sub = Get_Menu_Window(menu);
	      int ry = event.y, rx = event.x;	 

	      result = E_REQUEST_DENIED;
	      if (mouse_trafo(&ry, &rx, FALSE))
		{		 
		  if (ry < sub->_begy)
		    {		 
		      if (event.bstate & BUTTON1_CLICKED)
			result = menu_driver(menu, REQ_SCR_ULINE);
		      else if (event.bstate & BUTTON1_DOUBLE_CLICKED)
			result = menu_driver(menu, REQ_SCR_UPAGE);
		      else if (event.bstate & BUTTON1_TRIPLE_CLICKED)
			result = menu_driver(menu, REQ_FIRST_ITEM);
		      RETURN(result);
		    }
		  else if (ry > sub->_begy + sub->_maxy)
		    {		 
		      if (event.bstate & BUTTON1_CLICKED)
			result = menu_driver(menu, REQ_SCR_DLINE);
		      else if (event.bstate & BUTTON1_DOUBLE_CLICKED)
			result = menu_driver(menu, REQ_SCR_DPAGE);
		      else if (event.bstate & BUTTON1_TRIPLE_CLICKED)
			result = menu_driver(menu, REQ_LAST_ITEM);
		      RETURN(result);
		    }
		  else if (wenclose(sub, event.y, event.x))
		    {		 
		      int x, y;

		      ry = event.y;
		      rx = event.x;
		      if (wmouse_trafo(sub, &ry, &rx, FALSE))
			{
			  int i;

			  for (i = 0; i < menu->nitems; i++)
			    {
			      int err = _nc_menu_cursor_pos(menu,
							    menu->items[i],
							    &y, &x);

			      if (E_OK == err)
				{
				  if ((ry == y) &&
				      (rx >= x) &&
				      (rx < x + menu->itemlen))
				    {
				      item = menu->items[i];
				      result = E_OK;
				      break;
				    }
				}
			    }
			  if (E_OK == result)
			    {	 
			      if (event.bstate & BUTTON1_DOUBLE_CLICKED)
				{
				  _nc_New_TopRow_and_CurrentItem(menu,
								 my_top_row,
								 item);
				  menu_driver(menu, REQ_TOGGLE_ITEM);
				  result = E_UNKNOWN_COMMAND;
				}
			    }
			}
		    }
		}
	    }
	  else
	    {
	      if (menu->opt & O_MOUSE_MENU)
		ungetmouse(&event);	 
	      result = E_REQUEST_DENIED;
	    }
	}
#endif  
      else
	result = E_UNKNOWN_COMMAND;
    }

  if (item == 0)
    {
      result = E_BAD_STATE;
    }
  else if (E_OK == result)
    {
       
      if (item->y < my_top_row)
	my_top_row = item->y;
      else if (item->y >= (my_top_row + menu->arows))
	my_top_row = item->y - menu->arows + 1;

      _nc_New_TopRow_and_CurrentItem(menu, my_top_row, item);

    }

  RETURN(result);
}

 
