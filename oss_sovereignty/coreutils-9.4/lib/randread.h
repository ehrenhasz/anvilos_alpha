 

#ifndef RANDREAD_H
# define RANDREAD_H 1

# include <stddef.h>

struct randread_source;

int randread_free (struct randread_source *) _GL_ATTRIBUTE_NONNULL ();
struct randread_source *randread_new (char const *, size_t)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (randread_free, 1);
void randread (struct randread_source *, void *, size_t)
  _GL_ATTRIBUTE_NONNULL ();
void randread_set_handler (struct randread_source *, void (*) (void const *))
  _GL_ATTRIBUTE_NONNULL ();
void randread_set_handler_arg (struct randread_source *, void const *)
  _GL_ATTRIBUTE_NONNULL ((1));

#endif
