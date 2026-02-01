 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_newftyp.c,v 1.23 2020/05/24 01:40:20 anonymous.maarten Exp $")

static FIELDTYPE default_fieldtype =
{
  0,				 
  0L,				 
  (FIELDTYPE *)0,		 
  (FIELDTYPE *)0,		 
  NULL,				 
  NULL,				 
  NULL,				 
  INIT_FT_FUNC(NULL),		 
  INIT_FT_FUNC(NULL),		 
  INIT_FT_FUNC(NULL),		 
  INIT_FT_FUNC(NULL),		 
#if NCURSES_INTEROP_FUNCS
  NULL				 
#endif
};

FORM_EXPORT_VAR(FIELDTYPE *)
  _nc_Default_FieldType = &default_fieldtype;

 
FORM_EXPORT(FIELDTYPE *)
new_fieldtype(bool (*const field_check) (FIELD *, const void *),
	      bool (*const char_check) (int, const void *))
{
  FIELDTYPE *nftyp = (FIELDTYPE *)0;

  TR_FUNC_BFR(2);

  T((T_CALLED("new_fieldtype(%s,%s)"),
     TR_FUNC_ARG(0, field_check),
     TR_FUNC_ARG(1, char_check)));

  if ((field_check) || (char_check))
    {
      nftyp = typeMalloc(FIELDTYPE, 1);

      if (nftyp)
	{
	  T((T_CREATE("fieldtype %p"), (void *)nftyp));
	  *nftyp = default_fieldtype;
#if NCURSES_INTEROP_FUNCS
	  nftyp->fieldcheck.ofcheck = field_check;
	  nftyp->charcheck.occheck = char_check;
#else
	  nftyp->fcheck = field_check;
	  nftyp->ccheck = char_check;
#endif
	}
      else
	{
	  SET_ERROR(E_SYSTEM_ERROR);
	}
    }
  else
    {
      SET_ERROR(E_BAD_ARGUMENT);
    }
  returnFieldType(nftyp);
}

 
FORM_EXPORT(int)
free_fieldtype(FIELDTYPE *typ)
{
  T((T_CALLED("free_fieldtype(%p)"), (void *)typ));

  if (!typ)
    RETURN(E_BAD_ARGUMENT);

  if (typ->ref != 0)
    RETURN(E_CONNECTED);

  if (typ->status & _RESIDENT)
    RETURN(E_CONNECTED);

  if (typ->status & _LINKED_TYPE)
    {
      if (typ->left)
	typ->left->ref--;
      if (typ->right)
	typ->right->ref--;
    }
  free(typ);
  RETURN(E_OK);
}

 
