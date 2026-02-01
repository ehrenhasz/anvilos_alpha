 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_current.c,v 1.16 2020/05/24 01:40:20 anonymous.maarten Exp $")

 
FORM_EXPORT(int)
set_current_field(FORM *form, FIELD *field)
{
  int err = E_OK;

  T((T_CALLED("set_current_field(%p,%p)"), (void *)form, (void *)field));
  if (form == 0 || field == 0)
    {
      RETURN(E_BAD_ARGUMENT);
    }
  else if ((form != field->form) || Field_Is_Not_Selectable(field))
    {
      RETURN(E_REQUEST_DENIED);
    }
  else if ((form->status & _POSTED) == 0)
    {
      form->current = field;
      form->curpage = field->page;
    }
  else
    {
      if ((form->status & _IN_DRIVER) != 0)
	{
	  err = E_BAD_STATE;
	}
      else
	{
	  if (form->current != field)
	    {
	      if (form->current && !_nc_Internal_Validation(form))
		{
		  err = E_INVALID_FIELD;
		}
	      else
		{
		  Call_Hook(form, fieldterm);
		  if (field->page != form->curpage)
		    {
		      Call_Hook(form, formterm);
		      err = _nc_Set_Form_Page(form, (int)field->page, field);
		      Call_Hook(form, forminit);
		    }
		  else
		    {
		      err = _nc_Set_Current_Field(form, field);
		    }
		  Call_Hook(form, fieldinit);
		  (void)_nc_Refresh_Current_Field(form);
		}
	    }
	}
    }
  RETURN(err);
}

 
FORM_EXPORT(int)
unfocus_current_field(FORM *const form)
{
  T((T_CALLED("unfocus_current_field(%p)"), (const void *)form));
  if (form == 0)
    {
      RETURN(E_BAD_ARGUMENT);
    }
  else if (form->current == 0)
    {
      RETURN(E_REQUEST_DENIED);
    }
  _nc_Unset_Current_Field(form);
  RETURN(E_OK);
}

 
FORM_EXPORT(FIELD *)
current_field(const FORM *form)
{
  T((T_CALLED("current_field(%p)"), (const void *)form));
  returnField(Normalize_Form(form)->current);
}

 
FORM_EXPORT(int)
field_index(const FIELD *field)
{
  T((T_CALLED("field_index(%p)"), (const void *)field));
  returnCode((field != 0 && field->form != 0) ? (int)field->index : -1);
}

 
