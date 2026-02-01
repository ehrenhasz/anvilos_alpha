 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_win.c,v 1.19 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
set_form_win(FORM *form, WINDOW *win)
{
  T((T_CALLED("set_form_win(%p,%p)"), (void *)form, (void *)win));

  if (form && (form->status & _POSTED))
    RETURN(E_POSTED);
  else
    {
#if NCURSES_SP_FUNCS
      FORM *f = Normalize_Form(form);

      f->win = win ? win : StdScreen(Get_Form_Screen(f));
      RETURN(E_OK);
#else
      Normalize_Form(form)->win = win;
      RETURN(E_OK);
#endif
    }
}

 
FORM_EXPORT(WINDOW *)
form_win(const FORM *form)
{
  WINDOW *result;
  const FORM *f;

  T((T_CALLED("form_win(%p)"), (const void *)form));

  f = Normalize_Form(form);
#if NCURSES_SP_FUNCS
  result = (f->win ? f->win : StdScreen(Get_Form_Screen(f)));
#else
  result = (f->win ? f->win : stdscr);
#endif
  returnWin(result);
}

 
