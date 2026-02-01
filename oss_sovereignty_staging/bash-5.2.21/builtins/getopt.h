 

 

 

#ifndef _SH_GETOPT_H
#define _SH_GETOPT_H 1

#include "stdc.h"

 

extern char *sh_optarg;

 

extern int sh_optind;

 

extern int sh_opterr;

 

extern int sh_optopt;

 
extern int sh_badopt;

extern int sh_getopt PARAMS((int, char *const *, const char *));

typedef struct sh_getopt_state
{
  char *gs_optarg;
  int gs_optind;
  int gs_curopt;
  char *gs_nextchar;
  int gs_charindex;
  int gs_flags;
} sh_getopt_state_t;

extern void sh_getopt_restore_state PARAMS((char **));

extern sh_getopt_state_t *sh_getopt_alloc_istate PARAMS((void));
extern void sh_getopt_dispose_istate PARAMS((sh_getopt_state_t *));

extern sh_getopt_state_t *sh_getopt_save_istate PARAMS((void));
extern void sh_getopt_restore_istate PARAMS((sh_getopt_state_t *));

#endif  
