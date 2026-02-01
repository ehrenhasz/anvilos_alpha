 

 

#if !defined (_PARSER_H_)
#  define _PARSER_H_

#  include "command.h"
#  include "input.h"

 
#define PST_CASEPAT	0x000001	 
#define PST_ALEXPNEXT	0x000002	 
#define PST_ALLOWOPNBRC	0x000004	 
#define PST_NEEDCLOSBRC	0x000008	 
#define PST_DBLPAREN	0x000010	 
#define PST_SUBSHELL	0x000020	 
#define PST_CMDSUBST	0x000040	 
#define PST_CASESTMT	0x000080	 
#define PST_CONDCMD	0x000100	 
#define PST_CONDEXPR	0x000200	 
#define PST_ARITHFOR	0x000400	 
#define PST_ALEXPAND	0x000800	 
#define PST_EXTPAT	0x001000	 
#define PST_COMPASSIGN	0x002000	 
#define PST_ASSIGNOK	0x004000	 
#define PST_EOFTOKEN	0x008000	 
#define PST_REGEXP	0x010000	 
#define PST_HEREDOC	0x020000	 
#define PST_REPARSE	0x040000	 
#define PST_REDIRLIST	0x080000	 
#define PST_COMMENT	0x100000	 
#define PST_ENDALIAS	0x200000	 
#define PST_NOEXPAND	0x400000	 
#define PST_NOERROR	0x800000	 
#define PST_STRING	0x1000000	 

 
struct dstack {
 
  char *delimiters;

 
  int delimiter_depth;

 
  int delimiter_space;
};

 
#define DOLBRACE_PARAM	0x01
#define DOLBRACE_OP	0x02
#define DOLBRACE_WORD	0x04

#define DOLBRACE_QUOTE	0x40	 
#define DOLBRACE_QUOTE2	0x80	 

 
extern struct dstack dstack;

extern char *primary_prompt;
extern char *secondary_prompt;

extern char *current_prompt_string;

extern char *ps1_prompt;
extern char *ps2_prompt;
extern char *ps0_prompt;

extern int expand_aliases;
extern int current_command_line_count;
extern int saved_command_line_count;
extern int shell_eof_token;
extern int current_token;
extern int parser_state;
extern int need_here_doc;

extern int ignoreeof;
extern int eof_encountered;
extern int eof_encountered_limit;

extern int line_number, line_number_base;

#endif  
