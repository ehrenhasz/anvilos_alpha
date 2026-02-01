 

 

 

#if !defined (__BASH_GETOPT_H)
#  define __BASH_GETOPT_H

#include <stdc.h>

#define GETOPT_EOF	-1
#define GETOPT_HELP	-99

extern char *list_optarg;
extern int list_optflags;

extern int list_optopt;
extern int list_opttype;

extern WORD_LIST *lcurrent;
extern WORD_LIST *loptend;

extern int internal_getopt PARAMS((WORD_LIST *, char *));
extern void reset_internal_getopt PARAMS((void));

#endif  
