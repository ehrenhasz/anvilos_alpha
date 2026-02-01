 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_pad.c,v 1.14 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
set_field_pad(FIELD *field, int ch)
{
  int res = E_BAD_ARGUMENT;

  T((T_CALLED("set_field_pad(%p,%d)"), (void *)field, ch));

  Normalize_Field(field);
  if (isprint(UChar(ch)))
    {
      if (field->pad != ch)
	{
	  field->pad = ch;
	  res = _nc_Synchronize_Attributes(field);
	}
      else
	res = E_OK;
    }
  RETURN(res);
}

 
FORM_EXPORT(int)
field_pad(const FIELD *field)
{
  T((T_CALLED("field_pad(%p)"), (const void *)field));

  returnCode(Normalize_Field(field)->pad);
}

 
