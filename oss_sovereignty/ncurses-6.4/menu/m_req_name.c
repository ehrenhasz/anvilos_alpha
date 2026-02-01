 

 

 

#include "menu.priv.h"

MODULE_ID("$Id: m_req_name.c,v 1.27 2021/06/17 21:11:08 tom Exp $")

#define DATA(s) { s }

static const char request_names[MAX_MENU_COMMAND - MIN_MENU_COMMAND + 1][14] =
{
  DATA("LEFT_ITEM"),
  DATA("RIGHT_ITEM"),
  DATA("UP_ITEM"),
  DATA("DOWN_ITEM"),
  DATA("SCR_ULINE"),
  DATA("SCR_DLINE"),
  DATA("SCR_DPAGE"),
  DATA("SCR_UPAGE"),
  DATA("FIRST_ITEM"),
  DATA("LAST_ITEM"),
  DATA("NEXT_ITEM"),
  DATA("PREV_ITEM"),
  DATA("TOGGLE_ITEM"),
  DATA("CLEAR_PATTERN"),
  DATA("BACK_PATTERN"),
  DATA("NEXT_MATCH"),
  DATA("PREV_MATCH")
};

#define A_SIZE (sizeof(request_names)/sizeof(request_names[0]))

 
MENU_EXPORT(const char *)
menu_request_name(int request)
{
  T((T_CALLED("menu_request_name(%d)"), request));
  if ((request < MIN_MENU_COMMAND) || (request > MAX_MENU_COMMAND))
    {
      SET_ERROR(E_BAD_ARGUMENT);
      returnCPtr((const char *)0);
    }
  else
    returnCPtr(request_names[request - MIN_MENU_COMMAND]);
}

 
MENU_EXPORT(int)
menu_request_by_name(const char *str)
{
   
  size_t i = 0;

  T((T_CALLED("menu_request_by_name(%s)"), _nc_visbuf(str)));

  if (str != 0 && (i = strlen(str)) != 0)
    {
      char buf[16];

      if (i > sizeof(buf) - 2)
	i = sizeof(buf) - 2;
      memcpy(buf, str, i);
      buf[i] = '\0';

      for (i = 0; buf[i] != '\0'; ++i)
	{
	  buf[i] = (char)toupper(UChar(buf[i]));
	}

      for (i = 0; i < A_SIZE; i++)
	{
	  if (strcmp(request_names[i], buf) == 0)
	    returnCode(MIN_MENU_COMMAND + (int)i);
	}
    }
  RETURN(E_NO_MATCH);
}

 
