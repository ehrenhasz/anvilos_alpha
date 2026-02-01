 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stddef.h>

 

#if !defined __STDC_VERSION__ || __STDC_VERSION__ < 201112
# define FLEXALIGNOF(type) (sizeof (type) & ~ (sizeof (type) - 1))
#else
# define FLEXALIGNOF(type) _Alignof (type)
#endif

 

#define FLEXSIZEOF(type, member, n) \
   ((offsetof (type, member) + FLEXALIGNOF (type) - 1 + (n)) \
    & ~ (FLEXALIGNOF (type) - 1))

 
#define FLEXNSIZEOF(type, member, n) \
  FLEXSIZEOF (type, member, (n) * sizeof (((type *) 0)->member[0]))
