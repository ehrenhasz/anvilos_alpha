 

#ifndef _GL_DI_SET_H
# define _GL_DI_SET_H

 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <sys/types.h>

struct di_set;

void di_set_free (struct di_set *) _GL_ATTRIBUTE_NONNULL ((1));

struct di_set *di_set_alloc (void)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (di_set_free, 1);

int di_set_insert (struct di_set *, dev_t, ino_t) _GL_ATTRIBUTE_NONNULL ((1));

int di_set_lookup (struct di_set *dis, dev_t dev, ino_t ino)
  _GL_ATTRIBUTE_NONNULL ((1));

#endif
