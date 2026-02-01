 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stddef.h>

 
#if defined __cplusplus
  template <class type> struct alignof_helper { char __slot1; type __slot2; };
# define alignof_slot(type) offsetof (alignof_helper<type>, __slot2)
#else
# define alignof_slot(type) alignof (type)
#endif

 
#if defined __GNUC__ || defined __clang__ || defined __IBM__ALIGNOF__
# define alignof_type __alignof__
#else
# define alignof_type alignof_slot
#endif

#endif  
