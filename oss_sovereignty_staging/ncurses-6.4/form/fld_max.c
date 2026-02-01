 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_max.c,v 1.18 2021/06/17 21:26:02 tom Exp $")

 
FORM_EXPORT(int)
set_max_field(FIELD *field, int maxgrow)
{
  T((T_CALLED("set_max_field(%p,%d)"), (void *)field, maxgrow));

  if (!field || (maxgrow < 0))
    RETURN(E_BAD_ARGUMENT);
  else
    {
      bool single_line_field = Single_Line_Field(field);

      if (maxgrow > 0)
	{
	  if (((single_line_field && (maxgrow < field->dcols)) ||
	       (!single_line_field && (maxgrow < field->drows))) &&
	      !Field_Has_Option(field, O_INPUT_LIMIT))
	    RETURN(E_BAD_ARGUMENT);
	}
      field->maxgrow = maxgrow;
       
      if (maxgrow > 0 && Field_Has_Option(field, O_INPUT_LIMIT) &&
	  field->dcols > maxgrow)
	field->dcols = maxgrow;
      ClrStatus(field, _MAY_GROW);
      if (!((unsigned)field->opts & O_STATIC))
	{
	  if ((maxgrow == 0) ||
	      (single_line_field && (field->dcols < maxgrow)) ||
	      (!single_line_field && (field->drows < maxgrow)))
	    SetStatus(field, _MAY_GROW);
	}
    }
  RETURN(E_OK);
}

 
