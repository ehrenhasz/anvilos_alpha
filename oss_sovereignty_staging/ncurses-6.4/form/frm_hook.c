 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_hook.c,v 1.20 2020/05/24 01:40:20 anonymous.maarten Exp $")

 
#define GEN_HOOK_SET_FUNCTION( typ, name ) \
FORM_IMPEXP int NCURSES_API set_ ## typ ## _ ## name (FORM *form, Form_Hook func)\
{\
   TR_FUNC_BFR(1); \
   T((T_CALLED("set_" #typ"_"#name"(%p,%s)"), (void *) form, TR_FUNC_ARG(0, func)));\
   (Normalize_Form( form ) -> typ ## name) = func ;\
   RETURN(E_OK);\
}

 
#define GEN_HOOK_GET_FUNCTION( typ, name ) \
FORM_IMPEXP Form_Hook NCURSES_API typ ## _ ## name ( const FORM *form )\
{\
   T((T_CALLED(#typ "_" #name "(%p)"), (const void *) form));\
   returnFormHook( Normalize_Form( form ) -> typ ## name );\
}

 
GEN_HOOK_SET_FUNCTION(field, init)

 
GEN_HOOK_GET_FUNCTION(field, init)

 
GEN_HOOK_SET_FUNCTION(field, term)

 
GEN_HOOK_GET_FUNCTION(field, term)

 
GEN_HOOK_SET_FUNCTION(form, init)

 
GEN_HOOK_GET_FUNCTION(form, init)

 
GEN_HOOK_SET_FUNCTION(form, term)

 
GEN_HOOK_GET_FUNCTION(form, term)

 
