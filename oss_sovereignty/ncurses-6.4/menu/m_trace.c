 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_trace.c,v 1.9 2020/12/12 00:38:14 tom Exp $")

MENU_EXPORT(ITEM *)
_nc_retrace_item(ITEM *code)
{
  T((T_RETURN("%p"), (void *)code));
  return code;
}

MENU_EXPORT(ITEM **)
_nc_retrace_item_ptr(ITEM **code)
{
  T((T_RETURN("%p"), (void *)code));
  return code;
}

MENU_EXPORT(Item_Options)
_nc_retrace_item_opts(Item_Options code)
{
  T((T_RETURN("%d"), code));
  return code;
}

MENU_EXPORT(MENU *)
_nc_retrace_menu(MENU *code)
{
  T((T_RETURN("%p"), (void *)code));
  return code;
}

MENU_EXPORT(Menu_Hook)
_nc_retrace_menu_hook(Menu_Hook code)
{
  TR_FUNC_BFR(1);
  T((T_RETURN("%s"), TR_FUNC_ARG(0, code)));
  return code;
}

MENU_EXPORT(Menu_Options)
_nc_retrace_menu_opts(Menu_Options code)
{
  T((T_RETURN("%d"), code));
  return code;
}
