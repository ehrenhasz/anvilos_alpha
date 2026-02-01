 

 

#ifndef NCURSES_PANEL_PRIV_H
#define NCURSES_PANEL_PRIV_H 1
 

#if HAVE_CONFIG_H
#  include <ncurses_cfg.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct screen;               

#include "curses.priv.h"     

#define NCURSES_OPAQUE_PANEL 0

#include "panel.h"

#ifdef TRACE
   extern PANEL_EXPORT(const char *) _nc_my_visbuf (const void *, int);
#  ifdef TRACE_TXT
#    define USER_PTR(ptr,n) _nc_visbuf2(n, (const char *)ptr)
#  else
#    define USER_PTR(ptr,n) _nc_my_visbuf((const char *)ptr, n)
#  endif

#  define returnPanel(code)	TRACE_RETURN1(code,panel)

   extern PANEL_EXPORT(PANEL *) _nc_retrace_panel (PANEL *);
   extern PANEL_EXPORT(void) _nc_dPanel (const char*, const PANEL*);
   extern PANEL_EXPORT(void) _nc_dStack (const char*, int, const PANEL*);
   extern PANEL_EXPORT(void) _nc_Wnoutrefresh (const PANEL*);
   extern PANEL_EXPORT(void) _nc_Touchpan (const PANEL*);
   extern PANEL_EXPORT(void) _nc_Touchline (const PANEL*, int, int);

#  define dBug(x) _tracef x
#  define dPanel(text,pan) _nc_dPanel(text,pan)
#  define dStack(fmt,num,pan) _nc_dStack(fmt,num,pan)
#  define Wnoutrefresh(pan) _nc_Wnoutrefresh(pan)
#  define Touchpan(pan) _nc_Touchpan(pan)
#  define Touchline(pan,start,count) _nc_Touchline(pan,start,count)
#else  
#  define returnPanel(code)	return code
#  define dBug(x)
#  define dPanel(text,pan)
#  define dStack(fmt,num,pan)
#  define Wnoutrefresh(pan) wnoutrefresh((pan)->win)
#  define Touchpan(pan) touchwin((pan)->win)
#  define Touchline(pan,start,count) touchline((pan)->win,start,count)
#endif

#if NCURSES_SP_FUNCS
#define GetScreenHook(sp) \
			struct panelhook* ph = NCURSES_SP_NAME(_nc_panelhook)(sp)
#define GetPanelHook(pan) \
			GetScreenHook(pan ? _nc_screen_of((pan)->win) : 0)
#define GetWindowHook(win) \
			SCREEN* sp = _nc_screen_of(win); \
			GetScreenHook(sp)
#define GetHook(pan)	SCREEN* sp = _nc_screen_of(pan->win); \
			GetScreenHook(sp)

#define _nc_stdscr_pseudo_panel ((ph)->stdscr_pseudo_panel)
#define _nc_top_panel           ((ph)->top_panel)
#define _nc_bottom_panel        ((ph)->bottom_panel)

#else	 

#define GetScreenHook(sp)  
#define GetPanelHook(pan)  
#define GetWindowHook(win)  
#define GetHook(pan)  

#define _nc_stdscr_pseudo_panel _nc_panelhook()->stdscr_pseudo_panel
#define _nc_top_panel           _nc_panelhook()->top_panel
#define _nc_bottom_panel        _nc_panelhook()->bottom_panel

#endif	 

#define EMPTY_STACK() (_nc_top_panel == _nc_bottom_panel)
#define Is_Bottom(p)  (((p) != (PANEL*)0) && !EMPTY_STACK() && (_nc_bottom_panel->above == (p)))
#define Is_Top(p)     (((p) != (PANEL*)0) && !EMPTY_STACK() && (_nc_top_panel == (p)))
#define Is_Pseudo(p)  (((p) != (PANEL*)0) && ((p) == _nc_bottom_panel))

 
 
#define IS_LINKED(p) (((p)->above || (p)->below ||((p)==_nc_bottom_panel)) ? TRUE : FALSE)

#define PSTARTX(pan) ((pan)->win->_begx)
#define PENDX(pan)   ((pan)->win->_begx + getmaxx((pan)->win) - 1)
#define PSTARTY(pan) ((pan)->win->_begy)
#define PENDY(pan)   ((pan)->win->_begy + getmaxy((pan)->win) - 1)

 
#define PANELS_OVERLAPPED(pan1,pan2) \
(( !(pan1) || !(pan2) || \
       PSTARTY(pan1) > PENDY(pan2) || PENDY(pan1) < PSTARTY(pan2) ||\
       PSTARTX(pan1) > PENDX(pan2) || PENDX(pan1) < PSTARTX(pan2) ) \
     ? FALSE : TRUE)


 
#define COMPUTE_INTERSECTION(pan1,pan2,ix1,ix2,iy1,iy2)\
   ix1 = (PSTARTX(pan1) < PSTARTX(pan2)) ? PSTARTX(pan2) : PSTARTX(pan1);\
   ix2 = (PENDX(pan1)   < PENDX(pan2))   ? PENDX(pan1)   : PENDX(pan2);\
   iy1 = (PSTARTY(pan1) < PSTARTY(pan2)) ? PSTARTY(pan2) : PSTARTY(pan1);\
   iy2 = (PENDY(pan1)   < PENDY(pan2))   ? PENDY(pan1)   : PENDY(pan2);\
   assert((ix1<=ix2) && (iy1<=iy2))


 
#define PANEL_UPDATE(pan,panstart)\
{  PANEL* pan2 = ((panstart) ? (panstart) : _nc_bottom_panel);\
   while(pan2 && pan2->win) {\
      if ((pan2 != pan) && PANELS_OVERLAPPED(pan,pan2)) {\
        int y, ix1, ix2, iy1, iy2;\
        COMPUTE_INTERSECTION(pan, pan2, ix1, ix2, iy1, iy2);\
	for(y = iy1; y <= iy2; y++) {\
	  if (is_linetouched(pan->win,y - PSTARTY(pan))) {\
            struct ldat* line = &(pan2->win->_line[y - PSTARTY(pan2)]);\
            CHANGED_RANGE(line, ix1 - PSTARTX(pan2), ix2 - PSTARTX(pan2));\
          }\
	}\
      }\
      pan2 = pan2->above;\
   }\
}

 
#define PANEL_UNLINK(pan,err) \
{  err = ERR;\
   if (pan) {\
     if (IS_LINKED(pan)) {\
       if ((pan)->below)\
         (pan)->below->above = (pan)->above;\
       if ((pan)->above)\
         (pan)->above->below = (pan)->below;\
       if ((pan) == _nc_bottom_panel) \
         _nc_bottom_panel = (pan)->above;\
       if ((pan) == _nc_top_panel) \
         _nc_top_panel = (pan)->below;\
       err = OK;\
     }\
     (pan)->above = (pan)->below = (PANEL*)0;\
   }\
}

#define HIDE_PANEL(pan,err,err_if_unlinked)\
  if (IS_LINKED(pan)) {\
    Touchpan(pan);\
    PANEL_UPDATE(pan,(PANEL*)0);\
    PANEL_UNLINK(pan,err);\
  } \
  else {\
      err = err_if_unlinked;\
  }

#if NCURSES_SP_FUNCS
 
extern PANEL_EXPORT(void) NCURSES_SP_NAME(_nc_update_panels)(SCREEN*);
#endif
 

#endif  
