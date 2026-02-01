 

#ifndef RANDINT_H

# define RANDINT_H 1

# include <stdint.h>

# include "randread.h"

 
typedef uintmax_t randint;
# define RANDINT_MAX UINTMAX_MAX

struct randint_source;

void randint_free (struct randint_source *) _GL_ATTRIBUTE_NONNULL ();
int randint_all_free (struct randint_source *) _GL_ATTRIBUTE_NONNULL ();
struct randint_source *randint_new (struct randread_source *)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (randint_free, 1)
  _GL_ATTRIBUTE_NONNULL () _GL_ATTRIBUTE_RETURNS_NONNULL;
struct randint_source *randint_all_new (char const *, size_t)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (randint_all_free, 1);
struct randread_source *randint_get_source (struct randint_source const *)
  _GL_ATTRIBUTE_NONNULL () _GL_ATTRIBUTE_PURE;
randint randint_genmax (struct randint_source *, randint genmax)
  _GL_ATTRIBUTE_NONNULL ();

 
static inline randint
randint_choose (struct randint_source *s, randint choices)
{
  return randint_genmax (s, choices - 1);
}

#endif
