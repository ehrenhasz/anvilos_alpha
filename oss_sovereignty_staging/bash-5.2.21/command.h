 

 

#if !defined (_COMMAND_H_)
#define _COMMAND_H_

#include "stdc.h"

 
enum r_instruction {
  r_output_direction, r_input_direction, r_inputa_direction,
  r_appending_to, r_reading_until, r_reading_string,
  r_duplicating_input, r_duplicating_output, r_deblank_reading_until,
  r_close_this, r_err_and_out, r_input_output, r_output_force,
  r_duplicating_input_word, r_duplicating_output_word,
  r_move_input, r_move_output, r_move_input_word, r_move_output_word,
  r_append_err_and_out
};

 
#define REDIR_VARASSIGN		0x01

 
#define AMBIGUOUS_REDIRECT  -1
#define NOCLOBBER_REDIRECT  -2
#define RESTRICTED_REDIRECT -3	 
#define HEREDOC_REDIRECT    -4   
#define BADVAR_REDIRECT	    -5   

#define CLOBBERING_REDIRECT(ri) \
  (ri == r_output_direction || ri == r_err_and_out)

#define OUTPUT_REDIRECT(ri) \
  (ri == r_output_direction || ri == r_input_output || ri == r_err_and_out || ri == r_append_err_and_out)

#define INPUT_REDIRECT(ri) \
  (ri == r_input_direction || ri == r_inputa_direction || ri == r_input_output)

#define WRITE_REDIRECT(ri) \
  (ri == r_output_direction || \
	ri == r_input_output || \
	ri == r_err_and_out || \
	ri == r_appending_to || \
	ri == r_append_err_and_out || \
	ri == r_output_force)

 
#define TRANSLATE_REDIRECT(ri) \
  (ri == r_duplicating_input_word || ri == r_duplicating_output_word || \
   ri == r_move_input_word || ri == r_move_output_word)

 
enum command_type { cm_for, cm_case, cm_while, cm_if, cm_simple, cm_select,
		    cm_connection, cm_function_def, cm_until, cm_group,
		    cm_arith, cm_cond, cm_arith_for, cm_subshell, cm_coproc };

 
#define W_HASDOLLAR	(1 << 0)	 
#define W_QUOTED	(1 << 1)	 
#define W_ASSIGNMENT	(1 << 2)	 
#define W_SPLITSPACE	(1 << 3)	 
#define W_NOSPLIT	(1 << 4)	 
#define W_NOGLOB	(1 << 5)	 
#define W_NOSPLIT2	(1 << 6)	 
#define W_TILDEEXP	(1 << 7)	 
#define W_DOLLARAT	(1 << 8)	 
#define W_ARRAYREF	(1 << 9)	 
#define W_NOCOMSUB	(1 << 10)	 
#define W_ASSIGNRHS	(1 << 11)	 
#define W_NOTILDE	(1 << 12)	 
#define W_NOASSNTILDE	(1 << 13)	 
#define W_EXPANDRHS	(1 << 14)	 
#define W_COMPASSIGN	(1 << 15)	 
#define W_ASSNBLTIN	(1 << 16)	 
#define W_ASSIGNARG	(1 << 17)	 
#define W_HASQUOTEDNULL	(1 << 18)	 
#define W_DQUOTE	(1 << 19)	 
#define W_NOPROCSUB	(1 << 20)	 
#define W_SAWQUOTEDNULL	(1 << 21)	 
#define W_ASSIGNASSOC	(1 << 22)	 
#define W_ASSIGNARRAY	(1 << 23)	 
#define W_ARRAYIND	(1 << 24)	 
#define W_ASSNGLOBAL	(1 << 25)	 
#define W_NOBRACE	(1 << 26)	 
#define W_COMPLETE	(1 << 27)	 
#define W_CHKLOCAL	(1 << 28)	 
#define W_FORCELOCAL	(1 << 29)	 
 

 
#define PF_NOCOMSUB	0x01	 
#define PF_IGNUNBOUND	0x02	 
#define PF_NOSPLIT2	0x04	 
#define PF_ASSIGNRHS	0x08	 
#define PF_COMPLETE	0x10	 
#define PF_EXPANDRHS	0x20	 
#define PF_ALLINDS	0x40	 
#define PF_BACKQUOTE	0x80	 

 
#define SUBSHELL_ASYNC	0x01	 
#define SUBSHELL_PAREN	0x02	 
#define SUBSHELL_COMSUB	0x04	 
#define SUBSHELL_FORK	0x08	 
#define SUBSHELL_PIPE	0x10	 
#define SUBSHELL_PROCSUB 0x20	 
#define SUBSHELL_COPROC	0x40	 
#define SUBSHELL_RESETTRAP 0x80	 
#define SUBSHELL_IGNTRAP 0x100   

 
typedef struct word_desc {
  char *word;		 
  int flags;		 
} WORD_DESC;

 
typedef struct word_list {
  struct word_list *next;
  WORD_DESC *word;
} WORD_LIST;


 
 
 
 
 

 

typedef union {
  int dest;			 
  WORD_DESC *filename;		 
} REDIRECTEE;

 
typedef struct redirect {
  struct redirect *next;	 
  REDIRECTEE redirector;	 
  int rflags;			 
  int flags;			 
  enum r_instruction  instruction;  
  REDIRECTEE redirectee;	 
  char *here_doc_eof;		 
} REDIRECT;

 
typedef struct element {
  WORD_DESC *word;
  REDIRECT *redirect;
} ELEMENT;

 
#define CMD_WANT_SUBSHELL  0x01	 
#define CMD_FORCE_SUBSHELL 0x02	 
#define CMD_INVERT_RETURN  0x04	 
#define CMD_IGNORE_RETURN  0x08	 
#define CMD_NO_FUNCTIONS   0x10  
#define CMD_INHIBIT_EXPANSION 0x20  
#define CMD_NO_FORK	   0x40	 
#define CMD_TIME_PIPELINE  0x80  
#define CMD_TIME_POSIX	   0x100  
#define CMD_AMPERSAND	   0x200  
#define CMD_STDIN_REDIR	   0x400  
#define CMD_COMMAND_BUILTIN 0x0800  
#define CMD_COPROC_SUBSHELL 0x1000
#define CMD_LASTPIPE	    0x2000
#define CMD_STDPATH	    0x4000	 
#define CMD_TRY_OPTIMIZING  0x8000	 

 
typedef struct command {
  enum command_type type;	 
  int flags;			 
  int line;			 
  REDIRECT *redirects;		 
  union {
    struct for_com *For;
    struct case_com *Case;
    struct while_com *While;
    struct if_com *If;
    struct connection *Connection;
    struct simple_com *Simple;
    struct function_def *Function_def;
    struct group_com *Group;
#if defined (SELECT_COMMAND)
    struct select_com *Select;
#endif
#if defined (DPAREN_ARITHMETIC)
    struct arith_com *Arith;
#endif
#if defined (COND_COMMAND)
    struct cond_com *Cond;
#endif
#if defined (ARITH_FOR_COMMAND)
    struct arith_for_com *ArithFor;
#endif
    struct subshell_com *Subshell;
    struct coproc_com *Coproc;
  } value;
} COMMAND;

 
typedef struct connection {
  int ignore;			 
  COMMAND *first;		 
  COMMAND *second;		 
  int connector;		 
} CONNECTION;

 

 
#define CASEPAT_FALLTHROUGH	0x01
#define CASEPAT_TESTNEXT	0x02

 
typedef struct pattern_list {
  struct pattern_list *next;	 
  WORD_LIST *patterns;		 
  COMMAND *action;		 
  int flags;
} PATTERN_LIST;

 
typedef struct case_com {
  int flags;			 
  int line;			 
  WORD_DESC *word;		 
  PATTERN_LIST *clauses;	 
} CASE_COM;

 
typedef struct for_com {
  int flags;		 
  int line;		 
  WORD_DESC *name;	 
  WORD_LIST *map_list;	 
  COMMAND *action;	 
} FOR_COM;

#if defined (ARITH_FOR_COMMAND)
typedef struct arith_for_com {
  int flags;
  int line;	 
  WORD_LIST *init;
  WORD_LIST *test;
  WORD_LIST *step;
  COMMAND *action;
} ARITH_FOR_COM;
#endif

#if defined (SELECT_COMMAND)
 
typedef struct select_com {
  int flags;		 
  int line;		 
  WORD_DESC *name;	 
  WORD_LIST *map_list;	 
  COMMAND *action;	 
} SELECT_COM;
#endif  

 
typedef struct if_com {
  int flags;			 
  COMMAND *test;		 
  COMMAND *true_case;		 
  COMMAND *false_case;		 
} IF_COM;

 
typedef struct while_com {
  int flags;			 
  COMMAND *test;		 
  COMMAND *action;		 
} WHILE_COM;

#if defined (DPAREN_ARITHMETIC)
 
typedef struct arith_com {
  int flags;
  int line;
  WORD_LIST *exp;
} ARITH_COM;
#endif  

 
#define COND_AND	1
#define COND_OR		2
#define COND_UNARY	3
#define COND_BINARY	4
#define COND_TERM	5
#define COND_EXPR	6

typedef struct cond_com {
  int flags;
  int line;
  int type;
  WORD_DESC *op;
  struct cond_com *left, *right;
} COND_COM;

 
typedef struct simple_com {
  int flags;			 
  int line;			 
  WORD_LIST *words;		 
  REDIRECT *redirects;		 
} SIMPLE_COM;

 
typedef struct function_def {
  int flags;			 
  int line;			 
  WORD_DESC *name;		 
  COMMAND *command;		 
  char *source_file;		 
} FUNCTION_DEF;

 
typedef struct group_com {
  int ignore;			 
  COMMAND *command;
} GROUP_COM;

typedef struct subshell_com {
  int flags;
  int line;
  COMMAND *command;
} SUBSHELL_COM;

#define COPROC_RUNNING	0x01
#define COPROC_DEAD	0x02

typedef struct coproc {
  char *c_name;
  pid_t c_pid;
  int c_rfd;
  int c_wfd;
  int c_rsave;
  int c_wsave;
  int c_flags;
  int c_status;
  int c_lock;
} Coproc;

typedef struct coproc_com {
  int flags;
  char *name;
  COMMAND *command;
} COPROC_COM;

extern COMMAND *global_command;
extern Coproc sh_coproc;

 
#define CMDERR_DEFAULT	0
#define CMDERR_BADTYPE	1
#define CMDERR_BADCONN	2
#define CMDERR_BADJUMP	3

#define CMDERR_LAST	3

 

extern FUNCTION_DEF *copy_function_def_contents PARAMS((FUNCTION_DEF *, FUNCTION_DEF *));
extern FUNCTION_DEF *copy_function_def PARAMS((FUNCTION_DEF *));

extern WORD_DESC *copy_word PARAMS((WORD_DESC *));
extern WORD_LIST *copy_word_list PARAMS((WORD_LIST *));
extern REDIRECT *copy_redirect PARAMS((REDIRECT *));
extern REDIRECT *copy_redirects PARAMS((REDIRECT *));
extern COMMAND *copy_command PARAMS((COMMAND *));

#endif  
