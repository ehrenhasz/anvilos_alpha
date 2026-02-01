 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_user.c,v 1.19 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
set_form_userptr(FORM *form, void *usrptr)
{
  T((T_CALLED("set_form_userptr(%p,%p)"), (void *)form, (void *)usrptr));

  Normalize_Form(form)->usrptr = usrptr;
  RETURN(E_OK);
}

 
FORM_EXPORT(void *)
form_userptr(const FORM *form)
{
  T((T_CALLED("form_userptr(%p)"), (const void *)form));
  returnVoidPtr(Normalize_Form(form)->usrptr);
}

 
