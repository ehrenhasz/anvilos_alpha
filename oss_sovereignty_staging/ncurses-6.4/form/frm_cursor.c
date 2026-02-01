 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_cursor.c,v 1.13 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
pos_form_cursor(FORM *form)
{
  int res;

  T((T_CALLED("pos_form_cursor(%p)"), (void *)form));

  if (!form)
    res = E_BAD_ARGUMENT;
  else
    {
      if (!(form->status & _POSTED))
	res = E_NOT_POSTED;
      else
	res = _nc_Position_Form_Cursor(form);
    }
  RETURN(res);
}

 
