 

 

#if !defined (_REDIR_H_)
#define _REDIR_H_

#include "stdc.h"

 
#define RX_ACTIVE	0x01	 
#define RX_UNDOABLE	0x02	 
#define RX_CLEXEC	0x04	 
#define RX_INTERNAL	0x08
#define RX_USER		0x10
#define RX_SAVCLEXEC	0x20	 
#define RX_SAVEFD	0x40	 

extern void redirection_error PARAMS((REDIRECT *, int, char *));
extern int do_redirections PARAMS((REDIRECT *, int));
extern char *redirection_expand PARAMS((WORD_DESC *));
extern int stdin_redirects PARAMS((REDIRECT *));

 
extern int open_redir_file PARAMS((REDIRECT *, char **));

#endif  
