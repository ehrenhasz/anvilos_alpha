 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_opts.c,v 1.21 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(int)
set_form_opts(FORM *form, Form_Options opts)
{
  T((T_CALLED("set_form_opts(%p,%d)"), (void *)form, opts));

  opts &= (Form_Options)ALL_FORM_OPTS;
  if ((unsigned)opts & ~ALL_FORM_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Form(form)->opts = opts;
      RETURN(E_OK);
    }
}

 
FORM_EXPORT(Form_Options)
form_opts(const FORM *form)
{
  T((T_CALLED("form_opts(%p)"), (const void *)form));
  returnCode((Form_Options)((unsigned)Normalize_Form(form)->opts & ALL_FORM_OPTS));
}

 
FORM_EXPORT(int)
form_opts_on(FORM *form, Form_Options opts)
{
  T((T_CALLED("form_opts_on(%p,%d)"), (void *)form, opts));

  opts &= (Form_Options)ALL_FORM_OPTS;
  if ((unsigned)opts & ~ALL_FORM_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Form(form)->opts |= opts;
      RETURN(E_OK);
    }
}

 
FORM_EXPORT(int)
form_opts_off(FORM *form, Form_Options opts)
{
  T((T_CALLED("form_opts_off(%p,%d)"), (void *)form, opts));

  opts &= (Form_Options)ALL_FORM_OPTS;
  if ((unsigned)opts & ~ALL_FORM_OPTS)
    RETURN(E_BAD_ARGUMENT);
  else
    {
      Normalize_Form(form)->opts &= ~opts;
      RETURN(E_OK);
    }
}

 
