 

 

 

#include "menu.priv.h"

#if USE_WIDEC_SUPPORT
#if HAVE_WCTYPE_H
#include <wctype.h>
#endif
#endif

MODULE_ID("$Id: m_item_new.c,v 1.38 2021/06/17 21:26:02 tom Exp $")

 
static bool
Is_Printable_String(const char *s)
{
  int result = TRUE;

#if USE_WIDEC_SUPPORT
  int count = (int)mbstowcs(0, s, 0);
  wchar_t *temp = 0;

  assert(s);

  if (count > 0
      && (temp = typeCalloc(wchar_t, (2 + (unsigned)count))) != 0)
    {
      int n;

      mbstowcs(temp, s, (unsigned)count);
      for (n = 0; n < count; ++n)
	if (!iswprint((wint_t)temp[n]))
	  {
	    result = FALSE;
	    break;
	  }
      free(temp);
    }
#else
  assert(s);
  while (*s)
    {
      if (!isprint(UChar(*s)))
	{
	  result = FALSE;
	  break;
	}
      s++;
    }
#endif
  return result;
}

 
MENU_EXPORT(ITEM *)
new_item(const char *name, const char *description)
{
  ITEM *item;

  T((T_CALLED("new_item(\"%s\", \"%s\")"),
     name ? name : "",
     description ? description : ""));

  if (!name || (*name == '\0') || !Is_Printable_String(name))
    {
      item = (ITEM *)0;
      SET_ERROR(E_BAD_ARGUMENT);
    }
  else
    {
      item = typeCalloc(ITEM, 1);

      if (item)
	{
	  T((T_CREATE("item %p"), (void *)item));
	  *item = _nc_Default_Item;	 

	  item->name.length = (unsigned short)strlen(name);
	  item->name.str = name;

	  if (description && (*description != '\0') &&
	      Is_Printable_String(description))
	    {
	      item->description.length = (unsigned short)strlen(description);
	      item->description.str = description;
	    }
	  else
	    {
	      item->description.length = 0;
	      item->description.str = (char *)0;
	    }
	}
      else
	SET_ERROR(E_SYSTEM_ERROR);
    }
  returnItem(item);
}

 
MENU_EXPORT(int)
free_item(ITEM *item)
{
  T((T_CALLED("free_item(%p)"), (void *)item));

  if (!item)
    RETURN(E_BAD_ARGUMENT);

  if (item->imenu)
    RETURN(E_CONNECTED);

  free(item);

  RETURN(E_OK);
}

 
MENU_EXPORT(int)
set_menu_mark(MENU *menu, const char *mark)
{
  short l;

  T((T_CALLED("set_menu_mark(%p,%s)"), (void *)menu, _nc_visbuf(mark)));

  if (mark && (*mark != '\0') && Is_Printable_String(mark))
    l = (short)strlen(mark);
  else
    l = 0;

  if (menu)
    {
      char *old_mark = menu->mark;
      unsigned short old_status = menu->status;

      if (menu->status & _POSTED)
	{
	   
	  if (menu->marklen != l)
	    RETURN(E_BAD_ARGUMENT);
	}
      menu->marklen = l;
      if (l)
	{
	  menu->mark = strdup(mark);
	  if (menu->mark)
	    {
	      if (menu != &_nc_Default_Menu)
		SetStatus(menu, _MARK_ALLOCATED);
	    }
	  else
	    {
	      menu->mark = old_mark;
	      menu->marklen = (short)((old_mark != 0) ? strlen(old_mark) : 0);
	      RETURN(E_SYSTEM_ERROR);
	    }
	}
      else
	menu->mark = (char *)0;

      if ((old_status & _MARK_ALLOCATED) && old_mark)
	free(old_mark);

      if (menu->status & _POSTED)
	{
	  _nc_Draw_Menu(menu);
	  _nc_Show_Menu(menu);
	}
      else
	{
	   
	  _nc_Calculate_Item_Length_and_Width(menu);
	}
    }
  else
    {
      returnCode(set_menu_mark(&_nc_Default_Menu, mark));
    }
  RETURN(E_OK);
}

 
MENU_EXPORT(const char *)
menu_mark(const MENU *menu)
{
  T((T_CALLED("menu_mark(%p)"), (const void *)menu));
  returnPtr(Normalize_Menu(menu)->mark);
}

 
