 

 

#include "form.priv.h"

MODULE_ID("$Id: f_trace.c,v 1.6 2020/05/24 01:40:20 anonymous.maarten Exp $")

FORM_EXPORT(FIELD **)
_nc_retrace_field_ptr(FIELD **code)
{
  T((T_RETURN("%p"), (void *)code));
  return code;
}

FORM_EXPORT(FIELD *)
_nc_retrace_field(FIELD *code)
{
  T((T_RETURN("%p"), (void *)code));
  return code;
}

FORM_EXPORT(FIELDTYPE *)
_nc_retrace_field_type(FIELDTYPE *code)
{
  T((T_RETURN("%p"), (void *)code));
  return code;
}

FORM_EXPORT(FORM *)
_nc_retrace_form(FORM *code)
{
  T((T_RETURN("%p"), (void *)code));
  return code;
}

FORM_EXPORT(Form_Hook)
_nc_retrace_form_hook(Form_Hook code)
{
  TR_FUNC_BFR(1);
  T((T_RETURN("%s"), TR_FUNC_ARG(0, code)));
  return code;
}
