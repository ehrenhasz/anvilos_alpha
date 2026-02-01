 

 

#ifndef _PARSE_COLORS_H_
#define _PARSE_COLORS_H_

#include "readline.h"

#define LEN_STR_PAIR(s) sizeof (s) - 1, s

void _rl_parse_colors (void);

static const char *const indicator_name[]=
  {
    "lc", "rc", "ec", "rs", "no", "fi", "di", "ln", "pi", "so",
    "bd", "cd", "mi", "or", "ex", "do", "su", "sg", "st",
    "ow", "tw", "ca", "mh", "cl", NULL
  };

 
static char *color_buf;

#endif  
