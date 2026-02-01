 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdlib.h>  

#define CAN_MODE_MASK (CAN_EXISTING | CAN_ALL_BUT_LAST | CAN_MISSING)

#ifdef __cplusplus
extern "C" {
#endif

enum canonicalize_mode_t
  {
     
    CAN_EXISTING = 0,

     
    CAN_ALL_BUT_LAST = 1,

     
    CAN_MISSING = 2,

     
    CAN_NOLINKS = 4
  };
typedef enum canonicalize_mode_t canonicalize_mode_t;

 
char *canonicalize_filename_mode (const char *, canonicalize_mode_t)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;

#ifdef __cplusplus
}
#endif

#endif  
