 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_ftchoice.c,v 1.18 2021/06/17 21:26:02 tom Exp $")

 
FORM_EXPORT(int)
set_fieldtype_choice(FIELDTYPE *typ,
		     bool (*const next_choice) (FIELD *, const void *),
		     bool (*const prev_choice) (FIELD *, const void *))
{
  TR_FUNC_BFR(2);

  T((T_CALLED("set_fieldtype_choice(%p,%s,%s)"),
     (void *)typ,
     TR_FUNC_ARG(0, next_choice),
     TR_FUNC_ARG(1, prev_choice)));

  if (!typ || !next_choice || !prev_choice)
    RETURN(E_BAD_ARGUMENT);

  SetStatus(typ, _HAS_CHOICE);
#if NCURSES_INTEROP_FUNCS
  typ->enum_next.onext = next_choice;
  typ->enum_prev.oprev = prev_choice;
#else
  typ->next = next_choice;
  typ->prev = prev_choice;
#endif
  RETURN(E_OK);
}

 
