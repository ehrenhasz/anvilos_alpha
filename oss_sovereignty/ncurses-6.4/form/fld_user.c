 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_user.c,v 1.20 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
set_field_userptr(FIELD *field, void *usrptr)
{
  T((T_CALLED("set_field_userptr(%p,%p)"), (void *)field, (void *)usrptr));

  Normalize_Field(field)->usrptr = usrptr;
  RETURN(E_OK);
}

 
FORM_EXPORT(void *)
field_userptr(const FIELD *field)
{
  T((T_CALLED("field_userptr(%p)"), (const void *)field));
  returnVoidPtr(Normalize_Field(field)->usrptr);
}

 
