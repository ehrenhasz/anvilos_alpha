 

 

 

#ifndef FORM_PRIV_H
#define FORM_PRIV_H 1
 
#include "curses.priv.h"

#define NCURSES_OPAQUE_FORM  0

#include "mf_common.h"

#if USE_WIDEC_SUPPORT
#if HAVE_WCTYPE_H
#include <wctype.h>
#endif

#ifndef MB_LEN_MAX
#define MB_LEN_MAX 8  
#endif

#define FIELD_CELL NCURSES_CH_T

#define NCURSES_FIELD_INTERNALS char** expanded; WINDOW *working;
#define NCURSES_FIELD_EXTENSION , (char **)0, (WINDOW *)0

#else

#define FIELD_CELL char

#define NCURSES_FIELD_EXTENSION  

#endif

#include "form.h"

	 
extern FORM_EXPORT_VAR(FORM *)      _nc_Default_Form;
extern FORM_EXPORT_VAR(FIELD *)     _nc_Default_Field;
extern FORM_EXPORT_VAR(FIELDTYPE *) _nc_Default_FieldType;

 
#define _OVLMODE         (0x04U)  
#define _WINDOW_MODIFIED (0x10U)  
#define _FCHECK_REQUIRED (0x20U)  

 
#define _CHANGED         (0x01U)  
#define _NEWTOP          (0x02U)  
#define _NEWPAGE         (0x04U)  
#define _MAY_GROW        (0x08U)  

 
#define _LINKED_TYPE     (0x01U)  
#define _HAS_ARGS        (0x02U)  
#define _HAS_CHOICE      (0x04U)  
#define _RESIDENT        (0x08U)  
#define _GENERIC         (0x10U)  

 
#define O_SELECTABLE (O_ACTIVE | O_VISIBLE)

 
#define Normalize_Form(form) \
  ((form) = (form != 0) ? (form) : _nc_Default_Form)

 
#define Normalize_Field(field) \
  ((field) = (field != 0) ? (field) : _nc_Default_Field)

#if NCURSES_SP_FUNCS
#define Get_Form_Screen(form) \
  ((form)->win ? _nc_screen_of((form->win)):CURRENT_SCREEN)
#else
#define Get_Form_Screen(form) CURRENT_SCREEN
#endif

 
#define Get_Form_Window(form) \
  ((form)->sub \
   ? (form)->sub \
   : ((form)->win \
      ? (form)->win \
      : StdScreen(Get_Form_Screen(form))))

 
#define Buffer_Length(field) ((field)->drows * (field)->dcols)

 
#define Total_Buffer_Size(field) \
   ( (size_t)(Buffer_Length(field) + 1) * (size_t)(1+(field)->nbuf) * sizeof(FIELD_CELL) )

 
#define Single_Line_Field(field) \
   (((field)->rows + (field)->nrow) == 1)

#define Field_Has_Option(f,o)      ((((unsigned)(f)->opts) & o) != 0)

 
#define Field_Is_Selectable(f)     (((unsigned)((f)->opts) & O_SELECTABLE)==O_SELECTABLE)
#define Field_Is_Not_Selectable(f) (((unsigned)((f)->opts) & O_SELECTABLE)!=O_SELECTABLE)

typedef struct typearg
  {
    struct typearg *left;
    struct typearg *right;
  }
TypeArgument;

 
#define FIRST_ACTIVE_MAGIC (-291056)

#define ALL_FORM_OPTS  (                \
			O_NL_OVERLOAD  |\
			O_BS_OVERLOAD   )

#define STD_FIELD_OPTS (Field_Options)( \
			O_VISIBLE |\
			O_ACTIVE  |\
			O_PUBLIC  |\
			O_EDIT    |\
			O_WRAP    |\
			O_BLANK   |\
			O_AUTOSKIP|\
			O_NULLOK  |\
			O_PASSOK  |\
			O_STATIC)

#define ALL_FIELD_OPTS (Field_Options)( \
			STD_FIELD_OPTS |\
			O_DYNAMIC_JUSTIFY |\
			O_NO_LEFT_STRIP |\
			O_EDGE_INSERT_STAY |\
			O_INPUT_LIMIT)

#define C_BLANK ' '
#define is_blank(c) ((c)==C_BLANK)

#define C_ZEROS '\0'

extern FORM_EXPORT(TypeArgument *) _nc_Make_Argument (const FIELDTYPE*, va_list*, int*);
extern FORM_EXPORT(TypeArgument *) _nc_Copy_Argument (const FIELDTYPE*, const TypeArgument*, int*);
extern FORM_EXPORT(void) _nc_Free_Argument (const FIELDTYPE*, TypeArgument*);
extern FORM_EXPORT(bool) _nc_Copy_Type (FIELD*, FIELD const *);
extern FORM_EXPORT(void) _nc_Free_Type (FIELD *);

extern FORM_EXPORT(int) _nc_Synchronize_Attributes (FIELD*);
extern FORM_EXPORT(int) _nc_Synchronize_Options (FIELD*, Field_Options);
extern FORM_EXPORT(int) _nc_Set_Form_Page (FORM*, int, FIELD*);
extern FORM_EXPORT(int) _nc_Refresh_Current_Field (FORM*);
extern FORM_EXPORT(FIELD *) _nc_First_Active_Field (FORM*);
extern FORM_EXPORT(bool) _nc_Internal_Validation (FORM*);
extern FORM_EXPORT(int) _nc_Set_Current_Field (FORM*, FIELD*);
extern FORM_EXPORT(int) _nc_Position_Form_Cursor (FORM*);
extern FORM_EXPORT(void) _nc_Unset_Current_Field(FORM *form);

#if NCURSES_INTEROP_FUNCS
extern FORM_EXPORT(FIELDTYPE *) _nc_TYPE_INTEGER(void);
extern FORM_EXPORT(FIELDTYPE *) _nc_TYPE_ALNUM(void);
extern FORM_EXPORT(FIELDTYPE *) _nc_TYPE_ALPHA(void);
extern FORM_EXPORT(FIELDTYPE *) _nc_TYPE_ENUM(void);
extern FORM_EXPORT(FIELDTYPE *) _nc_TYPE_NUMERIC(void);
extern FORM_EXPORT(FIELDTYPE *) _nc_TYPE_REGEXP(void);
extern FORM_EXPORT(FIELDTYPE *) _nc_TYPE_IPV4(void);

extern FORM_EXPORT(FIELDTYPE *)
_nc_generic_fieldtype(bool (*const field_check) (FORM*,
						 FIELD *,
						 const void *),
		      bool (*const char_check)  (int,
						 FORM*,
						 FIELD*,
						 const void *),
		      bool (*const next)(FORM*,FIELD*,const void*),
		      bool (*const prev)(FORM*,FIELD*,const void*),
		      void (*freecallback)(void*));
extern FORM_EXPORT(int) _nc_set_generic_fieldtype(FIELD*, FIELDTYPE*, int (*)(void**));
extern FORM_EXPORT(WINDOW*) _nc_form_cursor(const FORM* , int* , int* );

#define INIT_FT_FUNC(func) {func}
#else
#define INIT_FT_FUNC(func) func
#endif

extern FORM_EXPORT(void) _nc_get_fieldbuffer(FORM*, FIELD*, FIELD_CELL*);

#if USE_WIDEC_SUPPORT
extern FORM_EXPORT(wchar_t *) _nc_Widen_String(char *, int *);
#endif

#ifdef TRACE

#define returnField(code)	TRACE_RETURN1(code,field)
#define returnFieldPtr(code)	TRACE_RETURN1(code,field_ptr)
#define returnForm(code)	TRACE_RETURN1(code,form)
#define returnFieldType(code)	TRACE_RETURN1(code,field_type)
#define returnFormHook(code)	TRACE_RETURN1(code,form_hook)

extern FORM_EXPORT(FIELD **)	    _nc_retrace_field_ptr (FIELD **);
extern FORM_EXPORT(FIELD *)	    _nc_retrace_field (FIELD *);
extern FORM_EXPORT(FIELDTYPE *)  _nc_retrace_field_type (FIELDTYPE *);
extern FORM_EXPORT(FORM *)       _nc_retrace_form (FORM *);
extern FORM_EXPORT(Form_Hook)    _nc_retrace_form_hook (Form_Hook);

#else  

#define returnFieldPtr(code)	return code
#define returnFieldType(code)	return code
#define returnField(code)	return code
#define returnForm(code)	return code
#define returnFormHook(code)	return code

#endif  

 
#if USE_WIDEC_SUPPORT
#define Check_CTYPE_Field(result, buffer, width, ccheck) \
  while (*buffer && *buffer == ' ') \
    buffer++; \
  if (*buffer) \
    { \
      bool blank = FALSE; \
      int len; \
      int n; \
      wchar_t *list = _nc_Widen_String((char *)buffer, &len); \
      if (list != 0) \
	{ \
	  result = TRUE; \
	  for (n = 0; n < len; ++n) \
	    { \
	      if (blank) \
		{ \
		  if (list[n] != ' ') \
		    { \
		      result = FALSE; \
		      break; \
		    } \
		} \
	      else if (list[n] == ' ') \
		{ \
		  blank = TRUE; \
		  result = (n + 1 >= width); \
		} \
	      else if (!ccheck(list[n], NULL)) \
		{ \
		  result = FALSE; \
		  break; \
		} \
	    } \
	  free(list); \
	} \
    }
#else
#define Check_CTYPE_Field(result, buffer, width, ccheck) \
  while (*buffer && *buffer == ' ') \
    buffer++; \
  if (*buffer) \
    { \
      unsigned char *s = buffer; \
      int l = -1; \
      while (*buffer && ccheck(*buffer, NULL)) \
	buffer++; \
      l = (int)(buffer - s); \
      while (*buffer && *buffer == ' ') \
	buffer++; \
      result = ((*buffer || (l < width)) ? FALSE : TRUE); \
    }
#endif
 

#endif  
