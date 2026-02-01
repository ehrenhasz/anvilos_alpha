 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_page.c,v 1.15 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
set_form_page(FORM *form, int page)
{
  int err = E_OK;

  T((T_CALLED("set_form_page(%p,%d)"), (void *)form, page));

  if (!form || (page < 0) || (page >= form->maxpage))
    RETURN(E_BAD_ARGUMENT);

  if (!(form->status & _POSTED))
    {
      form->curpage = (short)page;
      form->current = _nc_First_Active_Field(form);
    }
  else
    {
      if (form->status & _IN_DRIVER)
	err = E_BAD_STATE;
      else
	{
	  if (form->curpage != page)
	    {
	      if (!_nc_Internal_Validation(form))
		err = E_INVALID_FIELD;
	      else
		{
		  Call_Hook(form, fieldterm);
		  Call_Hook(form, formterm);
		  err = _nc_Set_Form_Page(form, page, (FIELD *)0);
		  Call_Hook(form, forminit);
		  Call_Hook(form, fieldinit);
		  _nc_Refresh_Current_Field(form);
		}
	    }
	}
    }
  RETURN(err);
}

 
FORM_EXPORT(int)
form_page(const FORM *form)
{
  T((T_CALLED("form_page(%p)"), (const void *)form));

  returnCode(Normalize_Form(form)->curpage);
}

 
