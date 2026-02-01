 

 

 

#ifndef NC_PANEL_H
#define NC_PANEL_H 1

#include <ncurses_cfg.h>
#include <curses.h>

#ifdef __cplusplus
extern "C"
{
#endif

  struct panel;			 

  struct panelhook
    {
      struct panel *top_panel;
      struct panel *bottom_panel;
      struct panel *stdscr_pseudo_panel;
#if NO_LEAKS
      int (*destroy) (struct panel *);
#endif
    };

  struct screen;		 
 
  extern NCURSES_EXPORT(struct panelhook *)
    _nc_panelhook (void);
#if NCURSES_SP_FUNCS
  extern NCURSES_EXPORT(struct panelhook *)
    NCURSES_SP_NAME(_nc_panelhook) (SCREEN *);
#endif

#ifdef __cplusplus
}
#endif

#endif				 
