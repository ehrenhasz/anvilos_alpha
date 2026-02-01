 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stddef.h>

#ifndef DOUBLE_SLASH_IS_DISTINCT_ROOT
# define DOUBLE_SLASH_IS_DISTINCT_ROOT 0
#endif

#ifdef __cplusplus
extern "C" {
#endif


 

 
extern char *last_component (char const *filename) _GL_ATTRIBUTE_PURE;

 
extern size_t base_len (char const *filename) _GL_ATTRIBUTE_PURE;


#ifdef __cplusplus
}  
#endif

#endif  
