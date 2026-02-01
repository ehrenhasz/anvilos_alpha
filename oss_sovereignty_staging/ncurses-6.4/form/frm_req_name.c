 

 

 

#include "form.priv.h"

MODULE_ID("$Id: frm_req_name.c,v 1.23 2021/06/17 21:11:08 tom Exp $")

#define DATA(s) { s }

static const char request_names[MAX_FORM_COMMAND - MIN_FORM_COMMAND + 1][13] =
{
  DATA("NEXT_PAGE"),
  DATA("PREV_PAGE"),
  DATA("FIRST_PAGE"),
  DATA("LAST_PAGE"),

  DATA("NEXT_FIELD"),
  DATA("PREV_FIELD"),
  DATA("FIRST_FIELD"),
  DATA("LAST_FIELD"),
  DATA("SNEXT_FIELD"),
  DATA("SPREV_FIELD"),
  DATA("SFIRST_FIELD"),
  DATA("SLAST_FIELD"),
  DATA("LEFT_FIELD"),
  DATA("RIGHT_FIELD"),
  DATA("UP_FIELD"),
  DATA("DOWN_FIELD"),

  DATA("NEXT_CHAR"),
  DATA("PREV_CHAR"),
  DATA("NEXT_LINE"),
  DATA("PREV_LINE"),
  DATA("NEXT_WORD"),
  DATA("PREV_WORD"),
  DATA("BEG_FIELD"),
  DATA("END_FIELD"),
  DATA("BEG_LINE"),
  DATA("END_LINE"),
  DATA("LEFT_CHAR"),
  DATA("RIGHT_CHAR"),
  DATA("UP_CHAR"),
  DATA("DOWN_CHAR"),

  DATA("NEW_LINE"),
  DATA("INS_CHAR"),
  DATA("INS_LINE"),
  DATA("DEL_CHAR"),
  DATA("DEL_PREV"),
  DATA("DEL_LINE"),
  DATA("DEL_WORD"),
  DATA("CLR_EOL"),
  DATA("CLR_EOF"),
  DATA("CLR_FIELD"),
  DATA("OVL_MODE"),
  DATA("INS_MODE"),
  DATA("SCR_FLINE"),
  DATA("SCR_BLINE"),
  DATA("SCR_FPAGE"),
  DATA("SCR_BPAGE"),
  DATA("SCR_FHPAGE"),
  DATA("SCR_BHPAGE"),
  DATA("SCR_FCHAR"),
  DATA("SCR_BCHAR"),
  DATA("SCR_HFLINE"),
  DATA("SCR_HBLINE"),
  DATA("SCR_HFHALF"),
  DATA("SCR_HBHALF"),

  DATA("VALIDATION"),
  DATA("NEXT_CHOICE"),
  DATA("PREV_CHOICE")
};

#undef DATA

#define A_SIZE (sizeof(request_names)/sizeof(request_names[0]))

 
FORM_EXPORT(const char *)
form_request_name(int request)
{
  T((T_CALLED("form_request_name(%d)"), request));

  if ((request < MIN_FORM_COMMAND) || (request > MAX_FORM_COMMAND))
    {
      SET_ERROR(E_BAD_ARGUMENT);
      returnCPtr((const char *)0);
    }
  else
    returnCPtr(request_names[request - MIN_FORM_COMMAND]);
}

 
FORM_EXPORT(int)
form_request_by_name(const char *str)
{
   
  size_t i = 0;

  T((T_CALLED("form_request_by_name(%s)"), _nc_visbuf(str)));

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
	    returnCode(MIN_FORM_COMMAND + (int)i);
	}
    }
  RETURN(E_NO_MATCH);
}

 
