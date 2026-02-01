 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_type.c,v 1.20 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
set_field_type(FIELD *field, FIELDTYPE *type, ...)
{
  va_list ap;
  int res = E_SYSTEM_ERROR;
  int err = 0;

  T((T_CALLED("set_field_type(%p,%p)"), (void *)field, (void *)type));

  va_start(ap, type);

  Normalize_Field(field);
  _nc_Free_Type(field);

  field->type = type;
  field->arg = (void *)_nc_Make_Argument(field->type, &ap, &err);

  if (err)
    {
      _nc_Free_Argument(field->type, (TypeArgument *)(field->arg));
      field->type = (FIELDTYPE *)0;
      field->arg = (void *)0;
    }
  else
    {
      res = E_OK;
      if (field->type)
	field->type->ref++;
    }

  va_end(ap);
  RETURN(res);
}

 
FORM_EXPORT(FIELDTYPE *)
field_type(const FIELD *field)
{
  T((T_CALLED("field_type(%p)"), (const void *)field));
  returnFieldType(Normalize_Field(field)->type);
}

 
