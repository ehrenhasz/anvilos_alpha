 

 

 

 

#ifndef MENU_PRIV_H_incl
#define MENU_PRIV_H_incl 1
 

#include "curses.priv.h"

#define NCURSES_OPAQUE_MENU 0

#include "mf_common.h"
#include "menu.h"

 
#define BS (8)

extern MENU_EXPORT_VAR(ITEM) _nc_Default_Item;
extern MENU_EXPORT_VAR(MENU) _nc_Default_Menu;

 
#define Normalize_Item( item ) ((item)=(item)?(item):&_nc_Default_Item)

 
#define Normalize_Menu( menu ) ((menu)=(menu)?(menu):&_nc_Default_Menu)

#define Get_Menu_Screen( menu ) (menu->userwin ? \
				 _nc_screen_of(menu->userwin) : CURRENT_SCREEN)

 
#define Get_Menu_UserWin(menu) ((menu)->userwin ? \
    (menu)->userwin : CURRENT_SCREEN->_stdscr)

 
#define Get_Menu_Window(  menu ) \
   ((menu)->usersub  ? (menu)->usersub  : Get_Menu_UserWin(menu))

 
#define _LINK_NEEDED    (0x04)
#define _MARK_ALLOCATED (0x08)

#define ALL_MENU_OPTS (                 \
		       O_ONEVALUE     | \
		       O_SHOWDESC     | \
		       O_ROWMAJOR     | \
		       O_IGNORECASE   | \
		       O_SHOWMATCH    | \
		       O_NONCYCLIC    | \
		       O_MOUSE_MENU   )

#define ALL_ITEM_OPTS (O_SELECTABLE)

 
#define Move_And_Post_Item(menu,item) \
  {wmove((menu)->win,(menu)->spc_rows*(item)->y,((menu)->itemlen+(menu)->spc_cols)*(item)->x);\
   _nc_Post_Item((menu),(item));}

#define Move_To_Current_Item(menu,item) \
  if ( (item) != (menu)->curitem)\
    {\
      Move_And_Post_Item(menu,item);\
      Move_And_Post_Item(menu,(menu)->curitem);\
    }

 
#define Adjust_Current_Item(menu,row,item) \
  { if ((item)->y < row) \
      row = (short) (item)->y; \
    if ( (item)->y >= (row + (menu)->arows) ) \
      row = (short) (( (item)->y < ((menu)->rows - row) ) \
                     ? (item)->y \
		     : (menu)->rows - (menu)->arows); \
    _nc_New_TopRow_and_CurrentItem(menu,row,item); }

 
#define Reset_Pattern(menu) \
  { (menu)->pindex = 0; \
    (menu)->pattern[0] = '\0'; }

#define UChar(c)	((unsigned char)(c))

 
extern MENU_EXPORT(void) _nc_Draw_Menu (const MENU *);
extern MENU_EXPORT(void) _nc_Show_Menu (const MENU *);
extern MENU_EXPORT(void) _nc_Calculate_Item_Length_and_Width (MENU *);
extern MENU_EXPORT(int)  _nc_Calculate_Text_Width(const TEXT *);
extern MENU_EXPORT(void) _nc_Post_Item (const MENU *, const ITEM *);
extern MENU_EXPORT(bool) _nc_Connect_Items (MENU *, ITEM **);
extern MENU_EXPORT(void) _nc_Disconnect_Items (MENU *);
extern MENU_EXPORT(void) _nc_New_TopRow_and_CurrentItem (MENU *,int, ITEM *);
extern MENU_EXPORT(void) _nc_Link_Items (MENU *);
extern MENU_EXPORT(int)  _nc_Match_Next_Character_In_Item_Name (MENU*,int,ITEM**);
extern MENU_EXPORT(int)  _nc_menu_cursor_pos (const MENU* menu, const ITEM* item,
				int* pY, int* pX);

#ifdef TRACE

#define returnItem(code)	TRACE_RETURN1(code,item)
#define returnItemPtr(code)	TRACE_RETURN1(code,item_ptr)
#define returnItemOpts(code)	TRACE_RETURN1(code,item_opts)
#define returnMenu(code)	TRACE_RETURN1(code,menu)
#define returnMenuHook(code)	TRACE_RETURN1(code,menu_hook)
#define returnMenuOpts(code)	TRACE_RETURN1(code,menu_opts)

extern MENU_EXPORT(ITEM *)	    _nc_retrace_item (ITEM *);
extern MENU_EXPORT(ITEM **)	    _nc_retrace_item_ptr (ITEM **);
extern MENU_EXPORT(Item_Options) _nc_retrace_item_opts (Item_Options);
extern MENU_EXPORT(MENU *)	    _nc_retrace_menu (MENU *);
extern MENU_EXPORT(Menu_Hook)    _nc_retrace_menu_hook (Menu_Hook);
extern MENU_EXPORT(Menu_Options) _nc_retrace_menu_opts (Menu_Options);

#else  

#define returnItem(code)	return code
#define returnItemPtr(code)	return code
#define returnItemOpts(code)	return code
#define returnMenu(code)	return code
#define returnMenuHook(code)	return code
#define returnMenuOpts(code)	return code

#endif  
 

#endif  
