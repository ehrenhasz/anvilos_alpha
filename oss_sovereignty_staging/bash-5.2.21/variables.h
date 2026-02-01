 

 

#if !defined (_VARIABLES_H_)
#define _VARIABLES_H_

#include "stdc.h"
#include "array.h"
#include "assoc.h"

 
#include "hashlib.h"

#include "conftypes.h"

 
typedef struct var_context {
  char *name;		 
  int scope;		 
  int flags;
  struct var_context *up;	 
  struct var_context *down;	 
  HASH_TABLE *table;		 
} VAR_CONTEXT;

 
#define VC_HASLOCAL	0x01
#define VC_HASTMPVAR	0x02
#define VC_FUNCENV	0x04	 
#define VC_BLTNENV	0x08	 
#define VC_TEMPENV	0x10	 

#define VC_TEMPFLAGS	(VC_FUNCENV|VC_BLTNENV|VC_TEMPENV)

 
#define vc_isfuncenv(vc)	(((vc)->flags & VC_FUNCENV) != 0)
#define vc_isbltnenv(vc)	(((vc)->flags & VC_BLTNENV) != 0)
#define vc_istempenv(vc)	(((vc)->flags & (VC_TEMPFLAGS)) == VC_TEMPENV)

#define vc_istempscope(vc)	(((vc)->flags & (VC_TEMPENV|VC_BLTNENV)) != 0)

#define vc_haslocals(vc)	(((vc)->flags & VC_HASLOCAL) != 0)
#define vc_hastmpvars(vc)	(((vc)->flags & VC_HASTMPVAR) != 0)

 

typedef struct variable *sh_var_value_func_t PARAMS((struct variable *));
typedef struct variable *sh_var_assign_func_t PARAMS((struct variable *, char *, arrayind_t, char *));

 
union _value {
  char *s;			 
  intmax_t i;			 
  COMMAND *f;			 
  ARRAY *a;			 
  HASH_TABLE *h;		 
  double d;			 
#if defined (HAVE_LONG_DOUBLE)
  long double ld;		 
#endif
  struct variable *v;		 
  void *opaque;			 
};

typedef struct variable {
  char *name;			 
  char *value;			 
  char *exportstr;		 
  sh_var_value_func_t *dynamic_value;	 
  sh_var_assign_func_t *assign_func;  
  int attributes;		 
  int context;			 
} SHELL_VAR;

typedef struct _vlist {
  SHELL_VAR **list;
  int list_size;	 
  int list_len;		 
} VARLIST;

 
 
#define att_exported	0x0000001	 
#define att_readonly	0x0000002	 
#define att_array	0x0000004	 
#define att_function	0x0000008	 
#define att_integer	0x0000010	 
#define att_local	0x0000020	 
#define att_assoc	0x0000040	 
#define att_trace	0x0000080	 
#define att_uppercase	0x0000100	 
#define att_lowercase	0x0000200	 
#define att_capcase	0x0000400	 
#define att_nameref	0x0000800	 

#define user_attrs	(att_exported|att_readonly|att_integer|att_local|att_trace|att_uppercase|att_lowercase|att_capcase|att_nameref)

#define attmask_user	0x0000fff

 
#define att_invisible	0x0001000	 
#define att_nounset	0x0002000	 
#define att_noassign	0x0004000	 
#define att_imported	0x0008000	 
#define att_special	0x0010000	 
#define att_nofree	0x0020000	 
#define att_regenerate	0x0040000	 

#define	attmask_int	0x00ff000

 
#define att_tempvar	0x0100000	 
#define att_propagate	0x0200000	 

#define attmask_scope	0x0f00000

#define exported_p(var)		((((var)->attributes) & (att_exported)))
#define readonly_p(var)		((((var)->attributes) & (att_readonly)))
#define array_p(var)		((((var)->attributes) & (att_array)))
#define function_p(var)		((((var)->attributes) & (att_function)))
#define integer_p(var)		((((var)->attributes) & (att_integer)))
#define local_p(var)		((((var)->attributes) & (att_local)))
#define assoc_p(var)		((((var)->attributes) & (att_assoc)))
#define trace_p(var)		((((var)->attributes) & (att_trace)))
#define uppercase_p(var)	((((var)->attributes) & (att_uppercase)))
#define lowercase_p(var)	((((var)->attributes) & (att_lowercase)))
#define capcase_p(var)		((((var)->attributes) & (att_capcase)))
#define nameref_p(var)		((((var)->attributes) & (att_nameref)))

#define invisible_p(var)	((((var)->attributes) & (att_invisible)))
#define non_unsettable_p(var)	((((var)->attributes) & (att_nounset)))
#define noassign_p(var)		((((var)->attributes) & (att_noassign)))
#define imported_p(var)		((((var)->attributes) & (att_imported)))
#define specialvar_p(var)	((((var)->attributes) & (att_special)))
#define nofree_p(var)		((((var)->attributes) & (att_nofree)))
#define regen_p(var)		((((var)->attributes) & (att_regenerate)))

#define tempvar_p(var)		((((var)->attributes) & (att_tempvar)))
#define propagate_p(var)	((((var)->attributes) & (att_propagate)))

 
#define name_cell(var)		((var)->name)

 
#define value_cell(var)		((var)->value)
#define function_cell(var)	(COMMAND *)((var)->value)
#define array_cell(var)		(ARRAY *)((var)->value)
#define assoc_cell(var)		(HASH_TABLE *)((var)->value)
#define nameref_cell(var)	((var)->value)		 

#define NAMEREF_MAX	8	 

#define var_isset(var)		((var)->value != 0)
#define var_isunset(var)	((var)->value == 0)
#define var_isnull(var)		((var)->value && *(var)->value == 0)

 
#define var_setvalue(var, str)	((var)->value = (str))
#define var_setfunc(var, func)	((var)->value = (char *)(func))
#define var_setarray(var, arr)	((var)->value = (char *)(arr))
#define var_setassoc(var, arr)	((var)->value = (char *)(arr))
#define var_setref(var, str)	((var)->value = (str))

 
#define set_auto_export(var) \
  do { (var)->attributes |= att_exported; array_needs_making = 1; } while (0)

#define SETVARATTR(var, attr, undo) \
	((undo == 0) ? ((var)->attributes |= (attr)) \
		     : ((var)->attributes &= ~(attr)))

#define VSETATTR(var, attr)	((var)->attributes |= (attr))
#define VUNSETATTR(var, attr)	((var)->attributes &= ~(attr))

#define VGETFLAGS(var)		((var)->attributes)

#define VSETFLAGS(var, flags)	((var)->attributes = (flags))
#define VCLRFLAGS(var)		((var)->attributes = 0)

 
#define CLEAR_EXPORTSTR(var)	(var)->exportstr = (char *)NULL
#define COPY_EXPORTSTR(var)	((var)->exportstr) ? savestring ((var)->exportstr) : (char *)NULL
#define SET_EXPORTSTR(var, value)  (var)->exportstr = (value)
#define SAVE_EXPORTSTR(var, value) (var)->exportstr = (value) ? savestring (value) : (char *)NULL

#define FREE_EXPORTSTR(var) \
	do { if ((var)->exportstr) free ((var)->exportstr); } while (0)

#define CACHE_IMPORTSTR(var, value) \
	(var)->exportstr = savestring (value)

#define INVALIDATE_EXPORTSTR(var) \
	do { \
	  if ((var)->exportstr) \
	    { \
	      free ((var)->exportstr); \
	      (var)->exportstr = (char *)NULL; \
	    } \
	} while (0)

#define ifsname(s)	((s)[0] == 'I' && (s)[1] == 'F' && (s)[2] == 'S' && (s)[3] == '\0')

 
#define MKLOC_ASSOCOK		0x01
#define MKLOC_ARRAYOK		0x02
#define MKLOC_INHERIT		0x04

 
extern SHELL_VAR nameref_invalid_value;
#define INVALID_NAMEREF_VALUE	(void *)&nameref_invalid_value
	
 
typedef int sh_var_map_func_t PARAMS((SHELL_VAR *));

 
extern VAR_CONTEXT *global_variables;
extern VAR_CONTEXT *shell_variables;

extern HASH_TABLE *shell_functions;
extern HASH_TABLE *temporary_env;

extern int variable_context;
extern char *dollar_vars[];
extern char **export_env;

extern int tempenv_assign_error;
extern int array_needs_making;
extern int shell_level;

 
extern WORD_LIST *rest_of_args;
extern int posparam_count;
extern pid_t dollar_dollar_pid;

extern int localvar_inherit;		 

extern void initialize_shell_variables PARAMS((char **, int));

extern int validate_inherited_value PARAMS((SHELL_VAR *, int));

extern SHELL_VAR *set_if_not PARAMS((char *, char *));

extern void sh_set_lines_and_columns PARAMS((int, int));
extern void set_pwd PARAMS((void));
extern void set_ppid PARAMS((void));
extern void make_funcname_visible PARAMS((int));

extern SHELL_VAR *var_lookup PARAMS((const char *, VAR_CONTEXT *));

extern SHELL_VAR *find_function PARAMS((const char *));
extern FUNCTION_DEF *find_function_def PARAMS((const char *));
extern SHELL_VAR *find_variable PARAMS((const char *));
extern SHELL_VAR *find_variable_noref PARAMS((const char *));
extern SHELL_VAR *find_variable_last_nameref PARAMS((const char *, int));
extern SHELL_VAR *find_global_variable_last_nameref PARAMS((const char *, int));
extern SHELL_VAR *find_variable_nameref PARAMS((SHELL_VAR *));
extern SHELL_VAR *find_variable_nameref_for_create PARAMS((const char *, int));
extern SHELL_VAR *find_variable_nameref_for_assignment PARAMS((const char *, int));
 
extern SHELL_VAR *find_variable_tempenv PARAMS((const char *));
extern SHELL_VAR *find_variable_notempenv PARAMS((const char *));
extern SHELL_VAR *find_global_variable PARAMS((const char *));
extern SHELL_VAR *find_global_variable_noref PARAMS((const char *));
extern SHELL_VAR *find_shell_variable PARAMS((const char *));
extern SHELL_VAR *find_tempenv_variable PARAMS((const char *));
extern SHELL_VAR *find_variable_no_invisible PARAMS((const char *));
extern SHELL_VAR *find_variable_for_assignment PARAMS((const char *));
extern char *nameref_transform_name PARAMS((char *, int));
extern SHELL_VAR *copy_variable PARAMS((SHELL_VAR *));
extern SHELL_VAR *make_local_variable PARAMS((const char *, int));
extern SHELL_VAR *bind_variable PARAMS((const char *, char *, int));
extern SHELL_VAR *bind_global_variable PARAMS((const char *, char *, int));
extern SHELL_VAR *bind_function PARAMS((const char *, COMMAND *));

extern void bind_function_def PARAMS((const char *, FUNCTION_DEF *, int));

extern SHELL_VAR **map_over PARAMS((sh_var_map_func_t *, VAR_CONTEXT *));
SHELL_VAR **map_over_funcs PARAMS((sh_var_map_func_t *));
     
extern SHELL_VAR **all_shell_variables PARAMS((void));
extern SHELL_VAR **all_shell_functions PARAMS((void));
extern SHELL_VAR **all_visible_variables PARAMS((void));
extern SHELL_VAR **all_visible_functions PARAMS((void));
extern SHELL_VAR **all_exported_variables PARAMS((void));
extern SHELL_VAR **local_exported_variables PARAMS((void));
extern SHELL_VAR **all_local_variables PARAMS((int));
#if defined (ARRAY_VARS)
extern SHELL_VAR **all_array_variables PARAMS((void));
#endif
extern char **all_variables_matching_prefix PARAMS((const char *));

extern char **make_var_array PARAMS((HASH_TABLE *));
extern char **add_or_supercede_exported_var PARAMS((char *, int));

extern char *get_variable_value PARAMS((SHELL_VAR *));
extern char *get_string_value PARAMS((const char *));
extern char *sh_get_env_value PARAMS((const char *));
extern char *make_variable_value PARAMS((SHELL_VAR *, char *, int));

extern SHELL_VAR *bind_variable_value PARAMS((SHELL_VAR *, char *, int));
extern SHELL_VAR *bind_int_variable PARAMS((char *, char *, int));
extern SHELL_VAR *bind_var_to_int PARAMS((char *, intmax_t, int));

extern int assign_in_env PARAMS((WORD_DESC *, int));

extern int unbind_variable PARAMS((const char *));
extern int check_unbind_variable PARAMS((const char *));
extern int unbind_nameref PARAMS((const char *));
extern int unbind_variable_noref PARAMS((const char *));
extern int unbind_global_variable PARAMS((const char *));
extern int unbind_global_variable_noref PARAMS((const char *));
extern int unbind_func PARAMS((const char *));
extern int unbind_function_def PARAMS((const char *));
extern int delete_var PARAMS((const char *, VAR_CONTEXT *));
extern int makunbound PARAMS((const char *, VAR_CONTEXT *));
extern int kill_local_variable PARAMS((const char *));

extern void delete_all_variables PARAMS((HASH_TABLE *));
extern void delete_all_contexts PARAMS((VAR_CONTEXT *));
extern void reset_local_contexts PARAMS((void));

extern VAR_CONTEXT *new_var_context PARAMS((char *, int));
extern void dispose_var_context PARAMS((VAR_CONTEXT *));
extern VAR_CONTEXT *push_var_context PARAMS((char *, int, HASH_TABLE *));
extern void pop_var_context PARAMS((void));
extern VAR_CONTEXT *push_scope PARAMS((int, HASH_TABLE *));
extern void pop_scope PARAMS((int));

extern void clear_dollar_vars PARAMS((void));

extern void push_context PARAMS((char *, int, HASH_TABLE *));
extern void pop_context PARAMS((void));
extern void push_dollar_vars PARAMS((void));
extern void pop_dollar_vars PARAMS((void));
extern void dispose_saved_dollar_vars PARAMS((void));

extern void init_bash_argv PARAMS((void));
extern void save_bash_argv PARAMS((void));
extern void push_args PARAMS((WORD_LIST *));
extern void pop_args PARAMS((void));

extern void adjust_shell_level PARAMS((int));
extern void non_unsettable PARAMS((char *));
extern void dispose_variable PARAMS((SHELL_VAR *));
extern void dispose_used_env_vars PARAMS((void));
extern void dispose_function_env PARAMS((void));
extern void dispose_builtin_env PARAMS((void));
extern void merge_temporary_env PARAMS((void));
extern void flush_temporary_env PARAMS((void));
extern void merge_builtin_env PARAMS((void));
extern void kill_all_local_variables PARAMS((void));

extern void set_var_read_only PARAMS((char *));
extern void set_func_read_only PARAMS((const char *));
extern void set_var_auto_export PARAMS((char *));
extern void set_func_auto_export PARAMS((const char *));

extern void sort_variables PARAMS((SHELL_VAR **));

extern int chkexport PARAMS((char *));
extern void maybe_make_export_env PARAMS((void));
extern void update_export_env_inplace PARAMS((char *, int, char *));
extern void put_command_name_into_env PARAMS((char *));
extern void put_gnu_argv_flags_into_env PARAMS((intmax_t, char *));

extern void print_var_list PARAMS((SHELL_VAR **));
extern void print_func_list PARAMS((SHELL_VAR **));
extern void print_assignment PARAMS((SHELL_VAR *));
extern void print_var_value PARAMS((SHELL_VAR *, int));
extern void print_var_function PARAMS((SHELL_VAR *));

#if defined (ARRAY_VARS)
extern SHELL_VAR *make_new_array_variable PARAMS((char *));
extern SHELL_VAR *make_local_array_variable PARAMS((char *, int));

extern SHELL_VAR *make_new_assoc_variable PARAMS((char *));
extern SHELL_VAR *make_local_assoc_variable PARAMS((char *, int));

extern void set_pipestatus_array PARAMS((int *, int));
extern ARRAY *save_pipestatus_array PARAMS((void));
extern void restore_pipestatus_array PARAMS((ARRAY *));
#endif

extern void set_pipestatus_from_exit PARAMS((int));

 
extern void stupidly_hack_special_variables PARAMS((char *));

 
extern void reinit_special_variables PARAMS((void));

extern int get_random_number PARAMS((void));

 
extern void sv_ifs PARAMS((char *));
extern void sv_path PARAMS((char *));
extern void sv_mail PARAMS((char *));
extern void sv_funcnest PARAMS((char *));
extern void sv_execignore PARAMS((char *));
extern void sv_globignore PARAMS((char *));
extern void sv_ignoreeof PARAMS((char *));
extern void sv_strict_posix PARAMS((char *));
extern void sv_optind PARAMS((char *));
extern void sv_opterr PARAMS((char *));
extern void sv_locale PARAMS((char *));
extern void sv_xtracefd PARAMS((char *));
extern void sv_shcompat PARAMS((char *));

#if defined (READLINE)
extern void sv_comp_wordbreaks PARAMS((char *));
extern void sv_terminal PARAMS((char *));
extern void sv_hostfile PARAMS((char *));
extern void sv_winsize PARAMS((char *));
#endif

#if defined (__CYGWIN__)
extern void sv_home PARAMS((char *));
#endif

#if defined (HISTORY)
extern void sv_histsize PARAMS((char *));
extern void sv_histignore PARAMS((char *));
extern void sv_history_control PARAMS((char *));
#  if defined (BANG_HISTORY)
extern void sv_histchars PARAMS((char *));
#  endif
extern void sv_histtimefmt PARAMS((char *));
#endif  

#if defined (HAVE_TZSET)
extern void sv_tz PARAMS((char *));
#endif

#if defined (JOB_CONTROL)
extern void sv_childmax PARAMS((char *));
#endif

#endif  
