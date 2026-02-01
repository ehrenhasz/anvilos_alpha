 

 

#include <config.h>

 
#include "areadlink.h"

#include <stdlib.h>

#include "careadlinkat.h"

#if HAVE_READLINKAT

 

char *
areadlinkat (int fd, char const *filename)
{
  return careadlinkat (fd, filename, NULL, 0, NULL, readlinkat);
}

#else  

 

# define AT_FUNC_NAME areadlinkat
# define AT_FUNC_F1 areadlink
# define AT_FUNC_POST_FILE_PARAM_DECLS  
# define AT_FUNC_POST_FILE_ARGS         
# define AT_FUNC_RESULT char *
# define AT_FUNC_FAIL NULL
# include "at-func.c"
# undef AT_FUNC_NAME
# undef AT_FUNC_F1
# undef AT_FUNC_POST_FILE_PARAM_DECLS
# undef AT_FUNC_POST_FILE_ARGS
# undef AT_FUNC_RESULT
# undef AT_FUNC_FAIL

#endif  
