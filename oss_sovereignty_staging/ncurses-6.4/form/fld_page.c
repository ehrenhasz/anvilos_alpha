 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_page.c,v 1.16 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
set_new_page(FIELD *field, bool new_page_flag)
{
  T((T_CALLED("set_new_page(%p,%d)"), (void *)field, new_page_flag));

  Normalize_Field(field);
  if (field->form)
    RETURN(E_CONNECTED);

  if (new_page_flag)
    SetStatus(field, _NEWPAGE);
  else
    ClrStatus(field, _NEWPAGE);

  RETURN(E_OK);
}

 
FORM_EXPORT(bool)
new_page(const FIELD *field)
{
  T((T_CALLED("new_page(%p)"), (const void *)field));

  returnBool((Normalize_Field(field)->status & _NEWPAGE) ? TRUE : FALSE);
}

 
