 

 

 

#ifndef FORM_H
#define FORM_H
 

#include <curses.h>
#include <eti.h>

#ifdef __cplusplus
  extern "C" {
#endif

#if defined(BUILDING_FORM)
# define FORM_IMPEXP NCURSES_EXPORT_GENERAL_EXPORT
#else
# define FORM_IMPEXP NCURSES_EXPORT_GENERAL_IMPORT
#endif

#define FORM_WRAPPED_VAR(type,name) extern FORM_IMPEXP type NCURSES_PUBLIC_VAR(name)(void)

#define FORM_EXPORT(type) FORM_IMPEXP type NCURSES_API
#define FORM_EXPORT_VAR(type) FORM_IMPEXP type

#ifndef FORM_PRIV_H
typedef void *FIELD_CELL;
#endif

#ifndef NCURSES_FIELD_INTERNALS
#define NCURSES_FIELD_INTERNALS  
#endif

typedef int Form_Options;
typedef int Field_Options;

	 

typedef struct pagenode
#if !NCURSES_OPAQUE_FORM
{
  short pmin;		 
  short pmax;		 
  short smin;		 
  short smax;		 
}
#endif  
_PAGE;

	 

typedef struct fieldnode
#if 1			 
{
  unsigned short	status;		 
  short			rows;		 
  short			cols;		 
  short			frow;		 
  short			fcol;		 
  int			drows;		 
  int			dcols;		 
  int			maxgrow;	 
  int			nrow;		 
  short			nbuf;		 
  short			just;		 
  short			page;		 
  short			index;		 
  int			pad;		 
  chtype		fore;		 
  chtype		back;		 
  Field_Options		opts;		 
  struct fieldnode *	snext;		 
  struct fieldnode *	sprev;		 
  struct fieldnode *	link;		 
  struct formnode *	form;		 
  struct typenode *	type;		 
  void *		arg;		 
  FIELD_CELL *		buf;		 
  void *		usrptr;		 
   
  NCURSES_FIELD_INTERNALS
}
#endif  
FIELD;


	 

typedef struct formnode
#if 1			 
{
  unsigned short	status;	  	 
  short			rows;		 
  short			cols;		 
  int			currow;		 
  int			curcol;		 
  int			toprow;		 
  int			begincol;	 
  short			maxfield;	 
  short			maxpage;	 
  short			curpage;	 
  Form_Options		opts;		 
  WINDOW *		win;		 
  WINDOW *		sub;		 
  WINDOW *		w;		 
  FIELD **		field;		 
  FIELD *		current;	 
  _PAGE *		page;		 
  void *		usrptr;		 

  void			(*forminit)(struct formnode *);
  void			(*formterm)(struct formnode *);
  void			(*fieldinit)(struct formnode *);
  void			(*fieldterm)(struct formnode *);

}
#endif  
FORM;


	 

typedef struct typenode
#if !NCURSES_OPAQUE_FORM
{
  unsigned short	status;			 
  long			ref;			 
  struct typenode *	left;			 
  struct typenode *	right;			 

  void* (*makearg)(va_list *);			 
  void* (*copyarg)(const void *);		 
  void	(*freearg)(void *);			 

#if NCURSES_INTEROP_FUNCS
  union {
    bool (*ofcheck)(FIELD *,const void *);	 
    bool (*gfcheck)(FORM*,FIELD *,const void*);	 
  } fieldcheck;
  union {
    bool (*occheck)(int,const void *);		 
    bool (*gccheck)(int,FORM*,
		    FIELD*,const void*);         
  } charcheck;
  union {
    bool (*onext)(FIELD *,const void *);         
    bool (*gnext)(FORM*,FIELD*,const void*);     
  } enum_next;
  union {
    bool (*oprev)(FIELD *,const void *);	 
    bool (*gprev)(FORM*,FIELD*,const void*);     
  } enum_prev;
  void* (*genericarg)(void*);                    
#else
  bool	(*fcheck)(FIELD *,const void *);	 
  bool	(*ccheck)(int,const void *);		 

  bool	(*next)(FIELD *,const void *);		 
  bool	(*prev)(FIELD *,const void *);		 
#endif
}
#endif  
FIELDTYPE;

typedef void (*Form_Hook)(FORM *);

	 

 
#define NO_JUSTIFICATION	(0)
#define JUSTIFY_LEFT		(1)
#define JUSTIFY_CENTER		(2)
#define JUSTIFY_RIGHT		(3)

 
#define O_VISIBLE		(0x0001U)
#define O_ACTIVE		(0x0002U)
#define O_PUBLIC		(0x0004U)
#define O_EDIT			(0x0008U)
#define O_WRAP			(0x0010U)
#define O_BLANK			(0x0020U)
#define O_AUTOSKIP		(0x0040U)
#define O_NULLOK		(0x0080U)
#define O_PASSOK		(0x0100U)
#define O_STATIC		(0x0200U)
#define O_DYNAMIC_JUSTIFY	(0x0400U)	 
#define O_NO_LEFT_STRIP		(0x0800U)	 
#define O_EDGE_INSERT_STAY      (0x1000U)	 
#define O_INPUT_LIMIT           (0x2000U)	 

 
#define O_NL_OVERLOAD		(0x0001U)
#define O_BS_OVERLOAD		(0x0002U)

 
#define REQ_NEXT_PAGE	 (KEY_MAX + 1)	 
#define REQ_PREV_PAGE	 (KEY_MAX + 2)	 
#define REQ_FIRST_PAGE	 (KEY_MAX + 3)	 
#define REQ_LAST_PAGE	 (KEY_MAX + 4)	 

#define REQ_NEXT_FIELD	 (KEY_MAX + 5)	 
#define REQ_PREV_FIELD	 (KEY_MAX + 6)	 
#define REQ_FIRST_FIELD	 (KEY_MAX + 7)	 
#define REQ_LAST_FIELD	 (KEY_MAX + 8)	 
#define REQ_SNEXT_FIELD	 (KEY_MAX + 9)	 
#define REQ_SPREV_FIELD	 (KEY_MAX + 10)	 
#define REQ_SFIRST_FIELD (KEY_MAX + 11)	 
#define REQ_SLAST_FIELD	 (KEY_MAX + 12)	 
#define REQ_LEFT_FIELD	 (KEY_MAX + 13)	 
#define REQ_RIGHT_FIELD	 (KEY_MAX + 14)	 
#define REQ_UP_FIELD	 (KEY_MAX + 15)	 
#define REQ_DOWN_FIELD	 (KEY_MAX + 16)	 

#define REQ_NEXT_CHAR	 (KEY_MAX + 17)	 
#define REQ_PREV_CHAR	 (KEY_MAX + 18)	 
#define REQ_NEXT_LINE	 (KEY_MAX + 19)	 
#define REQ_PREV_LINE	 (KEY_MAX + 20)	 
#define REQ_NEXT_WORD	 (KEY_MAX + 21)	 
#define REQ_PREV_WORD	 (KEY_MAX + 22)	 
#define REQ_BEG_FIELD	 (KEY_MAX + 23)	 
#define REQ_END_FIELD	 (KEY_MAX + 24)	 
#define REQ_BEG_LINE	 (KEY_MAX + 25)	 
#define REQ_END_LINE	 (KEY_MAX + 26)	 
#define REQ_LEFT_CHAR	 (KEY_MAX + 27)	 
#define REQ_RIGHT_CHAR	 (KEY_MAX + 28)	 
#define REQ_UP_CHAR	 (KEY_MAX + 29)	 
#define REQ_DOWN_CHAR	 (KEY_MAX + 30)	 

#define REQ_NEW_LINE	 (KEY_MAX + 31)	 
#define REQ_INS_CHAR	 (KEY_MAX + 32)	 
#define REQ_INS_LINE	 (KEY_MAX + 33)	 
#define REQ_DEL_CHAR	 (KEY_MAX + 34)	 
#define REQ_DEL_PREV	 (KEY_MAX + 35)	 
#define REQ_DEL_LINE	 (KEY_MAX + 36)	 
#define REQ_DEL_WORD	 (KEY_MAX + 37)	 
#define REQ_CLR_EOL	 (KEY_MAX + 38)	 
#define REQ_CLR_EOF	 (KEY_MAX + 39)	 
#define REQ_CLR_FIELD	 (KEY_MAX + 40)	 
#define REQ_OVL_MODE	 (KEY_MAX + 41)	 
#define REQ_INS_MODE	 (KEY_MAX + 42)	 
#define REQ_SCR_FLINE	 (KEY_MAX + 43)	 
#define REQ_SCR_BLINE	 (KEY_MAX + 44)	 
#define REQ_SCR_FPAGE	 (KEY_MAX + 45)	 
#define REQ_SCR_BPAGE	 (KEY_MAX + 46)	 
#define REQ_SCR_FHPAGE	 (KEY_MAX + 47)  
#define REQ_SCR_BHPAGE	 (KEY_MAX + 48)  
#define REQ_SCR_FCHAR	 (KEY_MAX + 49)  
#define REQ_SCR_BCHAR	 (KEY_MAX + 50)  
#define REQ_SCR_HFLINE	 (KEY_MAX + 51)  
#define REQ_SCR_HBLINE	 (KEY_MAX + 52)  
#define REQ_SCR_HFHALF	 (KEY_MAX + 53)  
#define REQ_SCR_HBHALF	 (KEY_MAX + 54)  

#define REQ_VALIDATION	 (KEY_MAX + 55)	 
#define REQ_NEXT_CHOICE	 (KEY_MAX + 56)	 
#define REQ_PREV_CHOICE	 (KEY_MAX + 57)	 

#define MIN_FORM_COMMAND (KEY_MAX + 1)	 
#define MAX_FORM_COMMAND (KEY_MAX + 57)	 

#if defined(MAX_COMMAND)
#  if (MAX_FORM_COMMAND > MAX_COMMAND)
#    error Something is wrong -- MAX_FORM_COMMAND is greater than MAX_COMMAND
#  elif (MAX_COMMAND != (KEY_MAX + 128))
#    error Something is wrong -- MAX_COMMAND is already inconsistently defined.
#  endif
#else
#  define MAX_COMMAND (KEY_MAX + 128)
#endif

	 
extern FORM_EXPORT_VAR(FIELDTYPE *) TYPE_ALPHA;
extern FORM_EXPORT_VAR(FIELDTYPE *) TYPE_ALNUM;
extern FORM_EXPORT_VAR(FIELDTYPE *) TYPE_ENUM;
extern FORM_EXPORT_VAR(FIELDTYPE *) TYPE_INTEGER;
extern FORM_EXPORT_VAR(FIELDTYPE *) TYPE_NUMERIC;
extern FORM_EXPORT_VAR(FIELDTYPE *) TYPE_REGEXP;

	 
extern FORM_EXPORT_VAR(FIELDTYPE *) TYPE_IPV4;       

	 
extern FORM_EXPORT(FIELDTYPE *) new_fieldtype (
		    bool (* const field_check)(FIELD *,const void *),
		    bool (* const char_check)(int,const void *));
extern FORM_EXPORT(FIELDTYPE *) link_fieldtype(
		    FIELDTYPE *, FIELDTYPE *);

extern FORM_EXPORT(int)	free_fieldtype (FIELDTYPE *);
extern FORM_EXPORT(int)	set_fieldtype_arg (FIELDTYPE *,
		    void * (* const make_arg)(va_list *),
		    void * (* const copy_arg)(const void *),
		    void (* const free_arg)(void *));
extern FORM_EXPORT(int)	 set_fieldtype_choice (FIELDTYPE *,
		    bool (* const next_choice)(FIELD *,const void *),
	      	    bool (* const prev_choice)(FIELD *,const void *));

	 
extern FORM_EXPORT(FIELD *)	new_field (int,int,int,int,int,int);
extern FORM_EXPORT(FIELD *)	dup_field (FIELD *,int,int);
extern FORM_EXPORT(FIELD *)	link_field (FIELD *,int,int);

extern FORM_EXPORT(int)	free_field (FIELD *);
extern FORM_EXPORT(int)	field_info (const FIELD *,int *,int *,int *,int *,int *,int *);
extern FORM_EXPORT(int)	dynamic_field_info (const FIELD *,int *,int *,int *);
extern FORM_EXPORT(int)	set_max_field ( FIELD *,int);
extern FORM_EXPORT(int)	move_field (FIELD *,int,int);
extern FORM_EXPORT(int)	set_field_type (FIELD *,FIELDTYPE *,...);
extern FORM_EXPORT(int)	set_new_page (FIELD *,bool);
extern FORM_EXPORT(int)	set_field_just (FIELD *,int);
extern FORM_EXPORT(int)	field_just (const FIELD *);
extern FORM_EXPORT(int)	set_field_fore (FIELD *,chtype);
extern FORM_EXPORT(int)	set_field_back (FIELD *,chtype);
extern FORM_EXPORT(int)	set_field_pad (FIELD *,int);
extern FORM_EXPORT(int)	field_pad (const FIELD *);
extern FORM_EXPORT(int)	set_field_buffer (FIELD *,int,const char *);
extern FORM_EXPORT(int)	set_field_status (FIELD *,bool);
extern FORM_EXPORT(int)	set_field_userptr (FIELD *, void *);
extern FORM_EXPORT(int)	set_field_opts (FIELD *,Field_Options);
extern FORM_EXPORT(int)	field_opts_on (FIELD *,Field_Options);
extern FORM_EXPORT(int)	field_opts_off (FIELD *,Field_Options);

extern FORM_EXPORT(chtype)	field_fore (const FIELD *);
extern FORM_EXPORT(chtype)	field_back (const FIELD *);

extern FORM_EXPORT(bool)	new_page (const FIELD *);
extern FORM_EXPORT(bool)	field_status (const FIELD *);

extern FORM_EXPORT(void *)	field_arg (const FIELD *);

extern FORM_EXPORT(void *)	field_userptr (const FIELD *);

extern FORM_EXPORT(FIELDTYPE *)	field_type (const FIELD *);

extern FORM_EXPORT(char *)	field_buffer (const FIELD *,int);

extern FORM_EXPORT(Field_Options)	field_opts (const FIELD *);

	 

extern FORM_EXPORT(FORM *)	new_form (FIELD **);

extern FORM_EXPORT(FIELD **)	form_fields (const FORM *);
extern FORM_EXPORT(FIELD *)	current_field (const FORM *);

extern FORM_EXPORT(WINDOW *)	form_win (const FORM *);
extern FORM_EXPORT(WINDOW *)	form_sub (const FORM *);

extern FORM_EXPORT(Form_Hook)	form_init (const FORM *);
extern FORM_EXPORT(Form_Hook)	form_term (const FORM *);
extern FORM_EXPORT(Form_Hook)	field_init (const FORM *);
extern FORM_EXPORT(Form_Hook)	field_term (const FORM *);

extern FORM_EXPORT(int)	free_form (FORM *);
extern FORM_EXPORT(int)	set_form_fields (FORM *,FIELD **);
extern FORM_EXPORT(int)	field_count (const FORM *);
extern FORM_EXPORT(int)	set_form_win (FORM *,WINDOW *);
extern FORM_EXPORT(int)	set_form_sub (FORM *,WINDOW *);
extern FORM_EXPORT(int)	set_current_field (FORM *,FIELD *);
extern FORM_EXPORT(int)	unfocus_current_field (FORM *);
extern FORM_EXPORT(int)	field_index (const FIELD *);
extern FORM_EXPORT(int)	set_form_page (FORM *,int);
extern FORM_EXPORT(int)	form_page (const FORM *);
extern FORM_EXPORT(int)	scale_form (const FORM *,int *,int *);
extern FORM_EXPORT(int)	set_form_init (FORM *,Form_Hook);
extern FORM_EXPORT(int)	set_form_term (FORM *,Form_Hook);
extern FORM_EXPORT(int)	set_field_init (FORM *,Form_Hook);
extern FORM_EXPORT(int)	set_field_term (FORM *,Form_Hook);
extern FORM_EXPORT(int)	post_form (FORM *);
extern FORM_EXPORT(int)	unpost_form (FORM *);
extern FORM_EXPORT(int)	pos_form_cursor (FORM *);
extern FORM_EXPORT(int)	form_driver (FORM *,int);
# if NCURSES_WIDECHAR
extern FORM_EXPORT(int)	form_driver_w (FORM *,int,wchar_t);
# endif
extern FORM_EXPORT(int)	set_form_userptr (FORM *,void *);
extern FORM_EXPORT(int)	set_form_opts (FORM *,Form_Options);
extern FORM_EXPORT(int)	form_opts_on (FORM *,Form_Options);
extern FORM_EXPORT(int)	form_opts_off (FORM *,Form_Options);
extern FORM_EXPORT(int)	form_request_by_name (const char *);

extern FORM_EXPORT(const char *)	form_request_name (int);

extern FORM_EXPORT(void *)	form_userptr (const FORM *);

extern FORM_EXPORT(Form_Options)	form_opts (const FORM *);

extern FORM_EXPORT(bool)	data_ahead (const FORM *);
extern FORM_EXPORT(bool)	data_behind (const FORM *);

#if NCURSES_SP_FUNCS
extern FORM_EXPORT(FORM *)	NCURSES_SP_NAME(new_form) (SCREEN*, FIELD **);
#endif

#ifdef __cplusplus
  }
#endif
 

#endif  
