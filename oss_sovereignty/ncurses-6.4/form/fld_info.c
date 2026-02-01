 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_info.c,v 1.16 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
field_info(const FIELD *field,
	   int *rows, int *cols,
	   int *frow, int *fcol,
	   int *nrow, int *nbuf)
{
  T((T_CALLED("field_info(%p,%p,%p,%p,%p,%p,%p)"),
     (const void *)field,
     (void *)rows, (void *)cols,
     (void *)frow, (void *)fcol,
     (void *)nrow, (void *)nbuf));

  if (!field)
    RETURN(E_BAD_ARGUMENT);

  if (rows)
    *rows = field->rows;
  if (cols)
    *cols = field->cols;
  if (frow)
    *frow = field->frow;
  if (fcol)
    *fcol = field->fcol;
  if (nrow)
    *nrow = field->nrow;
  if (nbuf)
    *nbuf = field->nbuf;
  RETURN(E_OK);
}

 
FORM_EXPORT(int)
dynamic_field_info(const FIELD *field, int *drows, int *dcols, int *maxgrow)
{
  T((T_CALLED("dynamic_field_info(%p,%p,%p,%p)"),
     (const void *)field,
     (void *)drows,
     (void *)dcols,
     (void *)maxgrow));

  if (!field)
    RETURN(E_BAD_ARGUMENT);

  if (drows)
    *drows = field->drows;
  if (dcols)
    *dcols = field->dcols;
  if (maxgrow)
    *maxgrow = field->maxgrow;

  RETURN(E_OK);
}

 
