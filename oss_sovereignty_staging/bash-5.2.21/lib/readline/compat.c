 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <stdio.h>

#include "rlstdc.h"
#include "rltypedefs.h"

extern void rl_free_undo_list (void);
extern int rl_maybe_save_line (void);
extern int rl_maybe_unsave_line (void);
extern int rl_maybe_replace_line (void);

extern int rl_crlf (void);
extern int rl_ding (void);
extern int rl_alphabetic (int);

extern char **rl_completion_matches (const char *, rl_compentry_func_t *);
extern char *rl_username_completion_function (const char *, int);
extern char *rl_filename_completion_function (const char *, int);

 

void
free_undo_list (void)
{
  rl_free_undo_list ();
}

int
maybe_replace_line (void)
{
  return rl_maybe_replace_line ();
}

int
maybe_save_line (void)
{
  return rl_maybe_save_line ();
}

int
maybe_unsave_line (void)
{
  return rl_maybe_unsave_line ();
}

int
ding (void)
{
  return rl_ding ();
}

int
crlf (void)
{
  return rl_crlf ();
}

int
alphabetic (int c)
{
  return rl_alphabetic (c);
}

char **
completion_matches (const char *s, rl_compentry_func_t *f)
{
  return rl_completion_matches (s, f);
}

char *
username_completion_function (const char *s, int i)
{
  return rl_username_completion_function (s, i);
}

char *
filename_completion_function (const char *s, int i)
{
  return rl_filename_completion_function (s, i);
}
