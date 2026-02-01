 

 

 
 

#include "c_varargs_to_ada.h"

int
set_field_type_alnum(FIELD *field,
		     int minimum_width)
{
  return set_field_type(field, TYPE_ALNUM, minimum_width);
}

int
set_field_type_alpha(FIELD *field,
		     int minimum_width)
{
  return set_field_type(field, TYPE_ALPHA, minimum_width);
}

int
set_field_type_enum(FIELD *field,
		    char **value_list,
		    int case_sensitive,
		    int unique_match)
{
  return set_field_type(field, TYPE_ENUM, value_list, case_sensitive,
			unique_match);
}

int
set_field_type_integer(FIELD *field,
		       int precision,
		       long minimum,
		       long maximum)
{
  return set_field_type(field, TYPE_INTEGER, precision, minimum, maximum);
}

int
set_field_type_numeric(FIELD *field,
		       int precision,
		       double minimum,
		       double maximum)
{
  return set_field_type(field, TYPE_NUMERIC, precision, minimum, maximum);
}

int
set_field_type_regexp(FIELD *field,
		      char *regular_expression)
{
  return set_field_type(field, TYPE_REGEXP, regular_expression);
}

int
set_field_type_ipv4(FIELD *field)
{
  return set_field_type(field, TYPE_IPV4);
}

int
set_field_type_user(FIELD *field,
		    FIELDTYPE *fieldtype,
		    void *arg)
{
  return set_field_type(field, fieldtype, arg);
}

void *
void_star_make_arg(va_list *list)
{
  return va_arg(*list, void *);
}

#ifdef TRACE
void
_traces(const char *fmt, char *arg)
{
  _tracef(fmt, arg);
}
#endif
