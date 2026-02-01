 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_sub.c,v 1.15 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
set_form_sub(FORM *form, WINDOW *win)
{
  T((T_CALLED("set_form_sub(%p,%p)"), (void *)form, (void *)win));

  if (form && (form->status & _POSTED))
    RETURN(E_POSTED);
  else
    {
#if NCURSES_SP_FUNCS
      FORM *f = Normalize_Form(form);

      f->sub = win ? win : StdScreen(Get_Form_Screen(f));
      RETURN(E_OK);
#else
      Normalize_Form(form)->sub = win;
      RETURN(E_OK);
#endif
    }
}

 
FORM_EXPORT(WINDOW *)
form_sub(const FORM *form)
{
  const FORM *f;

  T((T_CALLED("form_sub(%p)"), (const void *)form));

  f = Normalize_Form(form);
  returnWin(Get_Form_Window(f));
}

 
