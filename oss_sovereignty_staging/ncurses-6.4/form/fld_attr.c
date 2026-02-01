 

 

#include "form.priv.h"

MODULE_ID("$Id: fld_attr.c,v 1.15 2020/12/11 22:05:24 tom Exp $")

 
 
#define GEN_FIELD_ATTR_SET_FCT( name ) \
FORM_IMPEXP int NCURSES_API set_field_ ## name (FIELD * field, chtype attr)\
{\
   int res = E_BAD_ARGUMENT;\
   T((T_CALLED("set_field_" #name "(%p,%s)"), (void *)field, _traceattr(attr)));\
   if ( attr==A_NORMAL || ((attr & A_ATTRIBUTES)==attr) )\
     {\
       Normalize_Field( field );\
       if (field != 0) \
	 { \
	 if ((field -> name) != attr)\
	   {\
	     field -> name = attr;\
	     res = _nc_Synchronize_Attributes( field );\
	   }\
	 else\
	   {\
	     res = E_OK;\
	   }\
	 }\
     }\
   RETURN(res);\
}

 
#define GEN_FIELD_ATTR_GET_FCT( name ) \
FORM_IMPEXP chtype NCURSES_API field_ ## name (const FIELD * field)\
{\
   T((T_CALLED("field_" #name "(%p)"), (const void *) field));\
   returnAttr( A_ATTRIBUTES & (Normalize_Field( field ) -> name) );\
}

 
GEN_FIELD_ATTR_SET_FCT(fore)

 
GEN_FIELD_ATTR_GET_FCT(fore)

 
GEN_FIELD_ATTR_SET_FCT(back)

 
GEN_FIELD_ATTR_GET_FCT(back)

 
