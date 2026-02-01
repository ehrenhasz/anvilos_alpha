 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdio.h>

 
#include "arg-nonnull.h"

struct argv_iterator;

enum argv_iter_err
{
  AI_ERR_OK = 1,
  AI_ERR_EOF,
  AI_ERR_MEM,
  AI_ERR_READ
};

void argv_iter_free (struct argv_iterator *)
  _GL_ARG_NONNULL ((1));

struct argv_iterator *argv_iter_init_argv (char **argv)
  _GL_ARG_NONNULL ((1)) _GL_ATTRIBUTE_DEALLOC (argv_iter_free, 1);
struct argv_iterator *argv_iter_init_stream (FILE *fp)
  _GL_ARG_NONNULL ((1)) _GL_ATTRIBUTE_DEALLOC (argv_iter_free, 1);
char *argv_iter (struct argv_iterator *, enum argv_iter_err *)
  _GL_ARG_NONNULL ((1, 2));
size_t argv_iter_n_args (struct argv_iterator const *)
  _GL_ATTRIBUTE_PURE _GL_ARG_NONNULL ((1));
