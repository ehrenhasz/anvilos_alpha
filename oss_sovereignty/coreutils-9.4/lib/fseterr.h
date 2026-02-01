 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdio.h>

 

#if HAVE___FSETERR  

# include <stdio_ext.h>
# define fseterr(fp) __fseterr (fp)

#else

# ifdef __cplusplus
extern "C" {
# endif

extern void fseterr (FILE *fp);

# ifdef __cplusplus
}
# endif

#endif

#endif  
