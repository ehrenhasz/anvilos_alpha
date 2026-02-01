 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_post.c,v 1.14 2020/05/24 01:40:20 anonymous.maarten Exp $")

 
FORM_EXPORT(int)
post_form(FORM *form)
{
  WINDOW *formwin;
  int err;
  int page;

  T((T_CALLED("post_form(%p)"), (void *)form));

  if (!form)
    RETURN(E_BAD_ARGUMENT);

  if (form->status & _POSTED)
    RETURN(E_POSTED);

  if (!(form->field))
    RETURN(E_NOT_CONNECTED);

  formwin = Get_Form_Window(form);
  if ((form->cols > getmaxx(formwin)) || (form->rows > getmaxy(formwin)))
    RETURN(E_NO_ROOM);

   
  page = form->curpage;
  form->curpage = -1;
  if ((err = _nc_Set_Form_Page(form, page, form->current)) != E_OK)
    RETURN(err);

  SetStatus(form, _POSTED);

  Call_Hook(form, forminit);
  Call_Hook(form, fieldinit);

  _nc_Refresh_Current_Field(form);
  RETURN(E_OK);
}

 
FORM_EXPORT(int)
unpost_form(FORM *form)
{
  T((T_CALLED("unpost_form(%p)"), (void *)form));

  if (!form)
    RETURN(E_BAD_ARGUMENT);

  if (!(form->status & _POSTED))
    RETURN(E_NOT_POSTED);

  if (form->status & _IN_DRIVER)
    RETURN(E_BAD_STATE);

  Call_Hook(form, fieldterm);
  Call_Hook(form, formterm);

  werase(Get_Form_Window(form));
  delwin(form->w);
  form->w = (WINDOW *)0;
  ClrStatus(form, _POSTED);
  RETURN(E_OK);
}

 
