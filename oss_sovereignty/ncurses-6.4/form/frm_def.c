 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_def.c,v 1.30 2021/03/27 23:49:58 tom Exp $")

 
static FORM default_form =
{
  0,				 
  0,				 
  0,				 
  0,				 
  0,				 
  0,				 
  0,				 
  -1,				 
  -1,				 
  -1,				 
  ALL_FORM_OPTS,		 
  (WINDOW *)0,			 
  (WINDOW *)0,			 
  (WINDOW *)0,			 
  (FIELD **)0,			 
  (FIELD *)0,			 
  (_PAGE *) 0,			 
  (char *)0,			 
  NULL,				 
  NULL,				 
  NULL,				 
  NULL				 
};

FORM_EXPORT_VAR(FORM *) _nc_Default_Form = &default_form;

 
static FIELD *
Insert_Field_By_Position(FIELD *newfield, FIELD *head)
{
  FIELD *current, *newhead;

  assert(newfield);

  if (!head)
    {				 
      newhead = newfield->snext = newfield->sprev = newfield;
    }
  else
    {
      newhead = current = head;
      while ((current->frow < newfield->frow) ||
	     ((current->frow == newfield->frow) &&
	      (current->fcol < newfield->fcol)))
	{
	  current = current->snext;
	  if (current == head)
	    {			 
	      head = (FIELD *)0;
	      break;
	    }
	}
       
      newfield->snext = current;
      newfield->sprev = current->sprev;
      newfield->snext->sprev = newfield;
      newfield->sprev->snext = newfield;
      if (current == head)
	newhead = newfield;
    }
  return (newhead);
}

 
static void
Disconnect_Fields(FORM *form)
{
  if (form->field)
    {
      FIELD **fields;

      for (fields = form->field; *fields; fields++)
	{
	  if (form == (*fields)->form)
	    (*fields)->form = (FORM *)0;
	}

      form->rows = form->cols = 0;
      form->maxfield = form->maxpage = -1;
      form->field = (FIELD **)0;
      if (form->page)
	free(form->page);
      form->page = (_PAGE *) 0;
    }
}

 
static int
Connect_Fields(FORM *form, FIELD **fields)
{
  int field_cnt, j;
  int page_nr;
  _PAGE *pg;

  T((T_CALLED("Connect_Fields(%p,%p)"), (void *)form, (void *)fields));

  assert(form);

  form->field = fields;
  form->maxfield = 0;
  form->maxpage = 0;

  if (!fields)
    RETURN(E_OK);

  page_nr = 0;
   
  for (field_cnt = 0; fields[field_cnt]; field_cnt++)
    {
      if (fields[field_cnt]->form)
	RETURN(E_CONNECTED);
      if (field_cnt == 0 ||
	  (fields[field_cnt]->status & _NEWPAGE))
	page_nr++;
      fields[field_cnt]->form = form;
    }
  if (field_cnt == 0 || (short)field_cnt < 0)
    RETURN(E_BAD_ARGUMENT);

   
  if ((pg = typeMalloc(_PAGE, page_nr)) != (_PAGE *) 0)
    {
      T((T_CREATE("_PAGE %p"), (void *)pg));
      form->page = pg;
    }
  else
    RETURN(E_SYSTEM_ERROR);

   
  for (j = 0; j < field_cnt; j++)
    {
      int maximum_row_in_field;
      int maximum_col_in_field;

      if (j == 0)
	pg->pmin = (short)j;
      else
	{
	  if (fields[j]->status & _NEWPAGE)
	    {
	      pg->pmax = (short)(j - 1);
	      pg++;
	      pg->pmin = (short)j;
	    }
	}

      maximum_row_in_field = fields[j]->frow + fields[j]->rows;
      maximum_col_in_field = fields[j]->fcol + fields[j]->cols;

      if (form->rows < maximum_row_in_field)
	form->rows = (short)maximum_row_in_field;
      if (form->cols < maximum_col_in_field)
	form->cols = (short)maximum_col_in_field;
    }

  pg->pmax = (short)(field_cnt - 1);
  form->maxfield = (short)field_cnt;
  form->maxpage = (short)page_nr;

   
  for (page_nr = 0; page_nr < form->maxpage; page_nr++)
    {
      FIELD *fld = (FIELD *)0;

      for (j = form->page[page_nr].pmin; j <= form->page[page_nr].pmax; j++)
	{
	  fields[j]->index = (short)j;
	  fields[j]->page = (short)page_nr;
	  fld = Insert_Field_By_Position(fields[j], fld);
	}
      if (fld)
	{
	  form->page[page_nr].smin = fld->index;
	  form->page[page_nr].smax = fld->sprev->index;
	}
      else
	{
	  form->page[page_nr].smin = 0;
	  form->page[page_nr].smax = 0;
	}
    }
  RETURN(E_OK);
}

 
NCURSES_INLINE static int
Associate_Fields(FORM *form, FIELD **fields)
{
  int res = Connect_Fields(form, fields);

  if (res == E_OK)
    {
      if (form->maxpage > 0)
	{
	  form->curpage = 0;
	  form_driver(form, FIRST_ACTIVE_MAGIC);
	}
      else
	{
	  form->curpage = -1;
	  form->current = (FIELD *)0;
	}
    }
  return (res);
}

 
FORM_EXPORT(FORM *)
NCURSES_SP_NAME(new_form) (NCURSES_SP_DCLx FIELD **fields)
{
  int err = E_SYSTEM_ERROR;
  FORM *form = (FORM *)0;

  T((T_CALLED("new_form(%p,%p)"), (void *)SP_PARM, (void *)fields));

  if (IsValidScreen(SP_PARM))
    {
      form = typeMalloc(FORM, 1);

      if (form)
	{
	  T((T_CREATE("form %p"), (void *)form));
	  *form = *_nc_Default_Form;
	   
	  form->win = StdScreen(SP_PARM);
	  form->sub = StdScreen(SP_PARM);
	  if ((err = Associate_Fields(form, fields)) != E_OK)
	    {
	      free_form(form);
	      form = (FORM *)0;
	    }
	}
    }

  if (!form)
    SET_ERROR(err);

  returnForm(form);
}

 
#if NCURSES_SP_FUNCS
FORM_EXPORT(FORM *)
new_form(FIELD **fields)
{
  return NCURSES_SP_NAME(new_form) (CURRENT_SCREEN, fields);
}
#endif

 
FORM_EXPORT(int)
free_form(FORM *form)
{
  T((T_CALLED("free_form(%p)"), (void *)form));

  if (!form)
    RETURN(E_BAD_ARGUMENT);

  if (form->status & _POSTED)
    RETURN(E_POSTED);

  Disconnect_Fields(form);
  if (form->page)
    free(form->page);
  free(form);

  RETURN(E_OK);
}

 
FORM_EXPORT(int)
set_form_fields(FORM *form, FIELD **fields)
{
  FIELD **old;
  int res;

  T((T_CALLED("set_form_fields(%p,%p)"), (void *)form, (void *)fields));

  if (!form)
    RETURN(E_BAD_ARGUMENT);

  if (form->status & _POSTED)
    RETURN(E_POSTED);

  old = form->field;
  Disconnect_Fields(form);

  if ((res = Associate_Fields(form, fields)) != E_OK)
    Connect_Fields(form, old);

  RETURN(res);
}

 
FORM_EXPORT(FIELD **)
form_fields(const FORM *form)
{
  T((T_CALLED("form_field(%p)"), (const void *)form));
  returnFieldPtr(Normalize_Form(form)->field);
}

 
FORM_EXPORT(int)
field_count(const FORM *form)
{
  T((T_CALLED("field_count(%p)"), (const void *)form));

  returnCode(Normalize_Form(form)->maxfield);
}

 
