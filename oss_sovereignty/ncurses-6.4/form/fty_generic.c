 

 

#include "form.priv.h"

MODULE_ID("$Id: fty_generic.c,v 1.15 2021/03/27 23:49:53 tom Exp $")

 
#if NCURSES_INTEROP_FUNCS

 
static void *
Generic_This_Type(void *arg)
{
  return (arg);
}

 
FORM_EXPORT(FIELDTYPE *)
_nc_generic_fieldtype(bool (*const field_check) (FORM *, FIELD *, const void *),
		      bool (*const char_check) (int, FORM *, FIELD *, const
						void *),
		      bool (*const next) (FORM *, FIELD *, const void *),
		      bool (*const prev) (FORM *, FIELD *, const void *),
		      void (*freecallback) (void *))
{
  int code = E_SYSTEM_ERROR;
  FIELDTYPE *res = (FIELDTYPE *)0;

  TR_FUNC_BFR(5);

  T((T_CALLED("_nc_generic_fieldtype(%s,%s,%s,%s,%s)"),
     TR_FUNC_ARG(0, field_check),
     TR_FUNC_ARG(1, char_check),
     TR_FUNC_ARG(2, next),
     TR_FUNC_ARG(3, prev),
     TR_FUNC_ARG(4, freecallback)));

  if (field_check || char_check)
    {
      res = typeMalloc(FIELDTYPE, 1);

      if (res)
	{
	  *res = *_nc_Default_FieldType;
	  SetStatus(res, (_HAS_ARGS | _GENERIC));
	  res->fieldcheck.gfcheck = field_check;
	  res->charcheck.gccheck = char_check;
	  res->genericarg = Generic_This_Type;
	  res->freearg = freecallback;
	  res->enum_next.gnext = next;
	  res->enum_prev.gprev = prev;
	  code = E_OK;
	}
    }
  else
    code = E_BAD_ARGUMENT;

  if (E_OK != code)
    SET_ERROR(code);

  returnFieldType(res);
}

 
static TypeArgument *
GenericArgument(const FIELDTYPE *typ,
		int (*argiterator) (void **), int *err)
{
  TypeArgument *res = (TypeArgument *)0;

  if (typ != 0 && (typ->status & _HAS_ARGS) != 0 && err != 0 && argiterator != 0)
    {
      if (typ->status & _LINKED_TYPE)
	{
	   
	  TypeArgument *p = typeMalloc(TypeArgument, 1);

	  if (p)
	    {
	      p->left = GenericArgument(typ->left, argiterator, err);
	      p->right = GenericArgument(typ->right, argiterator, err);
	      return p;
	    }
	  else
	    *err += 1;
	}
      else
	{
	  assert(typ->genericarg != (void *)0);
	  if (typ->genericarg == 0)
	    *err += 1;
	  else
	    {
	      void *argp;
	      int valid = argiterator(&argp);

	      if (valid == 0 || argp == 0 ||
		  !(res = (TypeArgument *)typ->genericarg(argp)))
		{
		  *err += 1;
		}
	    }
	}
    }
  return res;
}

 
FORM_EXPORT(int)
_nc_set_generic_fieldtype(FIELD *field,
			  FIELDTYPE *ftyp,
			  int (*argiterator) (void **))
{
  int code = E_SYSTEM_ERROR;
  int err = 0;

  if (field)
    {
      if (field && field->type)
	_nc_Free_Type(field);

      field->type = ftyp;
      if (ftyp)
	{
	  if (argiterator)
	    {
	       
	      field->arg = (void *)GenericArgument(field->type, argiterator, &err);

	      if (err)
		{
		  _nc_Free_Argument(field->type, (TypeArgument *)(field->arg));
		  field->type = (FIELDTYPE *)0;
		  field->arg = (void *)0;
		}
	      else
		{
		  code = E_OK;
		  if (field->type)
		    field->type->ref++;
		}
	    }
	}
      else
	{
	  field->arg = (void *)0;
	  code = E_OK;
	}
    }
  return code;
}

 
FORM_EXPORT(WINDOW *)
_nc_form_cursor(const FORM *form, int *pRow, int *pCol)
{
  int code = E_SYSTEM_ERROR;
  WINDOW *res = (WINDOW *)0;

  if (form != 0 && pRow != 0 && pCol != 0)
    {
      *pRow = form->currow;
      *pCol = form->curcol;
      res = form->w;
      code = E_OK;
    }
  if (code != E_OK)
    SET_ERROR(code);
  return res;
}

#else
extern void _nc_fty_generic(void);
void
_nc_fty_generic(void)
{
}
#endif

 
