 

 

#if  !defined (__COMMON_H)
#  define __COMMON_H

#include "stdc.h"

#define ISOPTION(s, c)	(s[0] == '-' && s[1] == c && !s[2])
#define ISHELP(s)	(STREQ ((s), "--help"))

#define CHECK_HELPOPT(l) \
do { \
  if ((l) && (l)->word && ISHELP((l)->word->word)) \
    { \
      builtin_help (); \
      return (EX_USAGE); \
    } \
} while (0)

#define CASE_HELPOPT \
  case GETOPT_HELP: \
    builtin_help (); \
    return (EX_USAGE)

 
#define SEVAL_NONINT	0x001
#define SEVAL_INTERACT	0x002
#define SEVAL_NOHIST	0x004
#define SEVAL_NOFREE	0x008
#define SEVAL_RESETLINE	0x010
#define SEVAL_PARSEONLY	0x020
#define SEVAL_NOLONGJMP 0x040
#define SEVAL_FUNCDEF	0x080		 
#define SEVAL_ONECMD	0x100		 
#define SEVAL_NOHISTEXP	0x200		 
#define SEVAL_NOOPTIMIZE 0x400		 

 
#define CDESC_ALL		0x001	 
#define CDESC_SHORTDESC		0x002	 
#define CDESC_REUSABLE		0x004	 
#define CDESC_TYPE		0x008	 
#define CDESC_PATH_ONLY		0x010	 
#define CDESC_FORCE_PATH	0x020	 
#define CDESC_NOFUNCS		0x040	 
#define CDESC_ABSPATH		0x080	 
#define CDESC_STDPATH		0x100	 

 
#define JM_PREFIX		0x01	 
#define JM_SUBSTRING		0x02	 
#define JM_EXACT		0x04	 
#define JM_STOPPED		0x08	 
#define JM_FIRSTMATCH		0x10	 

 
#define ARGS_NONE		0x0
#define ARGS_INVOC		0x01
#define ARGS_FUNC		0x02
#define ARGS_SETBLTIN		0x04

 
#define MAX_ATTRIBUTES		16

 
extern void builtin_error PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));
extern void builtin_warning PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));
extern void builtin_usage PARAMS((void));
extern void no_args PARAMS((WORD_LIST *));
extern int no_options PARAMS((WORD_LIST *));

 
extern void sh_needarg PARAMS((char *));
extern void sh_neednumarg PARAMS((char *));
extern void sh_notfound PARAMS((char *));
extern void sh_invalidopt PARAMS((char *));
extern void sh_invalidoptname PARAMS((char *));
extern void sh_invalidid PARAMS((char *));
extern void sh_invalidnum PARAMS((char *));
extern void sh_invalidsig PARAMS((char *));
extern void sh_readonly PARAMS((const char *));
extern void sh_noassign PARAMS((const char *));
extern void sh_erange PARAMS((char *, char *));
extern void sh_badpid PARAMS((char *));
extern void sh_badjob PARAMS((char *));
extern void sh_nojobs PARAMS((char *));
extern void sh_restricted PARAMS((char *));
extern void sh_notbuiltin PARAMS((char *));
extern void sh_wrerror PARAMS((void));
extern void sh_ttyerror PARAMS((int));
extern int sh_chkwrite PARAMS((int));

extern char **make_builtin_argv PARAMS((WORD_LIST *, int *));
extern void remember_args PARAMS((WORD_LIST *, int));
extern void shift_args PARAMS((int));
extern int number_of_args PARAMS((void));

extern int dollar_vars_changed PARAMS((void));
extern void set_dollar_vars_unchanged PARAMS((void));
extern void set_dollar_vars_changed PARAMS((void));

extern int get_numeric_arg PARAMS((WORD_LIST *, int, intmax_t *));
extern int get_exitstat PARAMS((WORD_LIST *));
extern int read_octal PARAMS((char *));

 
extern char *the_current_working_directory;
extern char *get_working_directory PARAMS((char *));
extern void set_working_directory PARAMS((char *));

#if defined (JOB_CONTROL)
extern int get_job_by_name PARAMS((const char *, int));
extern int get_job_spec PARAMS((WORD_LIST *));
#endif
extern int display_signal_list PARAMS((WORD_LIST *, int));

 
extern struct builtin *builtin_address_internal PARAMS((char *, int));
extern sh_builtin_func_t *find_shell_builtin PARAMS((char *));
extern sh_builtin_func_t *builtin_address PARAMS((char *));
extern sh_builtin_func_t *find_special_builtin PARAMS((char *));
extern void initialize_shell_builtins PARAMS((void));

#if defined (ARRAY_VARS)
extern int set_expand_once PARAMS((int, int));
#endif

 
extern void bash_logout PARAMS((void));

 
extern void getopts_reset PARAMS((int));

 
extern void builtin_help PARAMS((void));

 
extern void read_tty_cleanup PARAMS((void));
extern int read_tty_modified PARAMS((void));

extern int read_builtin_timeout PARAMS((int));
extern void check_read_timeout PARAMS((void));

 
extern int minus_o_option_value PARAMS((char *));
extern void list_minus_o_opts PARAMS((int, int));
extern char **get_minus_o_opts PARAMS((void));
extern int set_minus_o_option PARAMS((int, char *));

extern void set_shellopts PARAMS((void));
extern void parse_shellopts PARAMS((char *));
extern void initialize_shell_options PARAMS((int));

extern void reset_shell_options PARAMS((void));

extern char *get_current_options PARAMS((void));
extern void set_current_options PARAMS((const char *));

 
extern void reset_shopt_options PARAMS((void));
extern char **get_shopt_options PARAMS((void));

extern int shopt_setopt PARAMS((char *, int));
extern int shopt_listopt PARAMS((char *, int));

extern int set_login_shell PARAMS((char *, int));

extern void set_bashopts PARAMS((void));
extern void parse_bashopts PARAMS((char *));
extern void initialize_bashopts PARAMS((int));

extern void set_compatibility_opts PARAMS((void));

 
extern int describe_command PARAMS((char *, int));

 
extern int set_or_show_attributes PARAMS((WORD_LIST *, int, int));
extern int show_all_var_attributes PARAMS((int, int));
extern int show_local_var_attributes PARAMS((int, int));
extern int show_var_attributes PARAMS((SHELL_VAR *, int, int));
extern int show_name_attributes PARAMS((char *, int));
extern int show_localname_attributes PARAMS((char *, int));
extern int show_func_attributes PARAMS((char *, int));
extern void set_var_attribute PARAMS((char *, int, int));
extern int var_attribute_string PARAMS((SHELL_VAR *, int, char *));

 
extern char *get_dirstack_from_string PARAMS((char *));
extern char *get_dirstack_element PARAMS((intmax_t, int));
extern void set_dirstack_element PARAMS((intmax_t, int, char *));
extern WORD_LIST *get_directory_stack PARAMS((int));

 
extern int parse_and_execute PARAMS((char *, const char *, int));
extern int evalstring PARAMS((char *, const char *, int));
extern void parse_and_execute_cleanup PARAMS((int));
extern int parse_string PARAMS((char *, const char *, int, COMMAND **, char **));
extern int should_suppress_fork PARAMS((COMMAND *));
extern int can_optimize_connection PARAMS((COMMAND *));
extern int can_optimize_cat_file PARAMS((COMMAND *));
extern void optimize_connection_fork PARAMS((COMMAND *));
extern void optimize_subshell_command PARAMS((COMMAND *));
extern void optimize_shell_function PARAMS((COMMAND *));

 
extern int maybe_execute_file PARAMS((const char *, int));
extern int force_execute_file PARAMS((const char *, int));
extern int source_file PARAMS((const char *, int));
extern int fc_execute_file PARAMS((const char *));

 
extern sh_builtin_func_t *this_shell_builtin;
extern sh_builtin_func_t *last_shell_builtin;

extern SHELL_VAR *builtin_bind_variable PARAMS((char *, char *, int));
extern SHELL_VAR *builtin_bind_var_to_int PARAMS((char *, intmax_t, int));
extern int builtin_unbind_variable PARAMS((const char *));

extern SHELL_VAR *builtin_find_indexed_array PARAMS((char *, int));
extern int builtin_arrayref_flags PARAMS((WORD_DESC *, int));

 
extern int sourcelevel;

 
extern int parse_and_execute_level;

 
extern int breaking;
extern int continuing;
extern int loop_level;

 
extern int print_shift_error;

 
#if defined (ARRAY_VARS)
extern int expand_once_flag;
#endif

#if defined (EXTENDED_GLOB)
extern int extglob_flag;
#endif

extern int expaliases_flag;

 
extern int source_searches_cwd;
extern int source_uses_path;

 
extern int wait_intr_flag;

 
#if defined (ARRAY_VARS)
#define SET_VFLAGS(wordflags, vflags, bindflags) \
  do { \
    vflags = assoc_expand_once ?  VA_NOEXPAND : 0; \
    bindflags = assoc_expand_once ? ASS_NOEXPAND : 0; \
    if (assoc_expand_once && (wordflags & W_ARRAYREF)) \
      vflags |= VA_ONEWORD|VA_NOEXPAND; \
    if (vflags & VA_NOEXPAND) \
      bindflags |= ASS_NOEXPAND; \
    if (vflags & VA_ONEWORD) \
      bindflags |= ASS_ONEWORD; \
  } while (0)
#endif

#endif  
