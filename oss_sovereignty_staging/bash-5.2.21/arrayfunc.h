 

 

#if !defined (_ARRAYFUNC_H_)
#define _ARRAYFUNC_H_

 

 

 
#define ARRAY_INVALID	-1
#define ARRAY_SCALAR	0
#define ARRAY_INDEXED	1
#define ARRAY_ASSOC	2

 
typedef struct element_state
{
  short type;			   
  short subtype;		 
  arrayind_t ind;
  char *key;			 
  char *value;
} array_eltstate_t;

#if defined (ARRAY_VARS)

 
extern int assoc_expand_once;

 
extern int array_expand_once;

 
#define AV_ALLOWALL	0x001	 
#define AV_QUOTED	0x002
#define AV_USEIND	0x004
#define AV_USEVAL	0x008	 
#define AV_ASSIGNRHS	0x010	 
#define AV_NOEXPAND	0x020	 
#define AV_ONEWORD	0x040	 
#define AV_ATSTARKEYS	0x080	 

 
#define VA_NOEXPAND	0x001
#define VA_ONEWORD	0x002
#define VA_ALLOWALL	0x004	 

extern SHELL_VAR *convert_var_to_array PARAMS((SHELL_VAR *));
extern SHELL_VAR *convert_var_to_assoc PARAMS((SHELL_VAR *));

extern char *make_array_variable_value PARAMS((SHELL_VAR *, arrayind_t, char *, char *, int));

extern SHELL_VAR *bind_array_variable PARAMS((char *, arrayind_t, char *, int));
extern SHELL_VAR *bind_array_element PARAMS((SHELL_VAR *, arrayind_t, char *, int));
extern SHELL_VAR *assign_array_element PARAMS((char *, char *, int, array_eltstate_t *));

extern SHELL_VAR *bind_assoc_variable PARAMS((SHELL_VAR *, char *, char *, char *, int));

extern SHELL_VAR *find_or_make_array_variable PARAMS((char *, int));

extern SHELL_VAR *assign_array_from_string  PARAMS((char *, char *, int));
extern SHELL_VAR *assign_array_var_from_word_list PARAMS((SHELL_VAR *, WORD_LIST *, int));

extern WORD_LIST *expand_compound_array_assignment PARAMS((SHELL_VAR *, char *, int));
extern void assign_compound_array_list PARAMS((SHELL_VAR *, WORD_LIST *, int));
extern SHELL_VAR *assign_array_var_from_string PARAMS((SHELL_VAR *, char *, int));

extern char *expand_and_quote_assoc_word PARAMS((char *, int));
extern void quote_compound_array_list PARAMS((WORD_LIST *, int));

extern int kvpair_assignment_p PARAMS((WORD_LIST *));
extern char *expand_and_quote_kvpair_word PARAMS((char *));

extern int unbind_array_element PARAMS((SHELL_VAR *, char *, int));
extern int skipsubscript PARAMS((const char *, int, int));

extern void print_array_assignment PARAMS((SHELL_VAR *, int));
extern void print_assoc_assignment PARAMS((SHELL_VAR *, int));

extern arrayind_t array_expand_index PARAMS((SHELL_VAR *, char *, int, int));
extern int valid_array_reference PARAMS((const char *, int));
extern int tokenize_array_reference PARAMS((char *, int, char **));

extern char *array_value PARAMS((const char *, int, int, array_eltstate_t *));
extern char *get_array_value PARAMS((const char *, int, array_eltstate_t *));

extern char *array_keys PARAMS((char *, int, int));

extern char *array_variable_name PARAMS((const char *, int, char **, int *));
extern SHELL_VAR *array_variable_part PARAMS((const char *, int, char **, int *));

extern void init_eltstate (array_eltstate_t *);
extern void flush_eltstate (array_eltstate_t *);

#else

#define AV_ALLOWALL	0
#define AV_QUOTED	0
#define AV_USEIND	0
#define AV_USEVAL	0
#define AV_ASSIGNRHS	0
#define AV_NOEXPAND	0
#define AV_ONEWORD	0
#define AV_ATSTARKEYS	0

#define VA_NOEXPAND	0
#define VA_ONEWORD	0
#define VA_ALLOWALL	0

#endif

#endif  
