 

 

#if !defined (_DISPOSE_CMD_H_)
#define _DISPOSE_CMD_H_

#include "stdc.h"

extern void dispose_command PARAMS((COMMAND *));
extern void dispose_word_desc PARAMS((WORD_DESC *));
extern void dispose_word PARAMS((WORD_DESC *));
extern void dispose_words PARAMS((WORD_LIST *));
extern void dispose_word_array PARAMS((char **));
extern void dispose_redirects PARAMS((REDIRECT *));

#if defined (COND_COMMAND)
extern void dispose_cond_node PARAMS((COND_COM *));
#endif

extern void dispose_function_def_contents PARAMS((FUNCTION_DEF *));
extern void dispose_function_def PARAMS((FUNCTION_DEF *));

#endif  
