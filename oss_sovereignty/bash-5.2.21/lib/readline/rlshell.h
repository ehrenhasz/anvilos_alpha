 

 

#if !defined (_RL_SHELL_H_)
#define _RL_SHELL_H_

#include "rlstdc.h"

extern char *sh_single_quote (char *);
extern void sh_set_lines_and_columns (int, int);
extern char *sh_get_env_value (const char *);
extern char *sh_get_home_dir (void);
extern int sh_unset_nodelay_mode (int);

#endif  
