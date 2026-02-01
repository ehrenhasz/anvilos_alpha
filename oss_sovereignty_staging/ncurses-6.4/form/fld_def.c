 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_def.c,v 1.44 2021/03/27 23:49:53 tom Exp $")

 
static FIELD default_field =
{
  0,				 
  0,				 
  0,				 
  0,				 
  0,				 
  0,				 
  0,				 
  0,				 
  0,				 
  0,				 
  NO_JUSTIFICATION,		 
  0,				 
  0,				 
  (int)' ',			 
  A_NORMAL,			 
  A_NORMAL,			 
  STD_FIELD_OPTS,		 
  (FIELD *)0,			 
  (FIELD *)0,			 
  (FIELD *)0,			 
  (FORM *)0,			 
  (FIELDTYPE *)0,		 
  (char *)0,			 
  (FIELD_CELL *)0,		 
  (char *)0			 
  NCURSES_FIELD_EXTENSION
};

FORM_EXPORT_VAR(FIELD *) _nc_Default_Field = &default_field;

 
FORM_EXPORT(TypeArgument *)
_nc_Make_Argument(const FIELDTYPE *typ, va_list *ap, int *err)
{
  TypeArgument *res = (TypeArgument *)0;

  if (typ != 0 && (typ->status & _HAS_ARGS) != 0)
    {
      assert(err != 0 && ap != (va_list *)0);
      if ((typ->status & _LINKED_TYPE) != 0)
	{
	  TypeArgument *p = typeMalloc(TypeArgument, 1);

	  if (p != 0)
	    {
	      p->left = _nc_Make_Argument(typ->left, ap, err);
	      p->right = _nc_Make_Argument(typ->right, ap, err);
	      return p;
	    }
	  else
	    {
	      *err += 1;
	    }
	}
      else
	{
	  assert(typ->makearg != (void *)0);
	  if (!(res = (TypeArgument *)typ->makearg(ap)))
	    {
	      *err += 1;
	    }
	}
    }
  return res;
}

 
FORM_EXPORT(TypeArgument *)
_nc_Copy_Argument(const FIELDTYPE *typ, const TypeArgument *argp, int *err)
{
  TypeArgument *res = (TypeArgument *)0;

  if (typ != 0 && (typ->status & _HAS_ARGS) != 0)
    {
      assert(err != 0 && argp != 0);
      if ((typ->status & _LINKED_TYPE) != 0)
	{
	  TypeArgument *p = typeMalloc(TypeArgument, 1);

	  if (p != 0)
	    {
	      p->left = _nc_Copy_Argument(typ, argp->left, err);
	      p->right = _nc_Copy_Argument(typ, argp->right, err);
	      return p;
	    }
	  *err += 1;
	}
      else
	{
	  if (typ->copyarg != (void *)0)
	    {
	      if (!(res = (TypeArgument *)(typ->copyarg((const void *)argp))))
		{
		  *err += 1;
		}
	    }
	  else
	    {
	      res = (TypeArgument *)argp;
	    }
	}
    }
  return res;
}

 
FORM_EXPORT(void)
_nc_Free_Argument(const FIELDTYPE *typ, TypeArgument *argp)
{
  if (typ != 0 && (typ->status & _HAS_ARGS) != 0)
    {
      if ((typ->status & _LINKED_TYPE) != 0)
	{
	  if (argp != 0)
	    {
	      _nc_Free_Argument(typ->left, argp->left);
	      _nc_Free_Argument(typ->right, argp->right);
	      free(argp);
	    }
	}
      else
	{
	  if (typ->freearg != (void *)0)
	    {
	      typ->freearg((void *)argp);
	    }
	}
    }
}

 
FORM_EXPORT(bool)
_nc_Copy_Type(FIELD *dst, FIELD const *src)
{
  int err = 0;

  assert(dst != 0 && src != 0);

  dst->type = src->type;
  dst->arg = (void *)_nc_Copy_Argument(src->type, (TypeArgument *)(src->arg), &err);

  if (err != 0)
    {
      _nc_Free_Argument(dst->type, (TypeArgument *)(dst->arg));
      dst->type = (FIELDTYPE *)0;
      dst->arg = (void *)0;
      return FALSE;
    }
  else
    {
      if (dst->type != 0)
	{
	  dst->type->ref++;
	}
      return TRUE;
    }
}

 
FORM_EXPORT(void)
_nc_Free_Type(FIELD *field)
{
  assert(field != 0);
  if (field->type != 0)
    {
      field->type->ref--;
      _nc_Free_Argument(field->type, (TypeArgument *)(field->arg));
    }
}

 
FORM_EXPORT(FIELD *)
new_field(int rows, int cols, int frow, int fcol, int nrow, int nbuf)
{
  static const FIELD_CELL blank = BLANK;
  static const FIELD_CELL zeros = ZEROS;

  FIELD *New_Field = (FIELD *)0;
  int err = E_BAD_ARGUMENT;

  T((T_CALLED("new_field(%d,%d,%d,%d,%d,%d)"), rows, cols, frow, fcol, nrow, nbuf));
  if (rows > 0 &&
      cols > 0 &&
      frow >= 0 &&
      fcol >= 0 &&
      nrow >= 0 &&
      nbuf >= 0 &&
      ((err = E_SYSTEM_ERROR) != 0) &&	 
      (New_Field = typeMalloc(FIELD, 1)) != 0)
    {
      T((T_CREATE("field %p"), (void *)New_Field));
      *New_Field = default_field;
      New_Field->rows = (short)rows;
      New_Field->cols = (short)cols;
      New_Field->drows = rows + nrow;
      New_Field->dcols = cols;
      New_Field->frow = (short)frow;
      New_Field->fcol = (short)fcol;
      New_Field->nrow = nrow;
      New_Field->nbuf = (short)nbuf;
      New_Field->link = New_Field;

#if USE_WIDEC_SUPPORT
      New_Field->working = newpad(1, Buffer_Length(New_Field) + 1);
      New_Field->expanded = typeCalloc(char *, 1 + (unsigned)nbuf);
#endif

      if (_nc_Copy_Type(New_Field, &default_field))
	{
	  size_t len;

	  len = Total_Buffer_Size(New_Field);
	  if ((New_Field->buf = (FIELD_CELL *)malloc(len)))
	    {
	       
	      int i, j;
	      int cells = Buffer_Length(New_Field);

	      for (i = 0; i <= New_Field->nbuf; i++)
		{
		  FIELD_CELL *buffer = &(New_Field->buf[(cells + 1) * i]);

		  for (j = 0; j < cells; ++j)
		    {
		      buffer[j] = blank;
		    }
		  buffer[j] = zeros;
		}
	      returnField(New_Field);
	    }
	}
    }

  if (New_Field)
    free_field(New_Field);

  SET_ERROR(err);
  returnField((FIELD *)0);
}

 
FORM_EXPORT(int)
free_field(FIELD *field)
{
  T((T_CALLED("free_field(%p)"), (void *)field));
  if (!field)
    {
      RETURN(E_BAD_ARGUMENT);
    }
  else if (field->form != 0)
    {
      RETURN(E_CONNECTED);
    }
  else if (field == field->link)
    {
      if (field->buf != 0)
	free(field->buf);
    }
  else
    {
      FIELD *f;

      for (f = field; f->link != field; f = f->link)
	{
	}
      f->link = field->link;
    }
  _nc_Free_Type(field);
#if USE_WIDEC_SUPPORT
  if (field->expanded != 0)
    {
      int n;

      for (n = 0; n <= field->nbuf; ++n)
	{
	  FreeIfNeeded(field->expanded[n]);
	}
      free(field->expanded);
      (void)delwin(field->working);
    }
#endif
  free(field);
  RETURN(E_OK);
}

 
