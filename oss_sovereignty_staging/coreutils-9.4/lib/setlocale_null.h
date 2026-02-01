 

#ifndef _SETLOCALE_NULL_H
#define _SETLOCALE_NULL_H

#include <stddef.h>

#include "arg-nonnull.h"


#ifdef __cplusplus
extern "C" {
#endif


 
#define SETLOCALE_NULL_MAX (256+1)

 
#define SETLOCALE_NULL_ALL_MAX (148+12*256+1)

 
extern int setlocale_null_r (int category, char *buf, size_t bufsize)
  _GL_ARG_NONNULL ((2));

 
extern const char *setlocale_null (int category);


#ifdef __cplusplus
}
#endif

#endif  
