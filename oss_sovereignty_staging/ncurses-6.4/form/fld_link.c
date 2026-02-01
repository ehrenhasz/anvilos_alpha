 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_link.c,v 1.17 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(FIELD *)
link_field(FIELD *field, int frow, int fcol)
{
  FIELD *New_Field = (FIELD *)0;
  int err = E_BAD_ARGUMENT;

  T((T_CALLED("link_field(%p,%d,%d)"), (void *)field, frow, fcol));
  if (field && (frow >= 0) && (fcol >= 0) &&
      ((err = E_SYSTEM_ERROR) != 0) &&	 
      (New_Field = typeMalloc(FIELD, 1)))
    {
      T((T_CREATE("field %p"), (void *)New_Field));
      *New_Field = *_nc_Default_Field;
      New_Field->frow = (short)frow;
      New_Field->fcol = (short)fcol;

      New_Field->link = field->link;
      field->link = New_Field;

      New_Field->buf = field->buf;
      New_Field->rows = field->rows;
      New_Field->cols = field->cols;
      New_Field->nrow = field->nrow;
      New_Field->nbuf = field->nbuf;
      New_Field->drows = field->drows;
      New_Field->dcols = field->dcols;
      New_Field->maxgrow = field->maxgrow;
      New_Field->just = field->just;
      New_Field->fore = field->fore;
      New_Field->back = field->back;
      New_Field->pad = field->pad;
      New_Field->opts = field->opts;
      New_Field->usrptr = field->usrptr;

      if (_nc_Copy_Type(New_Field, field))
	returnField(New_Field);
    }

  if (New_Field)
    free_field(New_Field);

  SET_ERROR(err);
  returnField((FIELD *)0);
}

 
