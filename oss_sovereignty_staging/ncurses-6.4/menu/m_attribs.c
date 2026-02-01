 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_attribs.c,v 1.20 2021/06/17 21:20:30 tom Exp $")

 
#define Refresh_Menu(menu) \
   if ( (menu) && ((menu)->status & _POSTED) )\
   {\
      _nc_Draw_Menu( menu );\
      _nc_Show_Menu( menu );\
   }

 
#define GEN_MENU_ATTR_SET_FCT( name ) \
MENU_EXPORT(int) NCURSES_API set_menu_ ## name (MENU* menu, chtype attr) \
{\
  T((T_CALLED("set_menu_" #name "(%p,%s)"), (void *) menu, _traceattr(attr))); \
   if (!(attr==A_NORMAL || (attr & A_ATTRIBUTES)==attr))\
      RETURN(E_BAD_ARGUMENT);\
   if (menu && ( menu -> name != attr))\
     {\
       (menu -> name) = attr;\
       Refresh_Menu(menu);\
     }\
   Normalize_Menu( menu ) -> name = attr;\
   RETURN(E_OK);\
}

 
#define GEN_MENU_ATTR_GET_FCT( name ) \
MENU_EXPORT(chtype) NCURSES_API menu_ ## name (const MENU * menu)\
{\
   T((T_CALLED("menu_" #name "(%p)"), (const void *) menu));\
   returnAttr(Normalize_Menu( menu ) -> name);\
}

 
GEN_MENU_ATTR_SET_FCT(fore)

 
GEN_MENU_ATTR_GET_FCT(fore)

 
GEN_MENU_ATTR_SET_FCT(back)

 
GEN_MENU_ATTR_GET_FCT(back)

 
GEN_MENU_ATTR_SET_FCT(grey)

 
GEN_MENU_ATTR_GET_FCT(grey)

 
