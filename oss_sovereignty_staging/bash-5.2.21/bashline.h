 

 

#if !defined (_BASHLINE_H_)
#define _BASHLINE_H_

#include "stdc.h"

extern int bash_readline_initialized;
extern int hostname_list_initialized;

 
extern int perform_hostname_completion;
extern int no_empty_command_completion;
extern int force_fignore;
extern int dircomplete_spelling;
extern int dircomplete_expand;
extern int dircomplete_expand_relpath;
extern int complete_fullquote;

extern void posix_readline_initialize PARAMS((int));
extern void reset_completer_word_break_chars PARAMS((void));
extern int enable_hostname_completion PARAMS((int));
extern void initialize_readline PARAMS((void));
extern void bashline_reset PARAMS((void));
extern void bashline_reinitialize PARAMS((void));
extern int bash_re_edit PARAMS((char *));

extern void bashline_set_event_hook PARAMS((void));
extern void bashline_reset_event_hook PARAMS((void));

extern int bind_keyseq_to_unix_command PARAMS((char *));
extern int bash_execute_unix_command PARAMS((int, int));
extern int print_unix_command_map PARAMS((void));
extern int unbind_unix_command PARAMS((char *));

extern char **bash_default_completion PARAMS((const char *, int, int, int, int));

extern void set_directory_hook PARAMS((void));

 
extern char *command_word_completion_function PARAMS((const char *, int));
extern char *bash_groupname_completion_function PARAMS((const char *, int));
extern char *bash_servicename_completion_function PARAMS((const char *, int));

extern char **get_hostname_list PARAMS((void));
extern void clear_hostname_list PARAMS((void));

extern char **bash_directory_completion_matches PARAMS((const char *));
extern char *bash_dequote_text PARAMS((const char *));

#endif  
