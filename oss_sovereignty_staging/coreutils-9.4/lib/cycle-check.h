 

#ifndef CYCLE_CHECK_H
# define CYCLE_CHECK_H 1

# include <stdint.h>
# include "dev-ino.h"
# include "same-inode.h"

struct cycle_check_state
{
  struct dev_ino dev_ino;
  uintmax_t chdir_counter;
  int magic;
};

void cycle_check_init (struct cycle_check_state *state);
bool cycle_check (struct cycle_check_state *state, struct stat const *sb);

# define CYCLE_CHECK_REFLECT_CHDIR_UP(State, SB_dir, SB_subdir) \
  do                                                            \
    {                                                           \
        \
      if ((State)->chdir_counter == 0)                          \
        abort ();                                               \
      if (SAME_INODE ((State)->dev_ino, SB_subdir))             \
        {                                                       \
          (State)->dev_ino.st_dev = (SB_dir).st_dev;            \
          (State)->dev_ino.st_ino = (SB_dir).st_ino;            \
        }                                                       \
    }                                                           \
  while (0)

#endif
