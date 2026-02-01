 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_hook.c,v 1.21 2021/06/17 21:26:02 tom Exp $")

 
#define GEN_HOOK_SET_FUNCTION( typ, name ) \
MENU_EXPORT(int) NCURSES_API set_ ## typ ## _ ## name (MENU *menu, Menu_Hook func )\
{\
   TR_FUNC_BFR(1);\
   T((T_CALLED("set_" #typ "_" #name "(%p,%s)"), (void *) menu, TR_FUNC_ARG(0, func)));\
   (Normalize_Menu(menu) -> typ ## name = func );\
   RETURN(E_OK);\
}

 
#define GEN_HOOK_GET_FUNCTION( typ, name ) \
MENU_EXPORT(Menu_Hook) NCURSES_API typ ## _ ## name ( const MENU *menu )\
{\
   T((T_CALLED(#typ "_" #name "(%p)"), (const void *) menu));\
   returnMenuHook(Normalize_Menu(menu) -> typ ## name);\
}

 
GEN_HOOK_SET_FUNCTION(menu, init)

 
GEN_HOOK_GET_FUNCTION(menu, init)

 
GEN_HOOK_SET_FUNCTION(menu, term)

 
GEN_HOOK_GET_FUNCTION(menu, term)

 
GEN_HOOK_SET_FUNCTION(item, init)

 
GEN_HOOK_GET_FUNCTION(item, init)

 
GEN_HOOK_SET_FUNCTION(item, term)

 
GEN_HOOK_GET_FUNCTION(item, term)

 
