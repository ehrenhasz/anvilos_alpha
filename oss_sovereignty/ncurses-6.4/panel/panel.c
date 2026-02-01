 

 

 
#include "panel.priv.h"

MODULE_ID("$Id: panel.c,v 1.30 2020/09/26 18:05:17 tom Exp $")

 
#ifdef TRACE
PANEL_EXPORT(PANEL *)
_nc_retrace_panel(PANEL * pan)
{
  T((T_RETURN("%p"), (void *)pan));
  return pan;
}
#endif

 
#ifdef TRACE
#ifndef TRACE_TXT
PANEL_EXPORT(const char *)
_nc_my_visbuf(const void *ptr, int n)
{
  char temp[32];

  if (ptr != 0)
    _nc_SPRINTF(temp, _nc_SLIMIT(sizeof(temp)) "%p", ptr);
  else
    _nc_STRCPY(temp, "<null>", sizeof(temp));
  return _nc_visbuf2(n, temp);
}
#endif
#endif

 
#ifdef TRACE
PANEL_EXPORT(void)
_nc_dPanel(const char *text, const PANEL * pan)
{
  _tracef("%s id=%s b=%s a=%s y=%d x=%d",
	  text, USER_PTR(pan->user, 1),
	  (pan->below) ? USER_PTR(pan->below->user, 2) : "--",
	  (pan->above) ? USER_PTR(pan->above->user, 3) : "--",
	  PSTARTY(pan), PSTARTX(pan));
}
#endif

 
#ifdef TRACE
PANEL_EXPORT(void)
_nc_dStack(const char *fmt, int num, const PANEL * pan)
{
  char s80[80];

  GetPanelHook(pan);

  _nc_SPRINTF(s80, _nc_SLIMIT(sizeof(s80)) fmt, num, pan);
  _tracef("%s b=%s t=%s", s80,
	  (_nc_bottom_panel) ? USER_PTR(_nc_bottom_panel->user, 1) : "--",
	  (_nc_top_panel) ? USER_PTR(_nc_top_panel->user, 2) : "--");
  if (pan)
    _tracef("pan id=%s", USER_PTR(pan->user, 1));
  pan = _nc_bottom_panel;
  while (pan)
    {
      dPanel("stk", pan);
      pan = pan->above;
    }
}
#endif

 
#ifdef TRACE
PANEL_EXPORT(void)
_nc_Wnoutrefresh(const PANEL * pan)
{
  dPanel("wnoutrefresh", pan);
  wnoutrefresh(pan->win);
}
#endif

 
#ifdef TRACE
PANEL_EXPORT(void)
_nc_Touchpan(const PANEL * pan)
{
  dPanel("Touchpan", pan);
  touchwin(pan->win);
}
#endif

 
#ifdef TRACE
PANEL_EXPORT(void)
_nc_Touchline(const PANEL * pan, int start, int count)
{
  char s80[80];

  _nc_SPRINTF(s80, _nc_SLIMIT(sizeof(s80)) "Touchline s=%d c=%d", start, count);
  dPanel(s80, pan);
  touchline(pan->win, start, count);
}
#endif

#ifndef TRACE
#  ifndef __GNUC__
      
extern void _nc_dummy_panel(void);
void
_nc_dummy_panel(void)
{
}
#  endif
#endif
