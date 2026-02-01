 

 

#include "form.priv.h"

MODULE_ID("$Id: fty_ipv4.c,v 1.16 2021/06/17 21:11:08 tom Exp $")

 
static bool
Check_IPV4_Field(FIELD *field, const void *argp GCC_UNUSED)
{
  char *bp = field_buffer(field, 0);
  int num = 0, len;
  unsigned int d1 = 0, d2 = 0, d3 = 0, d4 = 0;

  if (isdigit(UChar(*bp)))	 
    {
      num = sscanf(bp, "%u.%u.%u.%u%n", &d1, &d2, &d3, &d4, &len);
      if (num == 4)
	{
	  bp += len;		 
	  while (isspace(UChar(*bp)))
	    bp++;		 
	}
    }
  return ((num != 4 || *bp || d1 > 255 || d2 > 255
	   || d3 > 255 || d4 > 255) ? FALSE : TRUE);
}

 
static bool
Check_IPV4_Character(int c, const void *argp GCC_UNUSED)
{
  return ((isdigit(UChar(c)) || (c == '.')) ? TRUE : FALSE);
}

static FIELDTYPE typeIPV4 =
{
  _RESIDENT,
  1,				 
  (FIELDTYPE *)0,
  (FIELDTYPE *)0,
  NULL,
  NULL,
  NULL,
  INIT_FT_FUNC(Check_IPV4_Field),
  INIT_FT_FUNC(Check_IPV4_Character),
  INIT_FT_FUNC(NULL),
  INIT_FT_FUNC(NULL),
#if NCURSES_INTEROP_FUNCS
  NULL
#endif
};

FORM_EXPORT_VAR(FIELDTYPE *) TYPE_IPV4 = &typeIPV4;

#if NCURSES_INTEROP_FUNCS
 
FORM_EXPORT(FIELDTYPE *)
_nc_TYPE_IPV4(void)
{
  return TYPE_IPV4;
}
#endif

 
