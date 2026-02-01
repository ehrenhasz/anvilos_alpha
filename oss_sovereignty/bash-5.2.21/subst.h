 

 

#if !defined (_SUBST_H_)
#define _SUBST_H_

#include "stdc.h"

 
#define Q_DOUBLE_QUOTES  0x001
#define Q_HERE_DOCUMENT  0x002
#define Q_KEEP_BACKSLASH 0x004
#define Q_PATQUOTE	 0x008
#define Q_QUOTED	 0x010
#define Q_ADDEDQUOTES	 0x020
#define Q_QUOTEDNULL	 0x040
#define Q_DOLBRACE	 0x080
#define Q_ARITH		 0x100	 
#define Q_ARRAYSUB	 0x200	 

 
#define ASS_APPEND	0x0001
#define ASS_MKLOCAL	0x0002
#define ASS_MKASSOC	0x0004
#define ASS_MKGLOBAL	0x0008	 
#define ASS_NAMEREF	0x0010	 
#define ASS_FORCE	0x0020	 
#define ASS_CHKLOCAL	0x0040	 
#define ASS_NOEXPAND	0x0080	 
#define ASS_NOEVAL	0x0100	 
#define ASS_NOLONGJMP	0x0200	 
#define ASS_NOINVIS	0x0400	 
#define ASS_ALLOWALLSUB	0x0800	 
#define ASS_ONEWORD	0x1000	 

 
#define SX_NOALLOC	0x0001	 
#define SX_VARNAME	0x0002	 
#define SX_REQMATCH	0x0004	 
#define SX_COMMAND	0x0008	 
#define SX_NOCTLESC	0x0010	 
#define SX_NOESCCTLNUL	0x0020	 
#define SX_NOLONGJMP	0x0040	 
#define SX_ARITHSUB	0x0080	 
#define SX_POSIXEXP	0x0100	 
#define SX_WORD		0x0200	 
#define SX_COMPLETE	0x0400	 
#define SX_STRIPDQ	0x0800	 
#define SX_NOERROR	0x1000	 

 
extern char * de_backslash PARAMS((char *));

 
extern void unquote_bang PARAMS((char *));

 
extern char *extract_command_subst PARAMS((char *, int *, int));

 
extern char *extract_arithmetic_subst PARAMS((char *, int *));

#if defined (PROCESS_SUBSTITUTION)
 
extern char *extract_process_subst PARAMS((char *, char *, int *, int));
#endif  

 
extern char *assignment_name PARAMS((char *));

 
extern char *string_list_internal PARAMS((WORD_LIST *, char *));

 
extern char *string_list PARAMS((WORD_LIST *));

 
extern char *string_list_dollar_star PARAMS((WORD_LIST *, int, int));

 
extern char *string_list_dollar_at PARAMS((WORD_LIST *, int, int));

 
extern char *string_list_pos_params PARAMS((int, WORD_LIST *, int, int));

 
extern void word_list_remove_quoted_nulls PARAMS((WORD_LIST *));

 
extern WORD_LIST *list_string PARAMS((char *, char *, int));

extern char *ifs_firstchar  PARAMS((int *));
extern char *get_word_from_string PARAMS((char **, char *, char **));
extern char *strip_trailing_ifs_whitespace PARAMS((char *, char *, int));

 
extern int do_assignment PARAMS((char *));
extern int do_assignment_no_expand PARAMS((char *));
extern int do_word_assignment PARAMS((WORD_DESC *, int));

 
extern char *sub_append_string PARAMS((char *, char *, size_t *, size_t *));

 
extern char *sub_append_number PARAMS((intmax_t, char *, int *, int *));

 
extern WORD_LIST *list_rest_of_args PARAMS((void));

 
extern char *string_rest_of_args PARAMS((int));

 
extern WORD_LIST *expand_string_unsplit PARAMS((char *, int));

 
extern WORD_LIST *expand_string_assignment PARAMS((char *, int));

 
extern WORD_LIST *expand_prompt_string PARAMS((char *, int, int));

 
extern WORD_LIST *expand_string PARAMS((char *, int));

 
extern char *expand_string_to_string PARAMS((char *, int));
extern char *expand_string_unsplit_to_string PARAMS((char *, int));
extern char *expand_assignment_string_to_string PARAMS((char *, int));
extern char *expand_subscript_string PARAMS((char *, int));

 
extern char *expand_arith_string PARAMS((char *, int));

 
extern char *expand_string_dollar_quote PARAMS((char *, int));

 
extern char *dequote_string PARAMS((char *));

 
extern char *dequote_escapes PARAMS((const char *));

extern WORD_DESC *dequote_word PARAMS((WORD_DESC *));

 
extern WORD_LIST *dequote_list PARAMS((WORD_LIST *));

 
extern WORD_LIST *expand_word PARAMS((WORD_DESC *, int));

 
extern WORD_LIST *expand_word_unsplit PARAMS((WORD_DESC *, int));
extern WORD_LIST *expand_word_leave_quoted PARAMS((WORD_DESC *, int));

 
extern char *get_dollar_var_value PARAMS((intmax_t));

 
extern char *quote_string PARAMS((char *));

 
extern char *quote_escapes PARAMS((const char *));

 
extern char *remove_quoted_escapes PARAMS((char *));

 
extern char *remove_quoted_nulls PARAMS((char *));

 
extern char *string_quote_removal PARAMS((char *, int));

 
extern WORD_DESC *word_quote_removal PARAMS((WORD_DESC *, int));

 
extern WORD_LIST *word_list_quote_removal PARAMS((WORD_LIST *, int));

 
extern void setifs PARAMS((SHELL_VAR *));

 
extern char *getifs PARAMS((void));

 
extern WORD_LIST *word_split PARAMS((WORD_DESC *, char *));

 
extern WORD_LIST *expand_words PARAMS((WORD_LIST *));

 
extern WORD_LIST *expand_words_no_vars PARAMS((WORD_LIST *));

 
extern WORD_LIST *expand_words_shellexp PARAMS((WORD_LIST *));

extern WORD_DESC *command_substitute PARAMS((char *, int, int));
extern char *pat_subst PARAMS((char *, char *, char *, int));

#if defined (PROCESS_SUBSTITUTION)
extern int fifos_pending PARAMS((void));
extern int num_fifos PARAMS((void));
extern void unlink_fifo_list PARAMS((void));
extern void unlink_all_fifos PARAMS((void));
extern void unlink_fifo PARAMS((int));

extern void *copy_fifo_list PARAMS((int *));
extern void close_new_fifos PARAMS((void *, int));

extern void clear_fifo_list PARAMS((void));

extern int find_procsub_child PARAMS((pid_t));
extern void set_procsub_status PARAMS((int, pid_t, int));

extern void wait_procsubs PARAMS((void));
extern void reap_procsubs PARAMS((void));
#endif

extern WORD_LIST *list_string_with_quotes PARAMS((char *));

#if defined (ARRAY_VARS)
extern char *extract_array_assignment_list PARAMS((char *, int *));
#endif

#if defined (COND_COMMAND)
extern char *remove_backslashes PARAMS((char *));
extern char *cond_expand_word PARAMS((WORD_DESC *, int));
#endif

 
#define SD_NOJMP	0x001	 
#define SD_INVERT	0x002	 
#define SD_NOQUOTEDELIM	0x004	 
#define SD_NOSKIPCMD	0x008	 
#define SD_EXTGLOB	0x010	 
#define SD_IGNOREQUOTE	0x020	 
#define SD_GLOB		0x040	 
#define SD_NOPROCSUB	0x080	 
#define SD_COMPLETE	0x100	 
#define SD_HISTEXP	0x200	 
#define SD_ARITHEXP	0x400	 
#define SD_NOERROR	0x800	 

extern int skip_to_delim PARAMS((char *, int, char *, int));

#if defined (BANG_HISTORY)
extern int skip_to_histexp PARAMS((char *, int, char *, int));
#endif

#if defined (READLINE)
extern int char_is_quoted PARAMS((char *, int));
extern int unclosed_pair PARAMS((char *, int, char *));
extern WORD_LIST *split_at_delims PARAMS((char *, int, const char *, int, int, int *, int *));
#endif

 
extern SHELL_VAR *ifs_var;
extern char *ifs_value;
extern unsigned char ifs_cmap[];
extern int ifs_is_set, ifs_is_null;

#if defined (HANDLE_MULTIBYTE)
extern unsigned char ifs_firstc[];
extern size_t ifs_firstc_len;
#else
extern unsigned char ifs_firstc;
#endif

extern int assigning_in_environment;
extern int expanding_redir;
extern int inherit_errexit;

extern pid_t last_command_subst_pid;

 
#define isifs(c)	(ifs_cmap[(unsigned char)(c)] != 0)

 
#define QUOTED_CHAR(c)  ((c) == CTLESC)

 
#define QUOTED_NULL(string) ((string)[0] == CTLNUL && (string)[1] == '\0')

extern void invalidate_cached_quoted_dollar_at PARAMS((void));

#endif  
