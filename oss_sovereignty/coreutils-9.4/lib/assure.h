 

#ifndef _GL_ASSURE_H
#define _GL_ASSURE_H

#include <assert.h>
#include "verify.h"

 

#ifdef NDEBUG
# define affirm(E) assume (E)
#else
# define affirm(E) assert (E)
#endif

 

#ifdef NDEBUG
# define assure(E) ((void) (0 && (E)))
#else
# define assure(E) assert (E)
#endif

#endif
