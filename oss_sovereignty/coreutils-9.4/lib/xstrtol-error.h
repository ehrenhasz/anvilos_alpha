 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include "xstrtol.h"

# include <getopt.h>

 

_Noreturn void xstrtol_fatal (enum strtol_error  ,
                              int  , char  ,
                              struct option const *  ,
                              char const *  );

#endif  
