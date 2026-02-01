 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_arg.c,v 1.18 2020/12/11 22:05:24 tom Exp $")

 
FORM_EXPORT(int)
set_fieldtype_arg(FIELDTYPE *typ,
		  void *(*const make_arg)(va_list *),
		  void *(*const copy_arg)(const void *),
		  void (*const free_arg) (void *))
{
  TR_FUNC_BFR(3);

  T((T_CALLED("set_fieldtype_arg(%p,%s,%s,%s)"),
     (void *)typ,
     TR_FUNC_ARG(0, make_arg),
     TR_FUNC_ARG(1, copy_arg),
     TR_FUNC_ARG(2, free_arg)));

  if (typ != 0 && make_arg != (void *)0)
    {
      SetStatus(typ, _HAS_ARGS);
      typ->makearg = make_arg;
      typ->copyarg = copy_arg;
      typ->freearg = free_arg;
      RETURN(E_OK);
    }
  RETURN(E_BAD_ARGUMENT);
}

 
FORM_EXPORT(void *)
field_arg(const FIELD *field)
{
  T((T_CALLED("field_arg(%p)"), (const void *)field));
  returnVoidPtr(Normalize_Field(field)->arg);
}

 
