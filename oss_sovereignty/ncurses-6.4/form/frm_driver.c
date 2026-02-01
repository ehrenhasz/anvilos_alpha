 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_driver.c,v 1.135 2021/09/01 23:34:01 tom Exp $")

 

 

 

 
#define FRIENDLY_PREV_NEXT_WORD (1)
 
#define FIX_FORM_INACTIVE_BUG (1)
 
#define GROW_IF_NAVIGATE (1)

#if USE_WIDEC_SUPPORT
#define myADDNSTR(w, s, n) wide_waddnstr(w, s, n)
#define myINSNSTR(w, s, n) wide_winsnstr(w, s, n)
#define myINNSTR(w, s, n)  wide_winnstr(w, s, n)
#define myWCWIDTH(w, y, x) cell_width(w, y, x)
#else
#define myADDNSTR(w, s, n) waddnstr(w, s, n)
#define myINSNSTR(w, s, n) winsnstr(w, s, n)
#define myINNSTR(w, s, n)  winnstr(w, s, n)
#define myWCWIDTH(w, y, x) 1
#endif

 
static int Inter_Field_Navigation(int (*const fct) (FORM *), FORM *form);
static int FN_Next_Field(FORM *form);
static int FN_Previous_Field(FORM *form);
static int FE_New_Line(FORM *);
static int FE_Delete_Previous(FORM *);

 

 
#define Position_Of_Row_In_Buffer(field,row) ((row)*(field)->dcols)

 
#define Address_Of_Nth_Buffer(field,N) \
  ((field)->buf + (N)*(1+Buffer_Length(field)))

 
#define Address_Of_Row_In_Nth_Buffer(field,N,row) \
  (Address_Of_Nth_Buffer(field,N) + Position_Of_Row_In_Buffer(field,row))

 
#define Address_Of_Row_In_Buffer(field,row) \
  Address_Of_Row_In_Nth_Buffer(field,0,row)

 
#define Address_Of_Current_Row_In_Nth_Buffer(form,N) \
   Address_Of_Row_In_Nth_Buffer((form)->current,N,(form)->currow)

 
#define Address_Of_Current_Row_In_Buffer(form) \
   Address_Of_Current_Row_In_Nth_Buffer(form,0)

 
#define Address_Of_Current_Position_In_Nth_Buffer(form,N) \
   (Address_Of_Current_Row_In_Nth_Buffer(form,N) + (form)->curcol)

 
#define Address_Of_Current_Position_In_Buffer(form) \
  Address_Of_Current_Position_In_Nth_Buffer(form,0)

 
#define Is_Scroll_Field(field)          \
   (((field)->drows > (field)->rows) || \
    ((field)->dcols > (field)->cols))

 
#define Has_Invisible_Parts(field)     \
  (!(Field_Has_Option(field, O_PUBLIC)) || \
   Is_Scroll_Field(field))

 
#define Justification_Allowed(field)        \
   (((field)->just != NO_JUSTIFICATION)  && \
    (Single_Line_Field(field))           && \
    ((Field_Has_Option(field, O_STATIC)  && \
     ((field)->dcols == (field)->cols))  || \
    Field_Has_Option(field, O_DYNAMIC_JUSTIFY)))

 
#define Growable(field) ((field)->status & _MAY_GROW)

 
#define Set_Field_Window_Attributes(field,win) \
(  wbkgdset((win),(chtype)((chtype)((field)->pad) | (field)->back)), \
   (void) wattrset((win), (int)(field)->fore) )

 
#define Field_Really_Appears(field)         \
  ((field->form)                          &&\
   (field->form->status & _POSTED)        &&\
   (Field_Has_Option(field, O_VISIBLE))   &&\
   (field->page == field->form->curpage))

 
#define First_Position_In_Current_Field(form) \
  (((form)->currow==0) && ((form)->curcol==0))

#define Minimum(a,b) (((a)<=(b)) ? (a) : (b))
#define Maximum(a,b) (((a)>=(b)) ? (a) : (b))

 
static FIELD_CELL myBLANK = BLANK;
static FIELD_CELL myZEROS;

#ifdef TRACE
static void
check_pos(FORM *form, int lineno)
{
  if (form && form->w)
    {
      int y, x;

      getyx(form->w, y, x);
      if (y != form->currow || x != form->curcol)
	{
	  T(("CHECKPOS %s@%d have position %d,%d vs want %d,%d",
	     __FILE__, lineno,
	     y, x,
	     form->currow, form->curcol));
	}
    }
}
#define CHECKPOS(form) check_pos(form, __LINE__)
#else
#define CHECKPOS(form)		 
#endif

 
#if USE_WIDEC_SUPPORT
 
static int
wide_waddnstr(WINDOW *w, const cchar_t *s, int n)
{
  int rc = OK;

  while (n-- > 0)
    {
      if ((rc = wadd_wch(w, s)) != OK)
	break;
      ++s;
    }
  return rc;
}

 
static int
wide_winsnstr(WINDOW *w, const cchar_t *s, int n)
{
  int code = ERR;

  while (n-- > 0)
    {
      int y, x;

      getyx(w, y, x);
      if ((code = wins_wch(w, s++)) != OK)
	break;
      if ((code = wmove(w, y, x + 1)) != OK)
	break;
    }
  return code;
}

 
static int
wide_winnstr(WINDOW *w, cchar_t *s, int n)
{
  int x;

  win_wchnstr(w, s, n);
   
  for (x = 0; x < n; ++x)
    {
      RemAttr(s[x], A_ATTRIBUTES);
      SetPair(s[x], 0);
    }
  return n;
}

 
static int
cell_base(WINDOW *win, int y, int x)
{
  int result = x;

  while (LEGALYX(win, y, x))
    {
      cchar_t *data = &(win->_line[y].text[x]);

      if (isWidecBase(CHDEREF(data)) || !isWidecExt(CHDEREF(data)))
	{
	  result = x;
	  break;
	}
      --x;
    }
  return result;
}

 
static int
cell_width(WINDOW *win, int y, int x)
{
  int result = 1;

  if (LEGALYX(win, y, x))
    {
      cchar_t *data = &(win->_line[y].text[x]);

      if (isWidecExt(CHDEREF(data)))
	{
	   
	  result = cell_width(win, y, x - 1);
	}
      else
	{
	  result = wcwidth(CharOf(CHDEREF(data)));
	}
    }
  return result;
}

 
static void
delete_char(FORM *form)
{
  int cells = cell_width(form->w, form->currow, form->curcol);

  form->curcol = cell_base(form->w, form->currow, form->curcol);
  wmove(form->w, form->currow, form->curcol);
  while (cells-- > 0)
    {
      wdelch(form->w);
    }
}
#define DeleteChar(form) delete_char(form)
#else
#define DeleteChar(form) \
	  wmove((form)->w, (form)->currow, (form)->curcol), \
	  wdelch((form)->w)
#endif

 
NCURSES_INLINE static FIELD_CELL *
Get_Start_Of_Data(FIELD_CELL *buf, int blen)
{
  FIELD_CELL *p = buf;
  FIELD_CELL *end = &buf[blen];

  assert(buf && blen >= 0);
  while ((p < end) && ISBLANK(*p))
    p++;
  return ((p == end) ? buf : p);
}

 
NCURSES_INLINE static FIELD_CELL *
After_End_Of_Data(FIELD_CELL *buf, int blen)
{
  FIELD_CELL *p = &buf[blen];

  assert(buf && blen >= 0);
  while ((p > buf) && ISBLANK(p[-1]))
    p--;
  return (p);
}

 
NCURSES_INLINE static FIELD_CELL *
Get_First_Whitespace_Character(FIELD_CELL *buf, int blen)
{
  FIELD_CELL *p = buf;
  FIELD_CELL *end = &p[blen];

  assert(buf && blen >= 0);
  while ((p < end) && !ISBLANK(*p))
    p++;
  return ((p == end) ? buf : p);
}

 
NCURSES_INLINE static FIELD_CELL *
After_Last_Whitespace_Character(FIELD_CELL *buf, int blen)
{
  FIELD_CELL *p = &buf[blen];

  assert(buf && blen >= 0);
  while ((p > buf) && !ISBLANK(p[-1]))
    p--;
  return (p);
}

 
#define USE_DIV_T (0)

 
NCURSES_INLINE static void
Adjust_Cursor_Position(FORM *form, const FIELD_CELL *pos)
{
  FIELD *field;
  int idx;

  field = form->current;
  assert(pos >= field->buf && field->dcols > 0);
  idx = (int)(pos - field->buf);
#if USE_DIV_T
  *((div_t *) & (form->currow)) = div(idx, field->dcols);
#else
  form->currow = idx / field->dcols;
  form->curcol = idx - field->cols * form->currow;
#endif
  if (field->drows < form->currow)
    form->currow = 0;
}

 
static void
Buffer_To_Window(const FIELD *field, WINDOW *win)
{
  int width, height;
  int y, x;
  int row;
  FIELD_CELL *pBuffer;

  assert(win && field);

  getyx(win, y, x);
  width = getmaxx(win);
  height = getmaxy(win);

  for (row = 0, pBuffer = field->buf;
       row < height;
       row++, pBuffer += width)
    {
      int len;

      if ((len = (int)(After_End_Of_Data(pBuffer, width) - pBuffer)) > 0)
	{
	  wmove(win, row, 0);
	  myADDNSTR(win, pBuffer, len);
	}
    }
  wmove(win, y, x);
}

 
FORM_EXPORT(void)
_nc_get_fieldbuffer(FORM *form, FIELD *field, FIELD_CELL *buf)
{
  int pad;
  int len = 0;
  FIELD_CELL *p;
  int row, height;
  WINDOW *win;

  assert(form && field && buf);

  win = form->w;
  assert(win);

  pad = field->pad;
  p = buf;
  height = getmaxy(win);

  for (row = 0; (row < height) && (row < field->drows); row++)
    {
      wmove(win, row, 0);
      len += myINNSTR(win, p + len, field->dcols);
    }
  p[len] = myZEROS;

   
  if (pad != C_BLANK)
    {
      int i;

      for (i = 0; i < len; i++, p++)
	{
	  if ((unsigned long)CharOf(*p) == ChCharOf(pad)
#if USE_WIDEC_SUPPORT
	      && p->chars[1] == 0
#endif
	    )
	    *p = myBLANK;
	}
    }
}

 
static void
Window_To_Buffer(FORM *form, FIELD *field)
{
  _nc_get_fieldbuffer(form, field, field->buf);
}

 
NCURSES_INLINE static void
Synchronize_Buffer(FORM *form)
{
  if (form->status & _WINDOW_MODIFIED)
    {
      ClrStatus(form, _WINDOW_MODIFIED);
      SetStatus(form, _FCHECK_REQUIRED);
      Window_To_Buffer(form, form->current);
      wmove(form->w, form->currow, form->curcol);
    }
}

 
static bool
Field_Grown(FIELD *field, int amount)
{
  bool result = FALSE;

  if (field && Growable(field))
    {
      bool single_line_field = Single_Line_Field(field);
      int old_buflen = Buffer_Length(field);
      int new_buflen;
      int old_dcols = field->dcols;
      int old_drows = field->drows;
      FIELD_CELL *oldbuf = field->buf;
      FIELD_CELL *newbuf;

      int growth;
      FORM *form = field->form;
      bool need_visual_update = ((form != (FORM *)0) &&
				 (form->status & _POSTED) &&
				 (form->current == field));

      if (need_visual_update)
	Synchronize_Buffer(form);

      if (single_line_field)
	{
	  growth = field->cols * amount;
	  if (field->maxgrow)
	    growth = Minimum(field->maxgrow - field->dcols, growth);
	  field->dcols += growth;
	  if (field->dcols == field->maxgrow)
	    ClrStatus(field, _MAY_GROW);
	}
      else
	{
	  growth = (field->rows + field->nrow) * amount;
	  if (field->maxgrow)
	    growth = Minimum(field->maxgrow - field->drows, growth);
	  field->drows += growth;
	  if (field->drows == field->maxgrow)
	    ClrStatus(field, _MAY_GROW);
	}
       
      new_buflen = Buffer_Length(field);
      newbuf = (FIELD_CELL *)malloc(Total_Buffer_Size(field));
      if (!newbuf)
	{
	   
	  field->dcols = old_dcols;
	  field->drows = old_drows;
	  if ((single_line_field && (field->dcols != field->maxgrow)) ||
	      (!single_line_field && (field->drows != field->maxgrow)))
	    SetStatus(field, _MAY_GROW);
	}
      else
	{
	   
	  int i, j;

	  result = TRUE;	 

	  T((T_CREATE("fieldcell %p"), (void *)newbuf));
	  field->buf = newbuf;
	  for (i = 0; i <= field->nbuf; i++)
	    {
	      FIELD_CELL *new_bp = Address_Of_Nth_Buffer(field, i);
	      FIELD_CELL *old_bp = oldbuf + i * (1 + old_buflen);

	      for (j = 0; j < old_buflen; ++j)
		new_bp[j] = old_bp[j];
	      while (j < new_buflen)
		new_bp[j++] = myBLANK;
	      new_bp[new_buflen] = myZEROS;
	    }

#if USE_WIDEC_SUPPORT && NCURSES_EXT_FUNCS
	  if (wresize(field->working, 1, Buffer_Length(field) + 1) == ERR)
	    result = FALSE;
#endif

	  if (need_visual_update && result)
	    {
	      WINDOW *new_window = newpad(field->drows, field->dcols);

	      if (new_window != 0)
		{
		  assert(form != (FORM *)0);
		  if (form->w)
		    delwin(form->w);
		  form->w = new_window;
		  Set_Field_Window_Attributes(field, form->w);
		  werase(form->w);
		  Buffer_To_Window(field, form->w);
		  untouchwin(form->w);
		  wmove(form->w, form->currow, form->curcol);
		}
	      else
		result = FALSE;
	    }

	  if (result)
	    {
	      free(oldbuf);
	       
	      if (field != field->link)
		{
		  FIELD *linked_field;

		  for (linked_field = field->link;
		       linked_field != field;
		       linked_field = linked_field->link)
		    {
		      linked_field->buf = field->buf;
		      linked_field->drows = field->drows;
		      linked_field->dcols = field->dcols;
		    }
		}
	    }
	  else
	    {
	       
	      field->dcols = old_dcols;
	      field->drows = old_drows;
	      field->buf = oldbuf;
	      if ((single_line_field &&
		   (field->dcols != field->maxgrow)) ||
		  (!single_line_field &&
		   (field->drows != field->maxgrow)))
		SetStatus(field, _MAY_GROW);
	      free(newbuf);
	    }
	}
    }
  return (result);
}

#ifdef NCURSES_MOUSE_VERSION
 
static int
Field_encloses(FIELD *field, int ry, int rx)
{
  T((T_CALLED("Field_encloses(%p)"), (void *)field));
  if (field != 0
      && field->frow <= ry
      && (field->frow + field->rows) > ry
      && field->fcol <= rx
      && (field->fcol + field->cols) > rx)
    {
      RETURN(E_OK);
    }
  RETURN(E_INVALID_FIELD);
}
#endif

 
FORM_EXPORT(int)
_nc_Position_Form_Cursor(FORM *form)
{
  FIELD *field;
  WINDOW *formwin;

  if (!form)
    return (E_BAD_ARGUMENT);

  if (!form->w || !form->current)
    return (E_SYSTEM_ERROR);

  field = form->current;
  formwin = Get_Form_Window(form);

  wmove(form->w, form->currow, form->curcol);
  if (Has_Invisible_Parts(field))
    {
       
      wmove(formwin,
	    field->frow + form->currow - form->toprow,
	    field->fcol + form->curcol - form->begincol);
      wcursyncup(formwin);
    }
  else
    wcursyncup(form->w);
  return (E_OK);
}

 
static bool move_after_insert = TRUE;
FORM_EXPORT(int)
_nc_Refresh_Current_Field(FORM *form)
{
  WINDOW *formwin;
  FIELD *field;
  bool is_public;

  T((T_CALLED("_nc_Refresh_Current_Field(%p)"), (void *)form));

  if (!form)
    RETURN(E_BAD_ARGUMENT);

  if (!form->w || !form->current)
    RETURN(E_SYSTEM_ERROR);

  field = form->current;
  formwin = Get_Form_Window(form);

  is_public = Field_Has_Option(field, O_PUBLIC);

  if (Is_Scroll_Field(field))
    {
       
      if (Single_Line_Field(field))
	{
	   
	  if (form->curcol < form->begincol)
	    form->begincol = form->curcol;
	  else
	    {
	      if (form->curcol >= (form->begincol + field->cols))
		form->begincol = form->curcol - field->cols
		  + (move_after_insert ? 1 : 0);
	    }
	  if (is_public)
	    copywin(form->w,
		    formwin,
		    0,
		    form->begincol,
		    field->frow,
		    field->fcol,
		    field->frow,
		    field->cols + field->fcol - 1,
		    0);
	}
      else
	{
	   
	  int first_modified_row, first_unmodified_row;

	  if (field->drows > field->rows)
	    {
	      int row_after_bottom = form->toprow + field->rows;

	      if (form->currow < form->toprow)
		{
		  form->toprow = form->currow;
		  SetStatus(field, _NEWTOP);
		}
	      if (form->currow >= row_after_bottom)
		{
		  form->toprow = form->currow - field->rows + 1;
		  SetStatus(field, _NEWTOP);
		}
	      if (field->status & _NEWTOP)
		{
		   
		  first_modified_row = form->toprow;
		  first_unmodified_row = first_modified_row + field->rows;
		  ClrStatus(field, _NEWTOP);
		}
	      else
		{
		   
		  first_modified_row = form->toprow;
		  while (first_modified_row < row_after_bottom)
		    {
		      if (is_linetouched(form->w, first_modified_row))
			break;
		      first_modified_row++;
		    }
		  first_unmodified_row = first_modified_row;
		  while (first_unmodified_row < row_after_bottom)
		    {
		      if (!is_linetouched(form->w, first_unmodified_row))
			break;
		      first_unmodified_row++;
		    }
		}
	    }
	  else
	    {
	      first_modified_row = form->toprow;
	      first_unmodified_row = first_modified_row + field->rows;
	    }
	  if (first_unmodified_row != first_modified_row && is_public)
	    copywin(form->w,
		    formwin,
		    first_modified_row,
		    0,
		    field->frow + first_modified_row - form->toprow,
		    field->fcol,
		    field->frow + first_unmodified_row - form->toprow - 1,
		    field->cols + field->fcol - 1,
		    0);
	}
      if (is_public)
	wsyncup(formwin);
    }
  else
    {
       
      if (is_public)
	wsyncup(form->w);
    }
  untouchwin(form->w);
  returnCode(_nc_Position_Form_Cursor(form));
}

 
static void
Perform_Justification(FIELD *field, WINDOW *win)
{
  FIELD_CELL *bp;
  int len;

  bp = (Field_Has_Option(field, O_NO_LEFT_STRIP)
	? field->buf
	: Get_Start_Of_Data(field->buf, Buffer_Length(field)));
  len = (int)(After_End_Of_Data(field->buf, Buffer_Length(field)) - bp);

  if (len > 0)
    {
      int col = 0;

      assert(win && (field->drows == 1));

      if (field->cols - len >= 0)
	switch (field->just)
	  {
	  case JUSTIFY_LEFT:
	    break;
	  case JUSTIFY_CENTER:
	    col = (field->cols - len) / 2;
	    break;
	  case JUSTIFY_RIGHT:
	    col = field->cols - len;
	    break;
	  default:
	    break;
	  }

      wmove(win, 0, col);
      myADDNSTR(win, bp, len);
    }
}

 
static void
Undo_Justification(FIELD *field, WINDOW *win)
{
  FIELD_CELL *bp;
  int y, x;
  int len;

  getyx(win, y, x);

  bp = (Field_Has_Option(field, O_NO_LEFT_STRIP)
	? field->buf
	: Get_Start_Of_Data(field->buf, Buffer_Length(field)));
  len = (int)(After_End_Of_Data(field->buf, Buffer_Length(field)) - bp);

  if (len > 0)
    {
      assert(win);
      wmove(win, 0, 0);
      myADDNSTR(win, bp, len);
    }
  wmove(win, y, x);
}

 
static bool
Check_Char(FORM *form,
	   FIELD *field,
	   FIELDTYPE *typ,
	   int ch,
	   TypeArgument *argp)
{
  if (typ)
    {
      if (typ->status & _LINKED_TYPE)
	{
	  assert(argp);
	  return (
		   Check_Char(form, field, typ->left, ch, argp->left) ||
		   Check_Char(form, field, typ->right, ch, argp->right));
	}
      else
	{
#if NCURSES_INTEROP_FUNCS
	  if (typ->charcheck.occheck)
	    {
	      if (typ->status & _GENERIC)
		return typ->charcheck.gccheck(ch, form, field, (void *)argp);
	      else
		return typ->charcheck.occheck(ch, (void *)argp);
	    }
#else
	  if (typ->ccheck)
	    return typ->ccheck(ch, (void *)argp);
#endif
	}
    }
  return (!iscntrl(UChar(ch)) ? TRUE : FALSE);
}

 
static int
Display_Or_Erase_Field(FIELD *field, bool bEraseFlag)
{
  WINDOW *win;
  WINDOW *fwin;

  if (!field)
    return E_SYSTEM_ERROR;

  fwin = Get_Form_Window(field->form);
  win = derwin(fwin,
	       field->rows, field->cols, field->frow, field->fcol);

  if (!win)
    return E_SYSTEM_ERROR;
  else
    {
      if (Field_Has_Option(field, O_VISIBLE))
	{
	  Set_Field_Window_Attributes(field, win);
	}
      else
	{
	  (void)wattrset(win, (int)WINDOW_ATTRS(fwin));
	}
      werase(win);
    }

  if (!bEraseFlag)
    {
      if (Field_Has_Option(field, O_PUBLIC))
	{
	  if (Justification_Allowed(field))
	    Perform_Justification(field, win);
	  else
	    Buffer_To_Window(field, win);
	}
      ClrStatus(field, _NEWTOP);
    }
  wsyncup(win);
  delwin(win);
  return E_OK;
}

 
#define Display_Field(field) Display_Or_Erase_Field(field,FALSE)
#define Erase_Field(field)   Display_Or_Erase_Field(field,TRUE)

 
static int
Synchronize_Field(FIELD *field)
{
  FORM *form;
  int res = E_OK;

  if (!field)
    return (E_BAD_ARGUMENT);

  if (((form = field->form) != (FORM *)0)
      && Field_Really_Appears(field))
    {
      if (field == form->current)
	{
	  form->currow = form->curcol = form->toprow = form->begincol = 0;
	  werase(form->w);

	  if ((Field_Has_Option(field, O_PUBLIC)) && Justification_Allowed(field))
	    Undo_Justification(field, form->w);
	  else
	    Buffer_To_Window(field, form->w);

	  SetStatus(field, _NEWTOP);
	  res = _nc_Refresh_Current_Field(form);
	}
      else
	res = Display_Field(field);
    }
  SetStatus(field, _CHANGED);
  return (res);
}

 
static int
Synchronize_Linked_Fields(FIELD *field)
{
  FIELD *linked_field;
  int res = E_OK;

  if (!field)
    return (E_BAD_ARGUMENT);

  if (!field->link)
    return (E_SYSTEM_ERROR);

  for (linked_field = field->link;
       (linked_field != field) && (linked_field != 0);
       linked_field = linked_field->link)
    {
      int syncres;

      if (((syncres = Synchronize_Field(linked_field)) != E_OK) &&
	  (res == E_OK))
	res = syncres;
    }
  return (res);
}

 
FORM_EXPORT(int)
_nc_Synchronize_Attributes(FIELD *field)
{
  FORM *form;
  int res = E_OK;

  T((T_CALLED("_nc_Synchronize_Attributes(%p)"), (void *)field));

  if (!field)
    returnCode(E_BAD_ARGUMENT);

  CHECKPOS(field->form);
  if (((form = field->form) != (FORM *)0)
      && Field_Really_Appears(field))
    {
      if (form->current == field)
	{
	  Synchronize_Buffer(form);
	  Set_Field_Window_Attributes(field, form->w);
	  werase(form->w);
	  wmove(form->w, form->currow, form->curcol);

	  if (Field_Has_Option(field, O_PUBLIC))
	    {
	      if (Justification_Allowed(field))
		Undo_Justification(field, form->w);
	      else
		Buffer_To_Window(field, form->w);
	    }
	  else
	    {
	      WINDOW *formwin = Get_Form_Window(form);

	      copywin(form->w, formwin,
		      0, 0,
		      field->frow, field->fcol,
		      field->frow + field->rows - 1,
		      field->fcol + field->cols - 1, 0);
	      wsyncup(formwin);
	      Buffer_To_Window(field, form->w);
	      SetStatus(field, _NEWTOP);	 
	      _nc_Refresh_Current_Field(form);
	    }
	}
      else
	{
	  res = Display_Field(field);
	}
    }
  CHECKPOS(form);
  returnCode(res);
}

 
FORM_EXPORT(int)
_nc_Synchronize_Options(FIELD *field, Field_Options newopts)
{
  Field_Options oldopts;
  Field_Options changed_opts;
  FORM *form;
  int res = E_OK;

  T((T_CALLED("_nc_Synchronize_Options(%p,%#x)"), (void *)field, newopts));

  if (!field)
    returnCode(E_BAD_ARGUMENT);

  oldopts = field->opts;
  changed_opts = oldopts ^ newopts;
  field->opts = newopts;
  form = field->form;

  if (form)
    {
      if (form->status & _POSTED)
	{
	  if (form->current == field)
	    {
	      field->opts = oldopts;
	      returnCode(E_CURRENT);
	    }
	  if (form->curpage == field->page)
	    {
	      if ((unsigned)changed_opts & O_VISIBLE)
		{
		  if ((unsigned)newopts & O_VISIBLE)
		    res = Display_Field(field);
		  else
		    res = Erase_Field(field);
		}
	      else
		{
		  if (((unsigned)changed_opts & O_PUBLIC) &&
		      ((unsigned)newopts & O_VISIBLE))
		    res = Display_Field(field);
		}
	    }
	}
    }

  if ((unsigned)changed_opts & O_STATIC)
    {
      bool single_line_field = Single_Line_Field(field);
      int res2 = E_OK;

      if ((unsigned)newopts & O_STATIC)
	{
	   
	  ClrStatus(field, _MAY_GROW);
	   
	  if (single_line_field &&
	      (field->cols == field->dcols) &&
	      (field->just != NO_JUSTIFICATION) &&
	      Field_Really_Appears(field))
	    {
	      res2 = Display_Field(field);
	    }
	}
      else
	{
	   
	  if ((field->maxgrow == 0) ||
	      (single_line_field && (field->dcols < field->maxgrow)) ||
	      (!single_line_field && (field->drows < field->maxgrow)))
	    {
	      SetStatus(field, _MAY_GROW);
	       
	      if (single_line_field &&
		  (field->just != NO_JUSTIFICATION) &&
		  Field_Really_Appears(field))
		{
		  res2 = Display_Field(field);
		}
	    }
	}
      if (res2 != E_OK)
	res = res2;
    }

  returnCode(res);
}

 
void
_nc_Unset_Current_Field(FORM *form)
{
  FIELD *field = form->current;

  _nc_Refresh_Current_Field(form);
  if (Field_Has_Option(field, O_PUBLIC))
    {
      if (field->drows > field->rows)
	{
	  if (form->toprow == 0)
	    ClrStatus(field, _NEWTOP);
	  else
	    SetStatus(field, _NEWTOP);
	}
      else
	{
	  if (Justification_Allowed(field))
	    {
	      Window_To_Buffer(form, field);
	      werase(form->w);
	      Perform_Justification(field, form->w);
	      if (Field_Has_Option(field, O_DYNAMIC_JUSTIFY) &&
		  (form->w->_parent == 0))
		{
		  copywin(form->w,
			  Get_Form_Window(form),
			  0,
			  0,
			  field->frow,
			  field->fcol,
			  field->frow,
			  field->cols + field->fcol - 1,
			  0);
		  wsyncup(Get_Form_Window(form));
		}
	      else
		{
		  wsyncup(form->w);
		}
	    }
	}
    }
  delwin(form->w);
  form->w = (WINDOW *)0;
  form->current = 0;
}

 
FORM_EXPORT(int)
_nc_Set_Current_Field(FORM *form, FIELD *newfield)
{
  FIELD *field;
  WINDOW *new_window;

  T((T_CALLED("_nc_Set_Current_Field(%p,%p)"), (void *)form, (void *)newfield));

  if (!form || !newfield || (newfield->form != form))
    returnCode(E_BAD_ARGUMENT);

  if ((form->status & _IN_DRIVER))
    returnCode(E_BAD_STATE);

  if (!(form->field))
    returnCode(E_NOT_CONNECTED);

  field = form->current;

  if ((field != newfield) ||
      !(form->status & _POSTED))
    {
      if (field && (form->w) &&
	  (Field_Has_Option(field, O_VISIBLE)) &&
	  (field->form->curpage == field->page))
	_nc_Unset_Current_Field(form);

      field = newfield;

      if (Has_Invisible_Parts(field))
	new_window = newpad(field->drows, field->dcols);
      else
	new_window = derwin(Get_Form_Window(form),
			    field->rows, field->cols, field->frow, field->fcol);

      if (!new_window)
	returnCode(E_SYSTEM_ERROR);

      form->current = field;

      if (form->w)
	delwin(form->w);
      form->w = new_window;

      ClrStatus(form, _WINDOW_MODIFIED);
      Set_Field_Window_Attributes(field, form->w);

      if (Has_Invisible_Parts(field))
	{
	  werase(form->w);
	  Buffer_To_Window(field, form->w);
	}
      else
	{
	  if (Justification_Allowed(field))
	    {
	      werase(form->w);
	      Undo_Justification(field, form->w);
	      wsyncup(form->w);
	    }
	}

      untouchwin(form->w);
    }

  form->currow = form->curcol = form->toprow = form->begincol = 0;
  returnCode(E_OK);
}

 

 
static int
IFN_Next_Character(FORM *form)
{
  FIELD *field = form->current;
  int step = myWCWIDTH(form->w, form->currow, form->curcol);

  T((T_CALLED("IFN_Next_Character(%p)"), (void *)form));
  if ((form->curcol += step) == field->dcols)
    {
      if ((++(form->currow)) == field->drows)
	{
#if GROW_IF_NAVIGATE
	  if (!Single_Line_Field(field) && Field_Grown(field, 1))
	    {
	      form->curcol = 0;
	      returnCode(E_OK);
	    }
#endif
	  form->currow--;
#if GROW_IF_NAVIGATE
	  if (Single_Line_Field(field) && Field_Grown(field, 1))
	    returnCode(E_OK);
#endif
	  form->curcol -= step;
	  returnCode(E_REQUEST_DENIED);
	}
      form->curcol = 0;
    }
  returnCode(E_OK);
}

 
static int
IFN_Previous_Character(FORM *form)
{
  int amount = myWCWIDTH(form->w, form->currow, form->curcol - 1);
  int oldcol = form->curcol;

  T((T_CALLED("IFN_Previous_Character(%p)"), (void *)form));
  if ((form->curcol -= amount) < 0)
    {
      if ((--(form->currow)) < 0)
	{
	  form->currow++;
	  form->curcol = oldcol;
	  returnCode(E_REQUEST_DENIED);
	}
      form->curcol = form->current->dcols - 1;
    }
  returnCode(E_OK);
}

 
static int
IFN_Next_Line(FORM *form)
{
  FIELD *field = form->current;

  T((T_CALLED("IFN_Next_Line(%p)"), (void *)form));
  if ((++(form->currow)) == field->drows)
    {
#if GROW_IF_NAVIGATE
      if (!Single_Line_Field(field) && Field_Grown(field, 1))
	returnCode(E_OK);
#endif
      form->currow--;
      returnCode(E_REQUEST_DENIED);
    }
  form->curcol = 0;
  returnCode(E_OK);
}

 
static int
IFN_Previous_Line(FORM *form)
{
  T((T_CALLED("IFN_Previous_Line(%p)"), (void *)form));
  if ((--(form->currow)) < 0)
    {
      form->currow++;
      returnCode(E_REQUEST_DENIED);
    }
  form->curcol = 0;
  returnCode(E_OK);
}

 
static int
IFN_Next_Word(FORM *form)
{
  FIELD *field = form->current;
  FIELD_CELL *bp = Address_Of_Current_Position_In_Buffer(form);
  FIELD_CELL *s;
  FIELD_CELL *t;

  T((T_CALLED("IFN_Next_Word(%p)"), (void *)form));

   
  Synchronize_Buffer(form);

   
  s = Get_First_Whitespace_Character(bp, Buffer_Length(field) -
				     (int)(bp - field->buf));

   
  t = Get_Start_Of_Data(s, Buffer_Length(field) -
			(int)(s - field->buf));
#if !FRIENDLY_PREV_NEXT_WORD
  if (s == t)
    returnCode(E_REQUEST_DENIED);
  else
#endif
    {
      Adjust_Cursor_Position(form, t);
      returnCode(E_OK);
    }
}

 
static int
IFN_Previous_Word(FORM *form)
{
  FIELD *field = form->current;
  FIELD_CELL *bp = Address_Of_Current_Position_In_Buffer(form);
  FIELD_CELL *s;
  FIELD_CELL *t;
  bool again = FALSE;

  T((T_CALLED("IFN_Previous_Word(%p)"), (void *)form));

   
  Synchronize_Buffer(form);

  s = After_End_Of_Data(field->buf, (int)(bp - field->buf));
   
  if (s == bp)
    again = TRUE;

   
  t = After_Last_Whitespace_Character(field->buf, (int)(s - field->buf));
#if !FRIENDLY_PREV_NEXT_WORD
  if (s == t)
    returnCode(E_REQUEST_DENIED);
#endif
  if (again)
    {
       
      s = After_End_Of_Data(field->buf, (int)(t - field->buf));
      t = After_Last_Whitespace_Character(field->buf, (int)(s - field->buf));
#if !FRIENDLY_PREV_NEXT_WORD
      if (s == t)
	returnCode(E_REQUEST_DENIED);
#endif
    }
  Adjust_Cursor_Position(form, t);
  returnCode(E_OK);
}

 
static int
IFN_Beginning_Of_Field(FORM *form)
{
  FIELD *field = form->current;

  T((T_CALLED("IFN_Beginning_Of_Field(%p)"), (void *)form));
  Synchronize_Buffer(form);
  Adjust_Cursor_Position(form,
			 Get_Start_Of_Data(field->buf, Buffer_Length(field)));
  returnCode(E_OK);
}

 
static int
IFN_End_Of_Field(FORM *form)
{
  FIELD *field = form->current;
  FIELD_CELL *pos;

  T((T_CALLED("IFN_End_Of_Field(%p)"), (void *)form));
  Synchronize_Buffer(form);
  pos = After_End_Of_Data(field->buf, Buffer_Length(field));
  if (pos == (field->buf + Buffer_Length(field)))
    pos--;
  Adjust_Cursor_Position(form, pos);
  returnCode(E_OK);
}

 
static int
IFN_Beginning_Of_Line(FORM *form)
{
  FIELD *field = form->current;

  T((T_CALLED("IFN_Beginning_Of_Line(%p)"), (void *)form));
  Synchronize_Buffer(form);
  Adjust_Cursor_Position(form,
			 Get_Start_Of_Data(Address_Of_Current_Row_In_Buffer(form),
					   field->dcols));
  returnCode(E_OK);
}

 
static int
IFN_End_Of_Line(FORM *form)
{
  FIELD *field = form->current;
  FIELD_CELL *pos;
  FIELD_CELL *bp;

  T((T_CALLED("IFN_End_Of_Line(%p)"), (void *)form));
  Synchronize_Buffer(form);
  bp = Address_Of_Current_Row_In_Buffer(form);
  pos = After_End_Of_Data(bp, field->dcols);
  if (pos == (bp + field->dcols))
    pos--;
  Adjust_Cursor_Position(form, pos);
  returnCode(E_OK);
}

 
static int
IFN_Left_Character(FORM *form)
{
  int amount = myWCWIDTH(form->w, form->currow, form->curcol - 1);
  int oldcol = form->curcol;

  T((T_CALLED("IFN_Left_Character(%p)"), (void *)form));
  if ((form->curcol -= amount) < 0)
    {
      form->curcol = oldcol;
      returnCode(E_REQUEST_DENIED);
    }
  returnCode(E_OK);
}

 
static int
IFN_Right_Character(FORM *form)
{
  int amount = myWCWIDTH(form->w, form->currow, form->curcol);
  int oldcol = form->curcol;

  T((T_CALLED("IFN_Right_Character(%p)"), (void *)form));
  if ((form->curcol += amount) >= form->current->dcols)
    {
#if GROW_IF_NAVIGATE
      FIELD *field = form->current;

      if (Single_Line_Field(field) && Field_Grown(field, 1))
	returnCode(E_OK);
#endif
      form->curcol = oldcol;
      returnCode(E_REQUEST_DENIED);
    }
  returnCode(E_OK);
}

 
static int
IFN_Up_Character(FORM *form)
{
  T((T_CALLED("IFN_Up_Character(%p)"), (void *)form));
  if ((--(form->currow)) < 0)
    {
      form->currow++;
      returnCode(E_REQUEST_DENIED);
    }
  returnCode(E_OK);
}

 
static int
IFN_Down_Character(FORM *form)
{
  FIELD *field = form->current;

  T((T_CALLED("IFN_Down_Character(%p)"), (void *)form));
  if ((++(form->currow)) == field->drows)
    {
#if GROW_IF_NAVIGATE
      if (!Single_Line_Field(field) && Field_Grown(field, 1))
	returnCode(E_OK);
#endif
      --(form->currow);
      returnCode(E_REQUEST_DENIED);
    }
  returnCode(E_OK);
}
 

 

 
static int
VSC_Generic(FORM *form, int nlines)
{
  FIELD *field = form->current;
  int res = E_REQUEST_DENIED;
  int rows_to_go = (nlines > 0 ? nlines : -nlines);

  if (nlines > 0)
    {
      if ((rows_to_go + form->toprow) > (field->drows - field->rows))
	rows_to_go = (field->drows - field->rows - form->toprow);

      if (rows_to_go > 0)
	{
	  form->currow += rows_to_go;
	  form->toprow += rows_to_go;
	  res = E_OK;
	}
    }
  else
    {
      if (rows_to_go > form->toprow)
	rows_to_go = form->toprow;

      if (rows_to_go > 0)
	{
	  form->currow -= rows_to_go;
	  form->toprow -= rows_to_go;
	  res = E_OK;
	}
    }
  return (res);
}
 

 

 
static int
Vertical_Scrolling(int (*const fct) (FORM *), FORM *form)
{
  int res = E_REQUEST_DENIED;

  if (!Single_Line_Field(form->current))
    {
      res = fct(form);
      if (res == E_OK)
	SetStatus(form->current, _NEWTOP);
    }
  return (res);
}

 
static int
VSC_Scroll_Line_Forward(FORM *form)
{
  T((T_CALLED("VSC_Scroll_Line_Forward(%p)"), (void *)form));
  returnCode(VSC_Generic(form, 1));
}

 
static int
VSC_Scroll_Line_Backward(FORM *form)
{
  T((T_CALLED("VSC_Scroll_Line_Backward(%p)"), (void *)form));
  returnCode(VSC_Generic(form, -1));
}

 
static int
VSC_Scroll_Page_Forward(FORM *form)
{
  T((T_CALLED("VSC_Scroll_Page_Forward(%p)"), (void *)form));
  returnCode(VSC_Generic(form, form->current->rows));
}

 
static int
VSC_Scroll_Half_Page_Forward(FORM *form)
{
  T((T_CALLED("VSC_Scroll_Half_Page_Forward(%p)"), (void *)form));
  returnCode(VSC_Generic(form, (form->current->rows + 1) / 2));
}

 
static int
VSC_Scroll_Page_Backward(FORM *form)
{
  T((T_CALLED("VSC_Scroll_Page_Backward(%p)"), (void *)form));
  returnCode(VSC_Generic(form, -(form->current->rows)));
}

 
static int
VSC_Scroll_Half_Page_Backward(FORM *form)
{
  T((T_CALLED("VSC_Scroll_Half_Page_Backward(%p)"), (void *)form));
  returnCode(VSC_Generic(form, -((form->current->rows + 1) / 2)));
}
 

 

 
static int
HSC_Generic(FORM *form, int ncolumns)
{
  FIELD *field = form->current;
  int res = E_REQUEST_DENIED;
  int cols_to_go = (ncolumns > 0 ? ncolumns : -ncolumns);

  if (ncolumns > 0)
    {
      if ((cols_to_go + form->begincol) > (field->dcols - field->cols))
	cols_to_go = field->dcols - field->cols - form->begincol;

      if (cols_to_go > 0)
	{
	  form->curcol += cols_to_go;
	  form->begincol += cols_to_go;
	  res = E_OK;
	}
    }
  else
    {
      if (cols_to_go > form->begincol)
	cols_to_go = form->begincol;

      if (cols_to_go > 0)
	{
	  form->curcol -= cols_to_go;
	  form->begincol -= cols_to_go;
	  res = E_OK;
	}
    }
  return (res);
}
 

 

 
static int
Horizontal_Scrolling(int (*const fct) (FORM *), FORM *form)
{
  if (Single_Line_Field(form->current))
    return fct(form);
  else
    return (E_REQUEST_DENIED);
}

 
static int
HSC_Scroll_Char_Forward(FORM *form)
{
  T((T_CALLED("HSC_Scroll_Char_Forward(%p)"), (void *)form));
  returnCode(HSC_Generic(form, 1));
}

 
static int
HSC_Scroll_Char_Backward(FORM *form)
{
  T((T_CALLED("HSC_Scroll_Char_Backward(%p)"), (void *)form));
  returnCode(HSC_Generic(form, -1));
}

 
static int
HSC_Horizontal_Line_Forward(FORM *form)
{
  T((T_CALLED("HSC_Horizontal_Line_Forward(%p)"), (void *)form));
  returnCode(HSC_Generic(form, form->current->cols));
}

 
static int
HSC_Horizontal_Half_Line_Forward(FORM *form)
{
  T((T_CALLED("HSC_Horizontal_Half_Line_Forward(%p)"), (void *)form));
  returnCode(HSC_Generic(form, (form->current->cols + 1) / 2));
}

 
static int
HSC_Horizontal_Line_Backward(FORM *form)
{
  T((T_CALLED("HSC_Horizontal_Line_Backward(%p)"), (void *)form));
  returnCode(HSC_Generic(form, -(form->current->cols)));
}

 
static int
HSC_Horizontal_Half_Line_Backward(FORM *form)
{
  T((T_CALLED("HSC_Horizontal_Half_Line_Backward(%p)"), (void *)form));
  returnCode(HSC_Generic(form, -((form->current->cols + 1) / 2)));
}

 

 

 
NCURSES_INLINE static bool
Is_There_Room_For_A_Line(FORM *form)
{
  FIELD *field = form->current;
  FIELD_CELL *begin_of_last_line, *s;

  Synchronize_Buffer(form);
  begin_of_last_line = Address_Of_Row_In_Buffer(field, (field->drows - 1));
  s = After_End_Of_Data(begin_of_last_line, field->dcols);
  return ((s == begin_of_last_line) ? TRUE : FALSE);
}

 
NCURSES_INLINE static bool
Is_There_Room_For_A_Char_In_Line(FORM *form)
{
  int last_char_in_line;

  wmove(form->w, form->currow, form->current->dcols - 1);
  last_char_in_line = (int)(winch(form->w) & A_CHARTEXT);
  wmove(form->w, form->currow, form->curcol);
  return (((last_char_in_line == form->current->pad) ||
	   is_blank(last_char_in_line)) ? TRUE : FALSE);
}

#define There_Is_No_Room_For_A_Char_In_Line(f) \
  !Is_There_Room_For_A_Char_In_Line(f)

 
static int
Insert_String(FORM *form, int row, FIELD_CELL *txt, int len)
{
  FIELD *field = form->current;
  FIELD_CELL *bp = Address_Of_Row_In_Buffer(field, row);
  int datalen = (int)(After_End_Of_Data(bp, field->dcols) - bp);
  int freelen = field->dcols - datalen;
  int requiredlen = len + 1;
  int result = E_REQUEST_DENIED;

  if (freelen >= requiredlen)
    {
      wmove(form->w, row, 0);
      myINSNSTR(form->w, txt, len);
      wmove(form->w, row, len);
      myINSNSTR(form->w, &myBLANK, 1);
      result = E_OK;
    }
  else
    {
       
      if ((row == (field->drows - 1)) && Growable(field))
	{
	  if (!Field_Grown(field, 1))
	    return (E_SYSTEM_ERROR);
	   
	  bp = Address_Of_Row_In_Buffer(field, row);
	}

      if (row < (field->drows - 1))
	{
	  FIELD_CELL *split;

	  split =
	    After_Last_Whitespace_Character(bp,
					    (int)(Get_Start_Of_Data(bp
								    + field->dcols
								    - requiredlen,
								    requiredlen)
						  - bp));
	   
	  datalen = (int)(split - bp);	 
	  freelen = field->dcols - (datalen + freelen);		 

	  if ((result = Insert_String(form, row + 1, split, freelen)) == E_OK)
	    {
	      wmove(form->w, row, datalen);
	      wclrtoeol(form->w);
	      wmove(form->w, row, 0);
	      myINSNSTR(form->w, txt, len);
	      wmove(form->w, row, len);
	      myINSNSTR(form->w, &myBLANK, 1);
	      return E_OK;
	    }
	}
    }
  return (result);
}

 
static int
Wrapping_Not_Necessary_Or_Wrapping_Ok(FORM *form)
{
  FIELD *field = form->current;
  int result = E_REQUEST_DENIED;
  bool Last_Row = ((field->drows - 1) == form->currow);

  if ((Field_Has_Option(field, O_WRAP)) &&	 
      (!Single_Line_Field(field)) &&	 
      (There_Is_No_Room_For_A_Char_In_Line(form)) &&	 
      (!Last_Row || Growable(field)))	 
    {
      FIELD_CELL *bp;
      FIELD_CELL *split;
      int chars_to_be_wrapped;
      int chars_to_remain_on_line;

      if (Last_Row)
	{
	   
	  if (!Field_Grown(field, 1))
	    return E_SYSTEM_ERROR;
	}
      bp = Address_Of_Current_Row_In_Buffer(form);
      Window_To_Buffer(form, field);
      split = After_Last_Whitespace_Character(bp, field->dcols);
       
      chars_to_remain_on_line = (int)(split - bp);
      chars_to_be_wrapped = field->dcols - chars_to_remain_on_line;
      if (chars_to_remain_on_line > 0)
	{
	  if ((result = Insert_String(form, form->currow + 1, split,
				      chars_to_be_wrapped)) == E_OK)
	    {
	      wmove(form->w, form->currow, chars_to_remain_on_line);
	      wclrtoeol(form->w);
	      if (form->curcol >= chars_to_remain_on_line)
		{
		  form->currow++;
		  form->curcol -= chars_to_remain_on_line;
		}
	      return E_OK;
	    }
	}
      else
	return E_OK;
      if (result != E_OK)
	{
	  DeleteChar(form);
	  Window_To_Buffer(form, field);
	  result = E_REQUEST_DENIED;
	}
    }
  else
    result = E_OK;		 
  return (result);
}

 

 
static int
Field_Editing(int (*const fct) (FORM *), FORM *form)
{
  int res = E_REQUEST_DENIED;

   
  if ((fct == FE_Delete_Previous) &&
      ((unsigned)form->opts & O_BS_OVERLOAD) &&
      First_Position_In_Current_Field(form))
    {
      res = Inter_Field_Navigation(FN_Previous_Field, form);
    }
  else
    {
      if (fct == FE_New_Line)
	{
	  if (((unsigned)form->opts & O_NL_OVERLOAD) &&
	      First_Position_In_Current_Field(form))
	    {
	      res = Inter_Field_Navigation(FN_Next_Field, form);
	    }
	  else
	     
	    res = fct(form);
	}
      else
	{
	   
	  if ((unsigned)form->current->opts & O_EDIT)
	    {
	      res = fct(form);
	      if (res == E_OK)
		SetStatus(form, _WINDOW_MODIFIED);
	    }
	}
    }
  return res;
}

 
static int
FE_New_Line(FORM *form)
{
  FIELD *field = form->current;
  FIELD_CELL *bp, *t;
  bool Last_Row = ((field->drows - 1) == form->currow);

  T((T_CALLED("FE_New_Line(%p)"), (void *)form));
  if (form->status & _OVLMODE)
    {
      if (Last_Row &&
	  (!(Growable(field) && !Single_Line_Field(field))))
	{
	  if (!((unsigned)form->opts & O_NL_OVERLOAD))
	    returnCode(E_REQUEST_DENIED);
	  wmove(form->w, form->currow, form->curcol);
	  wclrtoeol(form->w);
	   
	  SetStatus(form, _WINDOW_MODIFIED);
	  returnCode(Inter_Field_Navigation(FN_Next_Field, form));
	}
      else
	{
	  if (Last_Row && !Field_Grown(field, 1))
	    {
	       
	      returnCode(E_SYSTEM_ERROR);
	    }
	  wmove(form->w, form->currow, form->curcol);
	  wclrtoeol(form->w);
	  form->currow++;
	  form->curcol = 0;
	  SetStatus(form, _WINDOW_MODIFIED);
	  returnCode(E_OK);
	}
    }
  else
    {
       
      if (Last_Row &&
	  !(Growable(field) && !Single_Line_Field(field)))
	{
	  if (!((unsigned)form->opts & O_NL_OVERLOAD))
	    returnCode(E_REQUEST_DENIED);
	  returnCode(Inter_Field_Navigation(FN_Next_Field, form));
	}
      else
	{
	  bool May_Do_It = !Last_Row && Is_There_Room_For_A_Line(form);

	  if (!(May_Do_It || Growable(field)))
	    returnCode(E_REQUEST_DENIED);
	  if (!May_Do_It && !Field_Grown(field, 1))
	    returnCode(E_SYSTEM_ERROR);

	  bp = Address_Of_Current_Position_In_Buffer(form);
	  t = After_End_Of_Data(bp, field->dcols - form->curcol);
	  wmove(form->w, form->currow, form->curcol);
	  wclrtoeol(form->w);
	  form->currow++;
	  form->curcol = 0;
	  wmove(form->w, form->currow, form->curcol);
	  winsertln(form->w);
	  myADDNSTR(form->w, bp, (int)(t - bp));
	  SetStatus(form, _WINDOW_MODIFIED);
	  returnCode(E_OK);
	}
    }
}

 
static int
FE_Insert_Character(FORM *form)
{
  FIELD *field = form->current;
  int result = E_REQUEST_DENIED;

  T((T_CALLED("FE_Insert_Character(%p)"), (void *)form));
  if (Check_Char(form, field, field->type, (int)C_BLANK,
		 (TypeArgument *)(field->arg)))
    {
      bool There_Is_Room = Is_There_Room_For_A_Char_In_Line(form);

      if (There_Is_Room ||
	  ((Single_Line_Field(field) && Growable(field))))
	{
	  if (!There_Is_Room && !Field_Grown(field, 1))
	    result = E_SYSTEM_ERROR;
	  else
	    {
	      winsch(form->w, (chtype)C_BLANK);
	      result = Wrapping_Not_Necessary_Or_Wrapping_Ok(form);
	    }
	}
    }
  returnCode(result);
}

 
static int
FE_Insert_Line(FORM *form)
{
  FIELD *field = form->current;
  int result = E_REQUEST_DENIED;

  T((T_CALLED("FE_Insert_Line(%p)"), (void *)form));
  if (Check_Char(form, field,
		 field->type, (int)C_BLANK, (TypeArgument *)(field->arg)))
    {
      bool Maybe_Done = (form->currow != (field->drows - 1)) &&
      Is_There_Room_For_A_Line(form);

      if (!Single_Line_Field(field) &&
	  (Maybe_Done || Growable(field)))
	{
	  if (!Maybe_Done && !Field_Grown(field, 1))
	    result = E_SYSTEM_ERROR;
	  else
	    {
	      form->curcol = 0;
	      winsertln(form->w);
	      result = E_OK;
	    }
	}
    }
  returnCode(result);
}

 
static int
FE_Delete_Character(FORM *form)
{
  T((T_CALLED("FE_Delete_Character(%p)"), (void *)form));
  DeleteChar(form);
  returnCode(E_OK);
}

 
static int
FE_Delete_Previous(FORM *form)
{
  FIELD *field = form->current;

  T((T_CALLED("FE_Delete_Previous(%p)"), (void *)form));
  if (First_Position_In_Current_Field(form))
    returnCode(E_REQUEST_DENIED);

  if ((--(form->curcol)) < 0)
    {
      FIELD_CELL *this_line, *prev_line, *prev_end, *this_end;
      int this_row = form->currow;

      form->curcol++;
      if (form->status & _OVLMODE)
	returnCode(E_REQUEST_DENIED);

      prev_line = Address_Of_Row_In_Buffer(field, (form->currow - 1));
      this_line = Address_Of_Row_In_Buffer(field, (form->currow));
      Synchronize_Buffer(form);
      prev_end = After_End_Of_Data(prev_line, field->dcols);
      this_end = After_End_Of_Data(this_line, field->dcols);
      if ((int)(this_end - this_line) >
	  (field->cols - (int)(prev_end - prev_line)))
	returnCode(E_REQUEST_DENIED);
      wmove(form->w, form->currow, form->curcol);
      wdeleteln(form->w);
      Adjust_Cursor_Position(form, prev_end);
       
      if (form->currow == this_row && this_row > 0)
	{
	  form->currow -= 1;
	  form->curcol = field->dcols - 1;
	  DeleteChar(form);
	}
      else
	{
	  wmove(form->w, form->currow, form->curcol);
	  myADDNSTR(form->w, this_line, (int)(this_end - this_line));
	}
    }
  else
    {
      DeleteChar(form);
    }
  returnCode(E_OK);
}

 
static int
FE_Delete_Line(FORM *form)
{
  T((T_CALLED("FE_Delete_Line(%p)"), (void *)form));
  form->curcol = 0;
  wdeleteln(form->w);
  returnCode(E_OK);
}

 
static int
FE_Delete_Word(FORM *form)
{
  FIELD *field = form->current;
  FIELD_CELL *bp = Address_Of_Current_Row_In_Buffer(form);
  FIELD_CELL *ep = bp + field->dcols;
  FIELD_CELL *cp = bp + form->curcol;
  FIELD_CELL *s;

  T((T_CALLED("FE_Delete_Word(%p)"), (void *)form));
  Synchronize_Buffer(form);
  if (ISBLANK(*cp))
    returnCode(E_REQUEST_DENIED);	 

   
  Adjust_Cursor_Position(form,
			 After_Last_Whitespace_Character(bp, form->curcol));
  wmove(form->w, form->currow, form->curcol);
  wclrtoeol(form->w);

   
  s = Get_First_Whitespace_Character(cp, (int)(ep - cp));
   
  s = Get_Start_Of_Data(s, (int)(ep - s));
  if ((s != cp) && !ISBLANK(*s))
    {
       
      myADDNSTR(form->w, s, (int)(s - After_End_Of_Data(s, (int)(ep - s))));
    }
  returnCode(E_OK);
}

 
static int
FE_Clear_To_End_Of_Line(FORM *form)
{
  T((T_CALLED("FE_Clear_To_End_Of_Line(%p)"), (void *)form));
  wmove(form->w, form->currow, form->curcol);
  wclrtoeol(form->w);
  returnCode(E_OK);
}

 
static int
FE_Clear_To_End_Of_Field(FORM *form)
{
  T((T_CALLED("FE_Clear_To_End_Of_Field(%p)"), (void *)form));
  wmove(form->w, form->currow, form->curcol);
  wclrtobot(form->w);
  returnCode(E_OK);
}

 
static int
FE_Clear_Field(FORM *form)
{
  T((T_CALLED("FE_Clear_Field(%p)"), (void *)form));
  form->currow = form->curcol = 0;
  werase(form->w);
  returnCode(E_OK);
}
 

 

 
static int
EM_Overlay_Mode(FORM *form)
{
  T((T_CALLED("EM_Overlay_Mode(%p)"), (void *)form));
  SetStatus(form, _OVLMODE);
  returnCode(E_OK);
}

 
static int
EM_Insert_Mode(FORM *form)
{
  T((T_CALLED("EM_Insert_Mode(%p)"), (void *)form));
  ClrStatus(form, _OVLMODE);
  returnCode(E_OK);
}

 

 

 
static bool
Next_Choice(FORM *form, FIELDTYPE *typ, FIELD *field, TypeArgument *argp)
{
  if (!typ || !(typ->status & _HAS_CHOICE))
    return FALSE;

  if (typ->status & _LINKED_TYPE)
    {
      assert(argp);
      return (
	       Next_Choice(form, typ->left, field, argp->left) ||
	       Next_Choice(form, typ->right, field, argp->right));
    }
  else
    {
#if NCURSES_INTEROP_FUNCS
      assert(typ->enum_next.onext);
      if (typ->status & _GENERIC)
	return typ->enum_next.gnext(form, field, (void *)argp);
      else
	return typ->enum_next.onext(field, (void *)argp);
#else
      assert(typ->next);
      return typ->next(field, (void *)argp);
#endif
    }
}

 
static bool
Previous_Choice(FORM *form, FIELDTYPE *typ, FIELD *field, TypeArgument *argp)
{
  if (!typ || !(typ->status & _HAS_CHOICE))
    return FALSE;

  if (typ->status & _LINKED_TYPE)
    {
      assert(argp);
      return (
	       Previous_Choice(form, typ->left, field, argp->left) ||
	       Previous_Choice(form, typ->right, field, argp->right));
    }
  else
    {
#if NCURSES_INTEROP_FUNCS
      assert(typ->enum_prev.oprev);
      if (typ->status & _GENERIC)
	return typ->enum_prev.gprev(form, field, (void *)argp);
      else
	return typ->enum_prev.oprev(field, (void *)argp);
#else
      assert(typ->prev);
      return typ->prev(field, (void *)argp);
#endif
    }
}
 

 

 
static int
CR_Next_Choice(FORM *form)
{
  FIELD *field = form->current;

  T((T_CALLED("CR_Next_Choice(%p)"), (void *)form));
  Synchronize_Buffer(form);
  returnCode((Next_Choice(form, field->type, field, (TypeArgument *)(field->arg)))
	     ? E_OK
	     : E_REQUEST_DENIED);
}

 
static int
CR_Previous_Choice(FORM *form)
{
  FIELD *field = form->current;

  T((T_CALLED("CR_Previous_Choice(%p)"), (void *)form));
  Synchronize_Buffer(form);
  returnCode((Previous_Choice(form, field->type, field, (TypeArgument *)(field->arg)))
	     ? E_OK
	     : E_REQUEST_DENIED);
}
 

 

 
static bool
Check_Field(FORM *form, FIELDTYPE *typ, FIELD *field, TypeArgument *argp)
{
  if (typ)
    {
      if (Field_Has_Option(field, O_NULLOK))
	{
	  FIELD_CELL *bp = field->buf;

	  assert(bp);
	  while (ISBLANK(*bp))
	    {
	      bp++;
	    }
	  if (CharOf(*bp) == 0)
	    return TRUE;
	}

      if (typ->status & _LINKED_TYPE)
	{
	  assert(argp);
	  return (
		   Check_Field(form, typ->left, field, argp->left) ||
		   Check_Field(form, typ->right, field, argp->right));
	}
      else
	{
#if NCURSES_INTEROP_FUNCS
	  if (typ->fieldcheck.ofcheck)
	    {
	      if (typ->status & _GENERIC)
		return typ->fieldcheck.gfcheck(form, field, (void *)argp);
	      else
		return typ->fieldcheck.ofcheck(field, (void *)argp);
	    }
#else
	  if (typ->fcheck)
	    return typ->fcheck(field, (void *)argp);
#endif
	}
    }
  return TRUE;
}

 
FORM_EXPORT(bool)
_nc_Internal_Validation(FORM *form)
{
  FIELD *field;

  field = form->current;

  Synchronize_Buffer(form);
  if ((form->status & _FCHECK_REQUIRED) ||
      (!(Field_Has_Option(field, O_PASSOK))))
    {
      if (!Check_Field(form, field->type, field, (TypeArgument *)(field->arg)))
	return FALSE;
      ClrStatus(form, _FCHECK_REQUIRED);
      SetStatus(field, _CHANGED);
      Synchronize_Linked_Fields(field);
    }
  return TRUE;
}
 

 

 
static int
FV_Validation(FORM *form)
{
  T((T_CALLED("FV_Validation(%p)"), (void *)form));
  if (_nc_Internal_Validation(form))
    returnCode(E_OK);
  else
    returnCode(E_INVALID_FIELD);
}
 

 

 
NCURSES_INLINE static FIELD *
Next_Field_On_Page(FIELD *field)
{
  FORM *form = field->form;
  FIELD **field_on_page = &form->field[field->index];
  FIELD **first_on_page = &form->field[form->page[form->curpage].pmin];
  FIELD **last_on_page = &form->field[form->page[form->curpage].pmax];

  do
    {
      field_on_page =
	(field_on_page == last_on_page) ? first_on_page : field_on_page + 1;
      if (Field_Is_Selectable(*field_on_page))
	break;
    }
  while (field != (*field_on_page));
  return (*field_on_page);
}

 
FORM_EXPORT(FIELD *)
_nc_First_Active_Field(FORM *form)
{
  FIELD **last_on_page = &form->field[form->page[form->curpage].pmax];
  FIELD *proposed = Next_Field_On_Page(*last_on_page);

  if (proposed == *last_on_page)
    {
       
      if (Field_Is_Not_Selectable(proposed))
	{
	  FIELD **field = &form->field[proposed->index];
	  FIELD **first = &form->field[form->page[form->curpage].pmin];

	  do
	    {
	      field = (field == last_on_page) ? first : field + 1;
	      if (Field_Has_Option(*field, O_VISIBLE))
		break;
	    }
	  while (proposed != (*field));

	  proposed = *field;

	  if ((proposed == *last_on_page) &&
	      !((unsigned)proposed->opts & O_VISIBLE))
	    {
	       
	      proposed = *first;
	    }
	}
    }
  return (proposed);
}

 
NCURSES_INLINE static FIELD *
Previous_Field_On_Page(FIELD *field)
{
  FORM *form = field->form;
  FIELD **field_on_page = &form->field[field->index];
  FIELD **first_on_page = &form->field[form->page[form->curpage].pmin];
  FIELD **last_on_page = &form->field[form->page[form->curpage].pmax];

  do
    {
      field_on_page =
	(field_on_page == first_on_page) ? last_on_page : field_on_page - 1;
      if (Field_Is_Selectable(*field_on_page))
	break;
    }
  while (field != (*field_on_page));

  return (*field_on_page);
}

 
NCURSES_INLINE static FIELD *
Sorted_Next_Field(FIELD *field)
{
  FIELD *field_on_page = field;

  do
    {
      field_on_page = field_on_page->snext;
      if (Field_Is_Selectable(field_on_page))
	break;
    }
  while (field_on_page != field);

  return (field_on_page);
}

 
NCURSES_INLINE static FIELD *
Sorted_Previous_Field(FIELD *field)
{
  FIELD *field_on_page = field;

  do
    {
      field_on_page = field_on_page->sprev;
      if (Field_Is_Selectable(field_on_page))
	break;
    }
  while (field_on_page != field);

  return (field_on_page);
}

 
NCURSES_INLINE static FIELD *
Left_Neighbor_Field(FIELD *field)
{
  FIELD *field_on_page = field;

   
  do
    {
      field_on_page = Sorted_Previous_Field(field_on_page);
    }
  while (field_on_page->frow != field->frow);

  return (field_on_page);
}

 
NCURSES_INLINE static FIELD *
Right_Neighbor_Field(FIELD *field)
{
  FIELD *field_on_page = field;

   
  do
    {
      field_on_page = Sorted_Next_Field(field_on_page);
    }
  while (field_on_page->frow != field->frow);

  return (field_on_page);
}

 
static FIELD *
Upper_Neighbor_Field(FIELD *field)
{
  FIELD *field_on_page = field;
  int frow = field->frow;
  int fcol = field->fcol;

   
  do
    {
      field_on_page = Sorted_Previous_Field(field_on_page);
    }
  while (field_on_page->frow == frow && field_on_page->fcol != fcol);

  if (field_on_page->frow != frow)
    {
       
      frow = field_on_page->frow;

       
      while (field_on_page->frow == frow && field_on_page->fcol > fcol)
	field_on_page = Sorted_Previous_Field(field_on_page);

       
      if (field_on_page->frow != frow)
	field_on_page = Sorted_Next_Field(field_on_page);
    }

  return (field_on_page);
}

 
static FIELD *
Down_Neighbor_Field(FIELD *field)
{
  FIELD *field_on_page = field;
  int frow = field->frow;
  int fcol = field->fcol;

   
  do
    {
      field_on_page = Sorted_Next_Field(field_on_page);
    }
  while (field_on_page->frow == frow && field_on_page->fcol != fcol);

  if (field_on_page->frow != frow)
    {
       
      frow = field_on_page->frow;

       
      while (field_on_page->frow == frow && field_on_page->fcol < fcol)
	field_on_page = Sorted_Next_Field(field_on_page);

       
      if (field_on_page->frow != frow)
	field_on_page = Sorted_Previous_Field(field_on_page);
    }

  return (field_on_page);
}

 

 
static int
Inter_Field_Navigation(int (*const fct) (FORM *), FORM *form)
{
  int res;

  if (!_nc_Internal_Validation(form))
    res = E_INVALID_FIELD;
  else
    {
      Call_Hook(form, fieldterm);
      res = fct(form);
      Call_Hook(form, fieldinit);
    }
  return res;
}

 
static int
FN_Next_Field(FORM *form)
{
  T((T_CALLED("FN_Next_Field(%p)"), (void *)form));
  returnCode(_nc_Set_Current_Field(form,
				   Next_Field_On_Page(form->current)));
}

 
static int
FN_Previous_Field(FORM *form)
{
  T((T_CALLED("FN_Previous_Field(%p)"), (void *)form));
  returnCode(_nc_Set_Current_Field(form,
				   Previous_Field_On_Page(form->current)));
}

 
static int
FN_First_Field(FORM *form)
{
  T((T_CALLED("FN_First_Field(%p)"), (void *)form));
  returnCode(_nc_Set_Current_Field(form,
				   Next_Field_On_Page(form->field[form->page[form->curpage].pmax])));
}

 
static int
FN_Last_Field(FORM *form)
{
  T((T_CALLED("FN_Last_Field(%p)"), (void *)form));
  returnCode(
	      _nc_Set_Current_Field(form,
				    Previous_Field_On_Page(form->field[form->page[form->curpage].pmin])));
}

 
static int
FN_Sorted_Next_Field(FORM *form)
{
  T((T_CALLED("FN_Sorted_Next_Field(%p)"), (void *)form));
  returnCode(_nc_Set_Current_Field(form,
				   Sorted_Next_Field(form->current)));
}

 
static int
FN_Sorted_Previous_Field(FORM *form)
{
  T((T_CALLED("FN_Sorted_Previous_Field(%p)"), (void *)form));
  returnCode(_nc_Set_Current_Field(form,
				   Sorted_Previous_Field(form->current)));
}

 
static int
FN_Sorted_First_Field(FORM *form)
{
  T((T_CALLED("FN_Sorted_First_Field(%p)"), (void *)form));
  returnCode(_nc_Set_Current_Field(form,
				   Sorted_Next_Field(form->field[form->page[form->curpage].smax])));
}

 
static int
FN_Sorted_Last_Field(FORM *form)
{
  T((T_CALLED("FN_Sorted_Last_Field(%p)"), (void *)form));
  returnCode(_nc_Set_Current_Field(form,
				   Sorted_Previous_Field(form->field[form->page[form->curpage].smin])));
}

 
static int
FN_Left_Field(FORM *form)
{
  T((T_CALLED("FN_Left_Field(%p)"), (void *)form));
  returnCode(_nc_Set_Current_Field(form,
				   Left_Neighbor_Field(form->current)));
}

 
static int
FN_Right_Field(FORM *form)
{
  T((T_CALLED("FN_Right_Field(%p)"), (void *)form));
  returnCode(_nc_Set_Current_Field(form,
				   Right_Neighbor_Field(form->current)));
}

 
static int
FN_Up_Field(FORM *form)
{
  T((T_CALLED("FN_Up_Field(%p)"), (void *)form));
  returnCode(_nc_Set_Current_Field(form,
				   Upper_Neighbor_Field(form->current)));
}

 
static int
FN_Down_Field(FORM *form)
{
  T((T_CALLED("FN_Down_Field(%p)"), (void *)form));
  returnCode(_nc_Set_Current_Field(form,
				   Down_Neighbor_Field(form->current)));
}
 

 

 
FORM_EXPORT(int)
_nc_Set_Form_Page(FORM *form, int page, FIELD *field)
{
  int res = E_OK;

  if ((form->curpage != page))
    {
      FIELD *last_field, *field_on_page;

      werase(Get_Form_Window(form));
      form->curpage = (short)page;
      last_field = field_on_page = form->field[form->page[page].smin];
      do
	{
	  if ((unsigned)field_on_page->opts & O_VISIBLE)
	    if ((res = Display_Field(field_on_page)) != E_OK)
	      return (res);
	  field_on_page = field_on_page->snext;
	}
      while (field_on_page != last_field);

      if (field)
	res = _nc_Set_Current_Field(form, field);
      else
	 
	res = FN_First_Field(form);
    }
  return (res);
}

 
NCURSES_INLINE static int
Next_Page_Number(const FORM *form)
{
  return (form->curpage + 1) % form->maxpage;
}

 
NCURSES_INLINE static int
Previous_Page_Number(const FORM *form)
{
  return (form->curpage != 0 ? form->curpage - 1 : form->maxpage - 1);
}

 

 
static int
Page_Navigation(int (*const fct) (FORM *), FORM *form)
{
  int res;

  if (!_nc_Internal_Validation(form))
    res = E_INVALID_FIELD;
  else
    {
      Call_Hook(form, fieldterm);
      Call_Hook(form, formterm);
      res = fct(form);
      Call_Hook(form, forminit);
      Call_Hook(form, fieldinit);
    }
  return res;
}

 
static int
PN_Next_Page(FORM *form)
{
  T((T_CALLED("PN_Next_Page(%p)"), (void *)form));
  returnCode(_nc_Set_Form_Page(form, Next_Page_Number(form), (FIELD *)0));
}

 
static int
PN_Previous_Page(FORM *form)
{
  T((T_CALLED("PN_Previous_Page(%p)"), (void *)form));
  returnCode(_nc_Set_Form_Page(form, Previous_Page_Number(form), (FIELD *)0));
}

 
static int
PN_First_Page(FORM *form)
{
  T((T_CALLED("PN_First_Page(%p)"), (void *)form));
  returnCode(_nc_Set_Form_Page(form, 0, (FIELD *)0));
}

 
static int
PN_Last_Page(FORM *form)
{
  T((T_CALLED("PN_Last_Page(%p)"), (void *)form));
  returnCode(_nc_Set_Form_Page(form, form->maxpage - 1, (FIELD *)0));
}

 

 

# if USE_WIDEC_SUPPORT
 
static int
Data_Entry_w(FORM *form, wchar_t c)
{
  FIELD *field = form->current;
  int result = E_REQUEST_DENIED;

  T((T_CALLED("Data_Entry(%p,%s)"), (void *)form, _tracechtype((chtype)c)));
  if ((Field_Has_Option(field, O_EDIT))
#if FIX_FORM_INACTIVE_BUG
      && (Field_Has_Option(field, O_ACTIVE))
#endif
    )
    {
      wchar_t given[2];
      cchar_t temp_ch;

      given[0] = c;
      given[1] = 0;
      setcchar(&temp_ch, given, 0, 0, (void *)0);
      if ((Field_Has_Option(field, O_BLANK)) &&
	  First_Position_In_Current_Field(form) &&
	  !(form->status & _FCHECK_REQUIRED) &&
	  !(form->status & _WINDOW_MODIFIED))
	werase(form->w);

      if (form->status & _OVLMODE)
	{
	  wadd_wch(form->w, &temp_ch);
	}
      else
	 
	{
	  bool There_Is_Room = Is_There_Room_For_A_Char_In_Line(form);

	  if (!(There_Is_Room ||
		((Single_Line_Field(field) && Growable(field)))))
	    RETURN(E_REQUEST_DENIED);

	  if (!There_Is_Room && !Field_Grown(field, 1))
	    RETURN(E_SYSTEM_ERROR);

	  wins_wch(form->w, &temp_ch);
	}

      if ((result = Wrapping_Not_Necessary_Or_Wrapping_Ok(form)) == E_OK)
	{
	  bool End_Of_Field = (((field->drows - 1) == form->currow) &&
			       ((field->dcols - 1) == form->curcol));

	  form->status |= _WINDOW_MODIFIED;
	  if (End_Of_Field && !Growable(field) && (Field_Has_Option(field, O_AUTOSKIP)))
	    result = Inter_Field_Navigation(FN_Next_Field, form);
	  else
	    {
	      if (End_Of_Field && Growable(field) && !Field_Grown(field, 1))
		result = E_SYSTEM_ERROR;
	      else
		{
		   
		  if (WINDOW_EXT(form->w, addch_used) == 0)
		    IFN_Next_Character(form);

		  result = E_OK;
		}
	    }
	}
    }
  RETURN(result);
}
# endif

 
static int
Data_Entry(FORM *form, int c)
{
  FIELD *field = form->current;
  int result = E_REQUEST_DENIED;

  T((T_CALLED("Data_Entry(%p,%s)"), (void *)form, _tracechtype((chtype)c)));
  if ((Field_Has_Option(field, O_EDIT))
#if FIX_FORM_INACTIVE_BUG
      && (Field_Has_Option(field, O_ACTIVE))
#endif
    )
    {
      if ((Field_Has_Option(field, O_BLANK)) &&
	  First_Position_In_Current_Field(form) &&
	  !(form->status & _FCHECK_REQUIRED) &&
	  !(form->status & _WINDOW_MODIFIED))
	werase(form->w);

      if (form->status & _OVLMODE)
	{
	  waddch(form->w, (chtype)c);
	}
      else
	 
	{
	  bool There_Is_Room = Is_There_Room_For_A_Char_In_Line(form);

	  if (!(There_Is_Room ||
		((Single_Line_Field(field) && Growable(field)))))
	    RETURN(E_REQUEST_DENIED);

	  if (!There_Is_Room && !Field_Grown(field, 1))
	    RETURN(E_SYSTEM_ERROR);

	  winsch(form->w, (chtype)c);
	}

      if ((result = Wrapping_Not_Necessary_Or_Wrapping_Ok(form)) == E_OK)
	{
	  bool End_Of_Field = (((field->drows - 1) == form->currow) &&
			       ((field->dcols - 1) == form->curcol));

	  if (Field_Has_Option(field, O_EDGE_INSERT_STAY))
	    move_after_insert = !!(form->curcol
				   - form->begincol
				   - field->cols
				   + 1);

	  SetStatus(form, _WINDOW_MODIFIED);
	  if (End_Of_Field && !Growable(field) && (Field_Has_Option(field, O_AUTOSKIP)))
	    result = Inter_Field_Navigation(FN_Next_Field, form);
	  else
	    {
	      if (End_Of_Field && Growable(field) && !Field_Grown(field, 1))
		result = E_SYSTEM_ERROR;
	      else
		{
#if USE_WIDEC_SUPPORT
		   
		  if (WINDOW_EXT(form->w, addch_used) == 0)
		    IFN_Next_Character(form);
#else
		  IFN_Next_Character(form);
#endif
		  result = E_OK;
		}
	    }
	}
    }
  RETURN(result);
}

 
typedef struct
{
  int keycode;			 
  int (*cmd) (FORM *);		 
}
Binding_Info;

 
#define ID_PN    (0x00000000)	 
#define ID_FN    (0x00010000)	 
#define ID_IFN   (0x00020000)	 
#define ID_VSC   (0x00030000)	 
#define ID_HSC   (0x00040000)	 
#define ID_FE    (0x00050000)	 
#define ID_EM    (0x00060000)	 
#define ID_FV    (0x00070000)	 
#define ID_CH    (0x00080000)	 
#define ID_Mask  (0xffff0000)
#define Key_Mask (0x0000ffff)
#define ID_Shft  (16)

 
 
static const Binding_Info bindings[MAX_FORM_COMMAND - MIN_FORM_COMMAND + 1] =
{
  { REQ_NEXT_PAGE    |ID_PN  ,PN_Next_Page},
  { REQ_PREV_PAGE    |ID_PN  ,PN_Previous_Page},
  { REQ_FIRST_PAGE   |ID_PN  ,PN_First_Page},
  { REQ_LAST_PAGE    |ID_PN  ,PN_Last_Page},

  { REQ_NEXT_FIELD   |ID_FN  ,FN_Next_Field},
  { REQ_PREV_FIELD   |ID_FN  ,FN_Previous_Field},
  { REQ_FIRST_FIELD  |ID_FN  ,FN_First_Field},
  { REQ_LAST_FIELD   |ID_FN  ,FN_Last_Field},
  { REQ_SNEXT_FIELD  |ID_FN  ,FN_Sorted_Next_Field},
  { REQ_SPREV_FIELD  |ID_FN  ,FN_Sorted_Previous_Field},
  { REQ_SFIRST_FIELD |ID_FN  ,FN_Sorted_First_Field},
  { REQ_SLAST_FIELD  |ID_FN  ,FN_Sorted_Last_Field},
  { REQ_LEFT_FIELD   |ID_FN  ,FN_Left_Field},
  { REQ_RIGHT_FIELD  |ID_FN  ,FN_Right_Field},
  { REQ_UP_FIELD     |ID_FN  ,FN_Up_Field},
  { REQ_DOWN_FIELD   |ID_FN  ,FN_Down_Field},

  { REQ_NEXT_CHAR    |ID_IFN ,IFN_Next_Character},
  { REQ_PREV_CHAR    |ID_IFN ,IFN_Previous_Character},
  { REQ_NEXT_LINE    |ID_IFN ,IFN_Next_Line},
  { REQ_PREV_LINE    |ID_IFN ,IFN_Previous_Line},
  { REQ_NEXT_WORD    |ID_IFN ,IFN_Next_Word},
  { REQ_PREV_WORD    |ID_IFN ,IFN_Previous_Word},
  { REQ_BEG_FIELD    |ID_IFN ,IFN_Beginning_Of_Field},
  { REQ_END_FIELD    |ID_IFN ,IFN_End_Of_Field},
  { REQ_BEG_LINE     |ID_IFN ,IFN_Beginning_Of_Line},
  { REQ_END_LINE     |ID_IFN ,IFN_End_Of_Line},
  { REQ_LEFT_CHAR    |ID_IFN ,IFN_Left_Character},
  { REQ_RIGHT_CHAR   |ID_IFN ,IFN_Right_Character},
  { REQ_UP_CHAR      |ID_IFN ,IFN_Up_Character},
  { REQ_DOWN_CHAR    |ID_IFN ,IFN_Down_Character},

  { REQ_NEW_LINE     |ID_FE  ,FE_New_Line},
  { REQ_INS_CHAR     |ID_FE  ,FE_Insert_Character},
  { REQ_INS_LINE     |ID_FE  ,FE_Insert_Line},
  { REQ_DEL_CHAR     |ID_FE  ,FE_Delete_Character},
  { REQ_DEL_PREV     |ID_FE  ,FE_Delete_Previous},
  { REQ_DEL_LINE     |ID_FE  ,FE_Delete_Line},
  { REQ_DEL_WORD     |ID_FE  ,FE_Delete_Word},
  { REQ_CLR_EOL      |ID_FE  ,FE_Clear_To_End_Of_Line},
  { REQ_CLR_EOF      |ID_FE  ,FE_Clear_To_End_Of_Field},
  { REQ_CLR_FIELD    |ID_FE  ,FE_Clear_Field},

  { REQ_OVL_MODE     |ID_EM  ,EM_Overlay_Mode},
  { REQ_INS_MODE     |ID_EM  ,EM_Insert_Mode},

  { REQ_SCR_FLINE    |ID_VSC ,VSC_Scroll_Line_Forward},
  { REQ_SCR_BLINE    |ID_VSC ,VSC_Scroll_Line_Backward},
  { REQ_SCR_FPAGE    |ID_VSC ,VSC_Scroll_Page_Forward},
  { REQ_SCR_BPAGE    |ID_VSC ,VSC_Scroll_Page_Backward},
  { REQ_SCR_FHPAGE   |ID_VSC ,VSC_Scroll_Half_Page_Forward},
  { REQ_SCR_BHPAGE   |ID_VSC ,VSC_Scroll_Half_Page_Backward},

  { REQ_SCR_FCHAR    |ID_HSC ,HSC_Scroll_Char_Forward},
  { REQ_SCR_BCHAR    |ID_HSC ,HSC_Scroll_Char_Backward},
  { REQ_SCR_HFLINE   |ID_HSC ,HSC_Horizontal_Line_Forward},
  { REQ_SCR_HBLINE   |ID_HSC ,HSC_Horizontal_Line_Backward},
  { REQ_SCR_HFHALF   |ID_HSC ,HSC_Horizontal_Half_Line_Forward},
  { REQ_SCR_HBHALF   |ID_HSC ,HSC_Horizontal_Half_Line_Backward},

  { REQ_VALIDATION   |ID_FV  ,FV_Validation},

  { REQ_NEXT_CHOICE  |ID_CH  ,CR_Next_Choice},
  { REQ_PREV_CHOICE  |ID_CH  ,CR_Previous_Choice}
};
 

 
FORM_EXPORT(int)
form_driver(FORM *form, int c)
{
  const Binding_Info *BI = (Binding_Info *)0;
  int res = E_UNKNOWN_COMMAND;

  move_after_insert = TRUE;

  T((T_CALLED("form_driver(%p,%d)"), (void *)form, c));

  if (!form)
    RETURN(E_BAD_ARGUMENT);

  if (!(form->field) || !(form->current))
    RETURN(E_NOT_CONNECTED);

  assert(form->page);

  if (c == FIRST_ACTIVE_MAGIC)
    {
      form->current = _nc_First_Active_Field(form);
      RETURN(E_OK);
    }

  assert(form->current &&
	 form->current->buf &&
	 (form->current->form == form)
    );

  if (form->status & _IN_DRIVER)
    RETURN(E_BAD_STATE);

  if (!(form->status & _POSTED))
    RETURN(E_NOT_POSTED);

  if ((c >= MIN_FORM_COMMAND && c <= MAX_FORM_COMMAND) &&
      ((bindings[c - MIN_FORM_COMMAND].keycode & Key_Mask) == c))
    {
      TR(TRACE_CALLS, ("form_request %s", form_request_name(c)));
      BI = &(bindings[c - MIN_FORM_COMMAND]);
    }

  if (BI)
    {
      typedef int (*Generic_Method) (int (*const) (FORM *), FORM *);
      static const Generic_Method Generic_Methods[] =
      {
	Page_Navigation,	 
	Inter_Field_Navigation,	 
	NULL,			 
	Vertical_Scrolling,	 
	Horizontal_Scrolling,	 
	Field_Editing,		 
	NULL,			 
	NULL,			 
	NULL			 
      };
      size_t nMethods = (sizeof(Generic_Methods) / sizeof(Generic_Methods[0]));
      size_t method = (size_t)((BI->keycode >> ID_Shft) & 0xffff);	 

      if ((method >= nMethods) || !(BI->cmd))
	res = E_SYSTEM_ERROR;
      else
	{
	  Generic_Method fct = Generic_Methods[method];

	  if (fct)
	    {
	      res = fct(BI->cmd, form);
	    }
	  else
	    {
	      res = (BI->cmd) (form);
	    }
	}
    }
#ifdef NCURSES_MOUSE_VERSION
  else if (KEY_MOUSE == c)
    {
      MEVENT event;
      WINDOW *win = form->win ? form->win : StdScreen(Get_Form_Screen(form));
      WINDOW *sub = form->sub ? form->sub : win;

      getmouse(&event);
      if ((event.bstate & (BUTTON1_CLICKED |
			   BUTTON1_DOUBLE_CLICKED |
			   BUTTON1_TRIPLE_CLICKED))
	  && wenclose(win, event.y, event.x))
	{			 
	  int ry = event.y, rx = event.x;	 

	  res = E_REQUEST_DENIED;
	  if (mouse_trafo(&ry, &rx, FALSE))
	    {			 
	      if (ry < sub->_begy)
		{		 
		  if (event.bstate & BUTTON1_CLICKED)
		    res = form_driver(form, REQ_PREV_FIELD);
		  else if (event.bstate & BUTTON1_DOUBLE_CLICKED)
		    res = form_driver(form, REQ_PREV_PAGE);
		  else if (event.bstate & BUTTON1_TRIPLE_CLICKED)
		    res = form_driver(form, REQ_FIRST_FIELD);
		}
	      else if (ry > sub->_begy + sub->_maxy)
		{		 
		  if (event.bstate & BUTTON1_CLICKED)
		    res = form_driver(form, REQ_NEXT_FIELD);
		  else if (event.bstate & BUTTON1_DOUBLE_CLICKED)
		    res = form_driver(form, REQ_NEXT_PAGE);
		  else if (event.bstate & BUTTON1_TRIPLE_CLICKED)
		    res = form_driver(form, REQ_LAST_FIELD);
		}
	      else if (wenclose(sub, event.y, event.x))
		{		 
		  ry = event.y;
		  rx = event.x;
		  if (wmouse_trafo(sub, &ry, &rx, FALSE))
		    {
		      int min_field = form->page[form->curpage].pmin;
		      int max_field = form->page[form->curpage].pmax;
		      int i;

		      for (i = min_field; i <= max_field; ++i)
			{
			  FIELD *field = form->field[i];

			  if (Field_Is_Selectable(field)
			      && Field_encloses(field, ry, rx) == E_OK)
			    {
			      res = _nc_Set_Current_Field(form, field);
			      if (res == E_OK)
				res = _nc_Position_Form_Cursor(form);
			      if (res == E_OK
				  && (event.bstate & BUTTON1_DOUBLE_CLICKED))
				res = E_UNKNOWN_COMMAND;
			      break;
			    }
			}
		    }
		}
	    }
	}
      else
	res = E_REQUEST_DENIED;
    }
#endif  
  else if (!(c & (~(int)MAX_REGULAR_CHARACTER)))
    {
       
#if USE_WIDEC_SUPPORT
      if (!iscntrl(UChar(c)))
#else
      if (isprint(UChar(c)) &&
	  Check_Char(form, form->current, form->current->type, c,
		     (TypeArgument *)(form->current->arg)))
#endif
	res = Data_Entry(form, c);
    }
  _nc_Refresh_Current_Field(form);
  RETURN(res);
}

# if USE_WIDEC_SUPPORT
 
FORM_EXPORT(int)
form_driver_w(FORM *form, int type, wchar_t c)
{
  const Binding_Info *BI = (Binding_Info *)0;
  int res = E_UNKNOWN_COMMAND;

  T((T_CALLED("form_driver(%p,%d)"), (void *)form, (int)c));

  if (!form)
    RETURN(E_BAD_ARGUMENT);

  if (!(form->field))
    RETURN(E_NOT_CONNECTED);

  assert(form->page);

  if (c == (wchar_t)FIRST_ACTIVE_MAGIC)
    {
      form->current = _nc_First_Active_Field(form);
      RETURN(E_OK);
    }

  assert(form->current &&
	 form->current->buf &&
	 (form->current->form == form)
    );

  if (form->status & _IN_DRIVER)
    RETURN(E_BAD_STATE);

  if (!(form->status & _POSTED))
    RETURN(E_NOT_POSTED);

   
  if (type == KEY_CODE_YES)
    {
      if ((c >= MIN_FORM_COMMAND && c <= MAX_FORM_COMMAND) &&
	  ((bindings[c - MIN_FORM_COMMAND].keycode & Key_Mask) == c))
	BI = &(bindings[c - MIN_FORM_COMMAND]);
    }

  if (BI)
    {
      typedef int (*Generic_Method) (int (*const) (FORM *), FORM *);
      static const Generic_Method Generic_Methods[] =
      {
	Page_Navigation,	 
	Inter_Field_Navigation,	 
	NULL,			 
	Vertical_Scrolling,	 
	Horizontal_Scrolling,	 
	Field_Editing,		 
	NULL,			 
	NULL,			 
	NULL			 
      };
      size_t nMethods = (sizeof(Generic_Methods) / sizeof(Generic_Methods[0]));
      size_t method = (size_t)(BI->keycode >> ID_Shft) & 0xffff;	 

      if ((method >= nMethods) || !(BI->cmd))
	res = E_SYSTEM_ERROR;
      else
	{
	  Generic_Method fct = Generic_Methods[method];

	  if (fct)
	    res = fct(BI->cmd, form);
	  else
	    res = (BI->cmd) (form);
	}
    }
#ifdef NCURSES_MOUSE_VERSION
  else if (KEY_MOUSE == c)
    {
      MEVENT event;
      WINDOW *win = form->win ? form->win : StdScreen(Get_Form_Screen(form));
      WINDOW *sub = form->sub ? form->sub : win;

      getmouse(&event);
      if ((event.bstate & (BUTTON1_CLICKED |
			   BUTTON1_DOUBLE_CLICKED |
			   BUTTON1_TRIPLE_CLICKED))
	  && wenclose(win, event.y, event.x))
	{			 
	  int ry = event.y, rx = event.x;	 

	  res = E_REQUEST_DENIED;
	  if (mouse_trafo(&ry, &rx, FALSE))
	    {			 
	      if (ry < sub->_begy)
		{		 
		  if (event.bstate & BUTTON1_CLICKED)
		    res = form_driver(form, REQ_PREV_FIELD);
		  else if (event.bstate & BUTTON1_DOUBLE_CLICKED)
		    res = form_driver(form, REQ_PREV_PAGE);
		  else if (event.bstate & BUTTON1_TRIPLE_CLICKED)
		    res = form_driver(form, REQ_FIRST_FIELD);
		}
	      else if (ry > sub->_begy + sub->_maxy)
		{		 
		  if (event.bstate & BUTTON1_CLICKED)
		    res = form_driver(form, REQ_NEXT_FIELD);
		  else if (event.bstate & BUTTON1_DOUBLE_CLICKED)
		    res = form_driver(form, REQ_NEXT_PAGE);
		  else if (event.bstate & BUTTON1_TRIPLE_CLICKED)
		    res = form_driver(form, REQ_LAST_FIELD);
		}
	      else if (wenclose(sub, event.y, event.x))
		{		 
		  ry = event.y;
		  rx = event.x;
		  if (wmouse_trafo(sub, &ry, &rx, FALSE))
		    {
		      int min_field = form->page[form->curpage].pmin;
		      int max_field = form->page[form->curpage].pmax;
		      int i;

		      for (i = min_field; i <= max_field; ++i)
			{
			  FIELD *field = form->field[i];

			  if (Field_Is_Selectable(field)
			      && Field_encloses(field, ry, rx) == E_OK)
			    {
			      res = _nc_Set_Current_Field(form, field);
			      if (res == E_OK)
				res = _nc_Position_Form_Cursor(form);
			      if (res == E_OK
				  && (event.bstate & BUTTON1_DOUBLE_CLICKED))
				res = E_UNKNOWN_COMMAND;
			      break;
			    }
			}
		    }
		}
	    }
	}
      else
	res = E_REQUEST_DENIED;
    }
#endif  
  else if (type == OK)
    {
      res = Data_Entry_w(form, c);
    }

  _nc_Refresh_Current_Field(form);
  RETURN(res);
}
# endif	 

 

 
FORM_EXPORT(int)
set_field_buffer(FIELD *field, int buffer, const char *value)
{
  FIELD_CELL *p;
  int res = E_OK;
  int i;
  int len;

#if USE_WIDEC_SUPPORT
  FIELD_CELL *widevalue = 0;
#endif

  T((T_CALLED("set_field_buffer(%p,%d,%s)"), (void *)field, buffer, _nc_visbuf(value)));

  if (!field || !value || ((buffer < 0) || (buffer > field->nbuf)))
    RETURN(E_BAD_ARGUMENT);

  len = Buffer_Length(field);

  if (Growable(field))
    {
       
      int vlen = (int)strlen(value);

      if (vlen > len)
	{
	  if (!Field_Grown(field,
			   (int)(1 + (vlen - len) / ((field->rows + field->nrow)
						     * field->cols))))
	    RETURN(E_SYSTEM_ERROR);

#if !USE_WIDEC_SUPPORT
	  len = vlen;
#endif
	}
    }

  p = Address_Of_Nth_Buffer(field, buffer);

#if USE_WIDEC_SUPPORT
   
#if NCURSES_EXT_FUNCS
  if (wresize(field->working, 1, Buffer_Length(field) + 1) == ERR)
#endif
    {
      delwin(field->working);
      field->working = newpad(1, Buffer_Length(field) + 1);
    }
  len = Buffer_Length(field);
  wclear(field->working);
  (void)mvwaddstr(field->working, 0, 0, value);

  if ((widevalue = typeCalloc(FIELD_CELL, len + 1)) == 0)
    {
      RETURN(E_SYSTEM_ERROR);
    }
  else
    {
      for (i = 0; i < field->drows; ++i)
	{
	  (void)mvwin_wchnstr(field->working, 0, (int)i * field->dcols,
			      widevalue + ((int)i * field->dcols),
			      field->dcols);
	}
      for (i = 0; i < len; ++i)
	{
	  if (CharEq(myZEROS, widevalue[i]))
	    {
	      while (i < len)
		p[i++] = myBLANK;
	      break;
	    }
	  p[i] = widevalue[i];
	}
      free(widevalue);
    }
#else
  for (i = 0; i < len; ++i)
    {
      if (value[i] == '\0')
	{
	  while (i < len)
	    p[i++] = myBLANK;
	  break;
	}
      p[i] = value[i];
    }
#endif

  if (buffer == 0)
    {
      int syncres;

      if (((syncres = Synchronize_Field(field)) != E_OK) &&
	  (res == E_OK))
	res = syncres;
      if (((syncres = Synchronize_Linked_Fields(field)) != E_OK) &&
	  (res == E_OK))
	res = syncres;
    }
  RETURN(res);
}

 
FORM_EXPORT(char *)
field_buffer(const FIELD *field, int buffer)
{
  char *result = 0;

  T((T_CALLED("field_buffer(%p,%d)"), (const void *)field, buffer));

  if (field && (buffer >= 0) && (buffer <= field->nbuf))
    {
#if USE_WIDEC_SUPPORT
      FIELD_CELL *data = Address_Of_Nth_Buffer(field, buffer);
      size_t need = 0;
      int size = Buffer_Length(field);
      int n;

       
      for (n = 0; n < size; ++n)
	{
	  if (!isWidecExt(data[n]) && data[n].chars[0] != L'\0')
	    {
	      mbstate_t state;
	      size_t next;

	      init_mb(state);
	      next = _nc_wcrtomb(0, data[n].chars[0], &state);
	      if (next > 0)
		need += next;
	    }
	}

       
      if (field->expanded[buffer] != 0)
	free(field->expanded[buffer]);
      field->expanded[buffer] = typeMalloc(char, need + 1);

       
      if ((result = field->expanded[buffer]) != 0)
	{
	  wclear(field->working);
	  wmove(field->working, 0, 0);
	  for (n = 0; n < size; ++n)
	    {
	      if (!isWidecExt(data[n]) && data[n].chars[0] != L'\0')
		wadd_wch(field->working, &data[n]);
	    }
	  wmove(field->working, 0, 0);
	  winnstr(field->working, result, (int)need);
	}
#else
      result = Address_Of_Nth_Buffer(field, buffer);
#endif
    }
  returnPtr(result);
}

#if USE_WIDEC_SUPPORT

 
FORM_EXPORT(wchar_t *)
_nc_Widen_String(char *source, int *lengthp)
{
  wchar_t *result = 0;
  wchar_t wch;
  size_t given = strlen(source);
  size_t tries;
  int pass;
  int status;

#ifndef state_unused
  mbstate_t state;
#endif

  for (pass = 0; pass < 2; ++pass)
    {
      unsigned need = 0;
      size_t passed = 0;

      while (passed < given)
	{
	  bool found = FALSE;

	  for (tries = 1, status = 0; tries <= (given - passed); ++tries)
	    {
	      int save = source[passed + tries];

	      source[passed + tries] = 0;
	      reset_mbytes(state);
	      status = check_mbytes(wch, source + passed, tries, state);
	      source[passed + tries] = (char)save;

	      if (status > 0)
		{
		  found = TRUE;
		  break;
		}
	    }
	  if (found)
	    {
	      if (pass)
		{
		  result[need] = wch;
		}
	      passed += (size_t)status;
	      ++need;
	    }
	  else
	    {
	      if (pass)
		{
		  result[need] = (wchar_t)source[passed];
		}
	      ++need;
	      ++passed;
	    }
	}

      if (!pass)
	{
	  if (!need)
	    break;
	  result = typeCalloc(wchar_t, need);

	  *lengthp = (int)need;
	  if (result == 0)
	    break;
	}
    }

  return result;
}
#endif

 
