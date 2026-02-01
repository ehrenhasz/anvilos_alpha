 

 

#if !defined (_ERROR_H_)
#define _ERROR_H_

#include "stdc.h"

 
extern char *get_name_for_error PARAMS((void));

 
extern void file_error PARAMS((const char *));

 
extern void programming_error PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));

 
extern void report_error PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));

 
extern void parser_error PARAMS((int, const char *, ...))  __attribute__((__format__ (printf, 2, 3)));

 
extern void fatal_error PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));

 
extern void sys_error PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));

 
extern void internal_error PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));

 
extern void internal_warning PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));

 
extern void internal_debug PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));

 
extern void internal_inform PARAMS((const char *, ...))  __attribute__((__format__ (printf, 1, 2)));

 
extern char *strescape PARAMS((const char *));
extern void itrace PARAMS((const char *, ...)) __attribute__ ((__format__ (printf, 1, 2)));
extern void trace PARAMS((const char *, ...)) __attribute__ ((__format__ (printf, 1, 2)));

 
extern void command_error PARAMS((const char *, int, int, int));

extern char *command_errstr PARAMS((int));

 

extern void err_badarraysub PARAMS((const char *));
extern void err_unboundvar PARAMS((const char *));
extern void err_readonly PARAMS((const char *));

#ifdef DEBUG
#  define INTERNAL_DEBUG(x)	internal_debug x
#else
#  define INTERNAL_DEBUG(x)
#endif

#endif  
