 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_ftlink.c,v 1.18 2021/06/17 21:20:30 tom Exp $")

 
FORM_EXPORT(FIELDTYPE *)
link_fieldtype(FIELDTYPE *type1, FIELDTYPE *type2)
{
  FIELDTYPE *nftyp = (FIELDTYPE *)0;

  T((T_CALLED("link_fieldtype(%p,%p)"), (void *)type1, (void *)type2));
  if (type1 && type2)
    {
      nftyp = typeMalloc(FIELDTYPE, 1);

      if (nftyp)
	{
	  T((T_CREATE("fieldtype %p"), (void *)nftyp));
	  *nftyp = *_nc_Default_FieldType;
	  SetStatus(nftyp, _LINKED_TYPE);
	  if ((type1->status & _HAS_ARGS) || (type2->status & _HAS_ARGS))
	    SetStatus(nftyp, _HAS_ARGS);
	  if ((type1->status & _HAS_CHOICE) || (type2->status & _HAS_CHOICE))
	    SetStatus(nftyp, _HAS_CHOICE);
	  nftyp->left = type1;
	  nftyp->right = type2;
	  type1->ref++;
	  type2->ref++;
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

 
