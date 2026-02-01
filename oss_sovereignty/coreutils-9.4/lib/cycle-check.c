 

#include <config.h>

#include "cycle-check.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "assure.h"

#define CC_MAGIC 9827862

 

static bool
is_zero_or_power_of_two (uintmax_t i)
{
  return (i & (i - 1)) == 0;
}

void
cycle_check_init (struct cycle_check_state *state)
{
  state->chdir_counter = 0;
  state->magic = CC_MAGIC;
}

 

bool
cycle_check (struct cycle_check_state *state, struct stat const *sb)
{
  assure (state->magic == CC_MAGIC);

   
  if (state->chdir_counter && SAME_INODE (*sb, state->dev_ino))
    return true;

   
  if (is_zero_or_power_of_two (++(state->chdir_counter)))
    {
       
      if (state->chdir_counter == 0)
        return true;

      state->dev_ino.st_dev = sb->st_dev;
      state->dev_ino.st_ino = sb->st_ino;
    }

  return false;
}
