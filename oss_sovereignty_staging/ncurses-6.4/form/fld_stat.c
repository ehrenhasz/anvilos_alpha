 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_stat.c,v 1.18 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
set_field_status(FIELD *field, bool status)
{
  T((T_CALLED("set_field_status(%p,%d)"), (void *)field, status));

  Normalize_Field(field);

  if (status)
    SetStatus(field, _CHANGED);
  else
    ClrStatus(field, _CHANGED);

  RETURN(E_OK);
}

 
FORM_EXPORT(bool)
field_status(const FIELD *field)
{
  T((T_CALLED("field_status(%p)"), (const void *)field));

  returnBool((Normalize_Field(field)->status & _CHANGED) ? TRUE : FALSE);
}

 
