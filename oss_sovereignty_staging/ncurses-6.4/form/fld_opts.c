 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_opts.c,v 1.16 2021/06/17 21:20:30 tom Exp $")

 

 
FORM_EXPORT(int)
set_field_opts(FIELD *field, Field_Options opts)
{
  int res = E_BAD_ARGUMENT;

  T((T_CALLED("set_field_opts(%p,%d)"), (void *)field, opts));

  opts &= ALL_FIELD_OPTS;
  if (!(opts & ~ALL_FIELD_OPTS))
    res = _nc_Synchronize_Options(Normalize_Field(field), opts);
  RETURN(res);
}

 
FORM_EXPORT(Field_Options)
field_opts(const FIELD *field)
{
  T((T_CALLED("field_opts(%p)"), (const void *)field));

  returnCode(ALL_FIELD_OPTS & Normalize_Field(field)->opts);
}

 
FORM_EXPORT(int)
field_opts_on(FIELD *field, Field_Options opts)
{
  int res = E_BAD_ARGUMENT;

  T((T_CALLED("field_opts_on(%p,%d)"), (void *)field, opts));

  opts &= ALL_FIELD_OPTS;
  if (!(opts & ~ALL_FIELD_OPTS))
    {
      Normalize_Field(field);
      res = _nc_Synchronize_Options(field, field->opts | opts);
    }
  RETURN(res);
}

 
FORM_EXPORT(int)
field_opts_off(FIELD *field, Field_Options opts)
{
  int res = E_BAD_ARGUMENT;

  T((T_CALLED("field_opts_off(%p,%d)"), (void *)field, opts));

  opts &= ALL_FIELD_OPTS;
  if (!(opts & ~ALL_FIELD_OPTS))
    {
      Normalize_Field(field);
      res = _nc_Synchronize_Options(field, field->opts & ~opts);
    }
  RETURN(res);
}

 
