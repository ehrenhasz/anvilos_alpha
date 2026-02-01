 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_data.c,v 1.21 2021/06/17 21:11:08 tom Exp $")

 
FORM_EXPORT(bool)
data_behind(const FORM *form)
{
  bool result = FALSE;

  T((T_CALLED("data_behind(%p)"), (const void *)form));

  if (form && (form->status & _POSTED) && form->current)
    {
      FIELD *field;

      field = form->current;
      if (!Single_Line_Field(field))
	{
	  result = (form->toprow == 0) ? FALSE : TRUE;
	}
      else
	{
	  result = (form->begincol == 0) ? FALSE : TRUE;
	}
    }
  returnBool(result);
}

 
NCURSES_INLINE static bool
Only_Padding(WINDOW *w, int len, int pad)
{
  bool result = TRUE;
  int y, x, j;
  FIELD_CELL cell;

  getyx(w, y, x);
  for (j = 0; j < len; ++j)
    {
      if (wmove(w, y, x + j) != ERR)
	{
#if USE_WIDEC_SUPPORT
	  if (win_wch(w, &cell) != ERR)
	    {
	      if ((chtype)CharOf(cell) != ChCharOf(pad)
		  || cell.chars[1] != 0)
		{
		  result = FALSE;
		  break;
		}
	    }
#else
	  cell = (FIELD_CELL)winch(w);
	  if (ChCharOf(cell) != ChCharOf(pad))
	    {
	      result = FALSE;
	      break;
	    }
#endif
	}
      else
	{
	   
	  break;
	}
    }
   
  return result;
}

 
FORM_EXPORT(bool)
data_ahead(const FORM *form)
{
  bool result = FALSE;

  T((T_CALLED("data_ahead(%p)"), (const void *)form));

  if (form && (form->status & _POSTED) && form->current)
    {
      FIELD *field;
      bool cursor_moved = FALSE;
      int pos;

      field = form->current;
      assert(form->w);

      if (Single_Line_Field(field))
	{
	  pos = form->begincol + field->cols;
	  while (pos < field->dcols)
	    {
	      int check_len = field->dcols - pos;

	      if (check_len >= field->cols)
		check_len = field->cols;
	      cursor_moved = TRUE;
	      wmove(form->w, 0, pos);
	      if (Only_Padding(form->w, check_len, field->pad))
		pos += field->cols;
	      else
		{
		  result = TRUE;
		  break;
		}
	    }
	}
      else
	{
	  pos = form->toprow + field->rows;
	  while (pos < field->drows)
	    {
	      cursor_moved = TRUE;
	      wmove(form->w, pos, 0);
	      pos++;
	      if (!Only_Padding(form->w, field->cols, field->pad))
		{
		  result = TRUE;
		  break;
		}
	    }
	}

      if (cursor_moved)
	wmove(form->w, form->currow, form->curcol);
    }
  returnBool(result);
}

 
