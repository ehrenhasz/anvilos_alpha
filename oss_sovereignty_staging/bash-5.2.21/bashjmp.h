 

 

#ifndef _BASHJMP_H_
#define _BASHJMP_H_

#include "posixjmp.h"

extern procenv_t	top_level;
extern procenv_t	subshell_top_level;
extern procenv_t	return_catch;	 
extern procenv_t	wait_intr_buf;

extern int no_longjmp_on_fatal_error;

#define SHFUNC_RETURN()	sh_longjmp (return_catch, 1)

#define COPY_PROCENV(old, save) \
	xbcopy ((char *)old, (char *)save, sizeof (procenv_t));

 
#define NOT_JUMPED	0	 
#define FORCE_EOF	1	 
#define DISCARD		2	 
#define EXITPROG	3	 
#define ERREXIT		4	 	
#define SIGEXIT		5	 
#define EXITBLTIN	6	 

#endif  
