 

 

#include "form.priv.h"

MODULE_ID("$Id: fty_alpha.c,v 1.31 2020/12/12 01:15:37 tom Exp $")

#define thisARG alphaARG

typedef struct
  {
    int width;
  }
thisARG;

 
static void *
Generic_This_Type(void *arg)
{
  thisARG *argp = (thisARG *)0;

  if (arg)
    {
      argp = typeMalloc(thisARG, 1);

      if (argp)
	{
	  T((T_CREATE("thisARG %p"), (void *)argp));
	  argp->width = *((int *)arg);
	}
    }
  return ((void *)argp);
}

 
static void *
Make_This_Type(va_list *ap)
{
  int w = va_arg(*ap, int);

  return Generic_This_Type((void *)&w);
}

 
static void *
Copy_This_Type(const void *argp)
{
  const thisARG *ap = (const thisARG *)argp;
  thisARG *result = typeMalloc(thisARG, 1);

  if (result)
    {
      T((T_CREATE("thisARG %p"), (void *)result));
      *result = *ap;
    }

  return ((void *)result);
}

 
static void
Free_This_Type(void *argp)
{
  if (argp)
    free(argp);
}

 
static bool
Check_This_Character(int c, const void *argp GCC_UNUSED)
{
#if USE_WIDEC_SUPPORT
  if (iswalpha((wint_t)c))
    return TRUE;
#endif
  return (isalpha(UChar(c)) ? TRUE : FALSE);
}

 
static bool
Check_This_Field(FIELD *field, const void *argp)
{
  int width = ((const thisARG *)argp)->width;
  unsigned char *bp = (unsigned char *)field_buffer(field, 0);
  bool result = (width < 0);

  Check_CTYPE_Field(result, bp, width, Check_This_Character);
  return (result);
}

static FIELDTYPE typeTHIS =
{
  _HAS_ARGS | _RESIDENT,
  1,				 
  (FIELDTYPE *)0,
  (FIELDTYPE *)0,
  Make_This_Type,
  Copy_This_Type,
  Free_This_Type,
  INIT_FT_FUNC(Check_This_Field),
  INIT_FT_FUNC(Check_This_Character),
  INIT_FT_FUNC(NULL),
  INIT_FT_FUNC(NULL),
#if NCURSES_INTEROP_FUNCS
  Generic_This_Type
#endif
};

FORM_EXPORT_VAR(FIELDTYPE *) TYPE_ALPHA = &typeTHIS;

#if NCURSES_INTEROP_FUNCS
 
FORM_EXPORT(FIELDTYPE *)
_nc_TYPE_ALPHA(void)
{
  return TYPE_ALPHA;
}
#endif

 
