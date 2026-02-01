 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_just.c,v 1.17 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
set_field_just(FIELD *field, int just)
{
  int res = E_BAD_ARGUMENT;

  T((T_CALLED("set_field_just(%p,%d)"), (void *)field, just));

  if ((just == NO_JUSTIFICATION) ||
      (just == JUSTIFY_LEFT) ||
      (just == JUSTIFY_CENTER) ||
      (just == JUSTIFY_RIGHT))
    {
      Normalize_Field(field);
      if (field->just != just)
	{
	  field->just = (short)just;
	  res = _nc_Synchronize_Attributes(field);
	}
      else
	res = E_OK;
    }
  RETURN(res);
}

 
FORM_EXPORT(int)
field_just(const FIELD *field)
{
  T((T_CALLED("field_just(%p)"), (const void *)field));
  returnCode(Normalize_Field(field)->just);
}

 
