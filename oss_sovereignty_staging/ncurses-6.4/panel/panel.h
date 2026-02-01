 

 

 

 

#ifndef NCURSES_PANEL_H_incl
#define NCURSES_PANEL_H_incl 1

#include <curses.h>

typedef struct panel
#if !NCURSES_OPAQUE_PANEL
{
  WINDOW *win;
  struct panel *below;
  struct panel *above;
  NCURSES_CONST void *user;
}
#endif  
PANEL;

#if	defined(__cplusplus)
extern "C" {
#endif

#if defined(BUILDING_PANEL)
# define PANEL_IMPEXP NCURSES_EXPORT_GENERAL_EXPORT
#else
# define PANEL_IMPEXP NCURSES_EXPORT_GENERAL_IMPORT
#endif

#define PANEL_WRAPPED_VAR(type,name) extern PANEL_IMPEXP type NCURSES_PUBLIC_VAR(name)(void)

#define PANEL_EXPORT(type) PANEL_IMPEXP type NCURSES_API
#define PANEL_EXPORT_VAR(type) PANEL_IMPEXP type

extern PANEL_EXPORT(WINDOW*) panel_window (const PANEL *);
extern PANEL_EXPORT(void)    update_panels (void);
extern PANEL_EXPORT(int)     hide_panel (PANEL *);
extern PANEL_EXPORT(int)     show_panel (PANEL *);
extern PANEL_EXPORT(int)     del_panel (PANEL *);
extern PANEL_EXPORT(int)     top_panel (PANEL *);
extern PANEL_EXPORT(int)     bottom_panel (PANEL *);
extern PANEL_EXPORT(PANEL*)  new_panel (WINDOW *);
extern PANEL_EXPORT(PANEL*)  panel_above (const PANEL *);
extern PANEL_EXPORT(PANEL*)  panel_below (const PANEL *);
extern PANEL_EXPORT(int)     set_panel_userptr (PANEL *, NCURSES_CONST void *);
extern PANEL_EXPORT(NCURSES_CONST void*) panel_userptr (const PANEL *);
extern PANEL_EXPORT(int)     move_panel (PANEL *, int, int);
extern PANEL_EXPORT(int)     replace_panel (PANEL *,WINDOW *);
extern PANEL_EXPORT(int)     panel_hidden (const PANEL *);

#if NCURSES_SP_FUNCS
extern PANEL_EXPORT(PANEL *) ground_panel(SCREEN *);
extern PANEL_EXPORT(PANEL *) ceiling_panel(SCREEN *);

extern PANEL_EXPORT(void)    NCURSES_SP_NAME(update_panels) (SCREEN*);
#endif

#if	defined(__cplusplus)
}
#endif

#endif  

 
