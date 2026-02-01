 

 

#include "c_threaded_variables.h"

#define WRAP(type, name)        \
  type                          \
  name ## _as_function (void)   \
  {                             \
    return name;                \
  }
 
WRAP(WINDOW *, stdscr)
WRAP(WINDOW *, curscr)

WRAP(int, LINES)
WRAP(int, COLS)
WRAP(int, TABSIZE)
WRAP(int, COLORS)
WRAP(int, COLOR_PAIRS)

chtype
acs_map_as_function(char inx)
{
  return acs_map[(unsigned char) inx];
}
 
