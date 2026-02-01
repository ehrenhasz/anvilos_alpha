 

 

 

#ifndef ETI_MENU
#define ETI_MENU

#ifdef AMIGA
#define TEXT TEXT_ncurses
#endif

#include <curses.h>
#include <eti.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(BUILDING_MENU)
# define MENU_IMPEXP NCURSES_EXPORT_GENERAL_EXPORT
#else
# define MENU_IMPEXP NCURSES_EXPORT_GENERAL_IMPORT
#endif

#define MENU_WRAPPED_VAR(type,name) extern MENU_IMPEXP type NCURSES_PUBLIC_VAR(name)(void)

#define MENU_EXPORT(type) MENU_IMPEXP type NCURSES_API
#define MENU_EXPORT_VAR(type) MENU_IMPEXP type

  typedef int Menu_Options;
  typedef int Item_Options;

 
#define O_ONEVALUE      (0x01)
#define O_SHOWDESC      (0x02)
#define O_ROWMAJOR      (0x04)
#define O_IGNORECASE    (0x08)
#define O_SHOWMATCH     (0x10)
#define O_NONCYCLIC     (0x20)
#define O_MOUSE_MENU    (0x40)

 
#define O_SELECTABLE    (0x01)

#if !NCURSES_OPAQUE_MENU
  typedef struct
    {
      const char *str;
      unsigned short length;
    }
  TEXT;
#endif				 

  struct tagMENU;

  typedef struct tagITEM
#if !NCURSES_OPAQUE_MENU
    {
      TEXT name;		 
      TEXT description;		 
      struct tagMENU *imenu;	 
      void *userptr;		 
      Item_Options opt;		 
      short index;		 
      short y;			 
      short x;
      bool value;		 

      struct tagITEM *left;	 
      struct tagITEM *right;
      struct tagITEM *up;
      struct tagITEM *down;

    }
#endif				 
  ITEM;

  typedef void (*Menu_Hook) (struct tagMENU *);

  typedef struct tagMENU
#if 1				 
    {
      short height;		 
      short width;		 
      short rows;		 
      short cols;		 
      short frows;		 
      short fcols;		 
      short arows;		 
      short namelen;		 
      short desclen;		 
      short marklen;		 
      short itemlen;		 
      short spc_desc;		 
      short spc_cols;		 
      short spc_rows;		 
      char *pattern;		 
      short pindex;		 
      WINDOW *win;		 
      WINDOW *sub;		 
      WINDOW *userwin;		 
      WINDOW *usersub;		 
      ITEM **items;		 
      short nitems;		 
      ITEM *curitem;		 
      short toprow;		 
      chtype fore;		 
      chtype back;		 
      chtype grey;		 
      unsigned char pad;	 

      Menu_Hook menuinit;	 
      Menu_Hook menuterm;
      Menu_Hook iteminit;
      Menu_Hook itemterm;

      void *userptr;		 
      char *mark;		 

      Menu_Options opt;		 
      unsigned short status;	 
    }
#endif				 
  MENU;

 

#define REQ_LEFT_ITEM           (KEY_MAX + 1)
#define REQ_RIGHT_ITEM          (KEY_MAX + 2)
#define REQ_UP_ITEM             (KEY_MAX + 3)
#define REQ_DOWN_ITEM           (KEY_MAX + 4)
#define REQ_SCR_ULINE           (KEY_MAX + 5)
#define REQ_SCR_DLINE           (KEY_MAX + 6)
#define REQ_SCR_DPAGE           (KEY_MAX + 7)
#define REQ_SCR_UPAGE           (KEY_MAX + 8)
#define REQ_FIRST_ITEM          (KEY_MAX + 9)
#define REQ_LAST_ITEM           (KEY_MAX + 10)
#define REQ_NEXT_ITEM           (KEY_MAX + 11)
#define REQ_PREV_ITEM           (KEY_MAX + 12)
#define REQ_TOGGLE_ITEM         (KEY_MAX + 13)
#define REQ_CLEAR_PATTERN       (KEY_MAX + 14)
#define REQ_BACK_PATTERN        (KEY_MAX + 15)
#define REQ_NEXT_MATCH          (KEY_MAX + 16)
#define REQ_PREV_MATCH          (KEY_MAX + 17)

#define MIN_MENU_COMMAND        (KEY_MAX + 1)
#define MAX_MENU_COMMAND        (KEY_MAX + 17)

 
#if defined(MAX_COMMAND)
#  if (MAX_MENU_COMMAND > MAX_COMMAND)
#    error Something is wrong -- MAX_MENU_COMMAND is greater than MAX_COMMAND
#  elif (MAX_COMMAND != (KEY_MAX + 128))
#    error Something is wrong -- MAX_COMMAND is already inconsistently defined.
#  endif
#else
#  define MAX_COMMAND (KEY_MAX + 128)
#endif

 

  extern MENU_EXPORT(ITEM **) menu_items(const MENU *);
  extern MENU_EXPORT(ITEM *) current_item(const MENU *);
  extern MENU_EXPORT(ITEM *) new_item(const char *, const char *);

  extern MENU_EXPORT(MENU *) new_menu(ITEM **);

  extern MENU_EXPORT(Item_Options) item_opts(const ITEM *);
  extern MENU_EXPORT(Menu_Options) menu_opts(const MENU *);

  extern MENU_EXPORT(Menu_Hook) item_init(const MENU *);
  extern MENU_EXPORT(Menu_Hook) item_term(const MENU *);
  extern MENU_EXPORT(Menu_Hook) menu_init(const MENU *);
  extern MENU_EXPORT(Menu_Hook) menu_term(const MENU *);

  extern MENU_EXPORT(WINDOW *) menu_sub(const MENU *);
  extern MENU_EXPORT(WINDOW *) menu_win(const MENU *);

  extern MENU_EXPORT(const char *) item_description(const ITEM *);
  extern MENU_EXPORT(const char *) item_name(const ITEM *);
  extern MENU_EXPORT(const char *) menu_mark(const MENU *);
  extern MENU_EXPORT(const char *) menu_request_name(int);

  extern MENU_EXPORT(char *) menu_pattern(const MENU *);

  extern MENU_EXPORT(void *) menu_userptr(const MENU *);
  extern MENU_EXPORT(void *) item_userptr(const ITEM *);

  extern MENU_EXPORT(chtype) menu_back(const MENU *);
  extern MENU_EXPORT(chtype) menu_fore(const MENU *);
  extern MENU_EXPORT(chtype) menu_grey(const MENU *);

  extern MENU_EXPORT(int) free_item(ITEM *);
  extern MENU_EXPORT(int) free_menu(MENU *);
  extern MENU_EXPORT(int) item_count(const MENU *);
  extern MENU_EXPORT(int) item_index(const ITEM *);
  extern MENU_EXPORT(int) item_opts_off(ITEM *, Item_Options);
  extern MENU_EXPORT(int) item_opts_on(ITEM *, Item_Options);
  extern MENU_EXPORT(int) menu_driver(MENU *, int);
  extern MENU_EXPORT(int) menu_opts_off(MENU *, Menu_Options);
  extern MENU_EXPORT(int) menu_opts_on(MENU *, Menu_Options);
  extern MENU_EXPORT(int) menu_pad(const MENU *);
  extern MENU_EXPORT(int) pos_menu_cursor(const MENU *);
  extern MENU_EXPORT(int) post_menu(MENU *);
  extern MENU_EXPORT(int) scale_menu(const MENU *, int *, int *);
  extern MENU_EXPORT(int) set_current_item(MENU *menu, ITEM *item);
  extern MENU_EXPORT(int) set_item_init(MENU *, Menu_Hook);
  extern MENU_EXPORT(int) set_item_opts(ITEM *, Item_Options);
  extern MENU_EXPORT(int) set_item_term(MENU *, Menu_Hook);
  extern MENU_EXPORT(int) set_item_userptr(ITEM *, void *);
  extern MENU_EXPORT(int) set_item_value(ITEM *, bool);
  extern MENU_EXPORT(int) set_menu_back(MENU *, chtype);
  extern MENU_EXPORT(int) set_menu_fore(MENU *, chtype);
  extern MENU_EXPORT(int) set_menu_format(MENU *, int, int);
  extern MENU_EXPORT(int) set_menu_grey(MENU *, chtype);
  extern MENU_EXPORT(int) set_menu_init(MENU *, Menu_Hook);
  extern MENU_EXPORT(int) set_menu_items(MENU *, ITEM **);
  extern MENU_EXPORT(int) set_menu_mark(MENU *, const char *);
  extern MENU_EXPORT(int) set_menu_opts(MENU *, Menu_Options);
  extern MENU_EXPORT(int) set_menu_pad(MENU *, int);
  extern MENU_EXPORT(int) set_menu_pattern(MENU *, const char *);
  extern MENU_EXPORT(int) set_menu_sub(MENU *, WINDOW *);
  extern MENU_EXPORT(int) set_menu_term(MENU *, Menu_Hook);
  extern MENU_EXPORT(int) set_menu_userptr(MENU *, void *);
  extern MENU_EXPORT(int) set_menu_win(MENU *, WINDOW *);
  extern MENU_EXPORT(int) set_top_row(MENU *, int);
  extern MENU_EXPORT(int) top_row(const MENU *);
  extern MENU_EXPORT(int) unpost_menu(MENU *);
  extern MENU_EXPORT(int) menu_request_by_name(const char *);
  extern MENU_EXPORT(int) set_menu_spacing(MENU *, int, int, int);
  extern MENU_EXPORT(int) menu_spacing(const MENU *, int *, int *, int *);

  extern MENU_EXPORT(bool) item_value(const ITEM *);
  extern MENU_EXPORT(bool) item_visible(const ITEM *);

  extern MENU_EXPORT(void) menu_format(const MENU *, int *, int *);

#if NCURSES_SP_FUNCS
  extern MENU_EXPORT(MENU *) NCURSES_SP_NAME(new_menu) (SCREEN *, ITEM **);
#endif

#ifdef __cplusplus
}
#endif

#endif				 
