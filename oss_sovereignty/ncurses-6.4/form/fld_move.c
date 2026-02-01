 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_move.c,v 1.16 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
move_field(FIELD *field, int frow, int fcol)
{
  T((T_CALLED("move_field(%p,%d,%d)"), (void *)field, frow, fcol));

  if (!field || (frow < 0) || (fcol < 0))
    RETURN(E_BAD_ARGUMENT);

  if (field->form)
    RETURN(E_CONNECTED);

  field->frow = (short)frow;
  field->fcol = (short)fcol;
  RETURN(E_OK);
}

 
