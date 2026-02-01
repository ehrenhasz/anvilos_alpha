 

 

#include "form.priv.h"

MODULE_ID("$Id: fty_int.c,v 1.33 2021/06/17 21:11:08 tom Exp $")

#if USE_WIDEC_SUPPORT
#define isDigit(c) (iswdigit((wint_t)(c)) || isdigit(UChar(c)))
#else
#define isDigit(c) isdigit(UChar(c))
#endif

#define thisARG integerARG

typedef struct
  {
    int precision;
    long low;
    long high;
  }
thisARG;

typedef struct
  {
    int precision;
    long low;
    long high;
  }
integerPARM;

 
static void *
Generic_This_Type(void *arg)
{
  thisARG *argp = (thisARG *)0;
  thisARG *param = (thisARG *)arg;

  if (param)
    {
      argp = typeMalloc(thisARG, 1);

      if (argp)
	{
	  T((T_CREATE("thisARG %p"), (void *)argp));
	  *argp = *param;
	}
    }
  return (void *)argp;
}

 
static void *
Make_This_Type(va_list *ap)
{
  thisARG arg;

  arg.precision = va_arg(*ap, int);
  arg.low = va_arg(*ap, long);
  arg.high = va_arg(*ap, long);

  return Generic_This_Type((void *)&arg);
}

 
static void *
Copy_This_Type(const void *argp)
{
  const thisARG *ap = (const thisARG *)argp;
  thisARG *result = (thisARG *)0;

  if (argp)
    {
      result = typeMalloc(thisARG, 1);

      if (result)
	{
	  T((T_CREATE("thisARG %p"), (void *)result));
	  *result = *ap;
	}
    }
  return (void *)result;
}

 
static void
Free_This_Type(void *argp)
{
  if (argp)
    free(argp);
}

 
static bool
Check_This_Field(FIELD *field, const void *argp)
{
  const thisARG *argi = (const thisARG *)argp;
  long low = argi->low;
  long high = argi->high;
  int prec = argi->precision;
  unsigned char *bp = (unsigned char *)field_buffer(field, 0);
  char *s = (char *)bp;
  bool result = FALSE;

  while (*bp == ' ')
    bp++;
  if (*bp)
    {
      if (*bp == '-')
	bp++;
#if USE_WIDEC_SUPPORT
      if (*bp)
	{
	  int len;
	  wchar_t *list = _nc_Widen_String((char *)bp, &len);

	  if (list != 0)
	    {
	      bool blank = FALSE;
	      int n;

	      result = TRUE;
	      for (n = 0; n < len; ++n)
		{
		  if (blank)
		    {
		      if (list[n] != ' ')
			{
			  result = FALSE;
			  break;
			}
		    }
		  else if (list[n] == ' ')
		    {
		      blank = TRUE;
		    }
		  else if (!isDigit(list[n]))
		    {
		      result = FALSE;
		      break;
		    }
		}
	      free(list);
	    }
	}
#else
      while (*bp)
	{
	  if (!isdigit(UChar(*bp)))
	    break;
	  bp++;
	}
      while (*bp && *bp == ' ')
	bp++;
      result = (*bp == '\0');
#endif
      if (result)
	{
	  long val = atol(s);

	  if (low < high)
	    {
	      if (val < low || val > high)
		result = FALSE;
	    }
	  if (result)
	    {
	      char buf[100];

	      _nc_SPRINTF(buf, _nc_SLIMIT(sizeof(buf))
			  "%.*ld", (prec > 0 ? prec : 0), val);
	      set_field_buffer(field, 0, buf);
	    }
	}
    }
  return (result);
}

 
static bool
Check_This_Character(int c, const void *argp GCC_UNUSED)
{
  return ((isDigit(UChar(c)) || (c == '-')) ? TRUE : FALSE);
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

FORM_EXPORT_VAR(FIELDTYPE *) TYPE_INTEGER = &typeTHIS;

#if NCURSES_INTEROP_FUNCS
 
FORM_EXPORT(FIELDTYPE *)
_nc_TYPE_INTEGER(void)
{
  return TYPE_INTEGER;
}
#endif

 
