 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_scale.c,v 1.13 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
scale_form(const FORM *form, int *rows, int *cols)
{
  T((T_CALLED("scale_form(%p,%p,%p)"),
     (const void *)form,
     (void *)rows,
     (void *)cols));

  if (!form)
    RETURN(E_BAD_ARGUMENT);

  if (!(form->field))
    RETURN(E_NOT_CONNECTED);

  if (rows)
    *rows = form->rows;
  if (cols)
    *cols = form->cols;

  RETURN(E_OK);
}

 
