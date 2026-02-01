 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <sys/types.h>
# include <sys/stat.h>

 
# if HAVE_DECL_STRMODE
#  include <string.h>  
#  include <unistd.h>  
# endif

# ifdef __cplusplus
extern "C" {
# endif

# if !HAVE_DECL_STRMODE
extern void strmode (mode_t mode, char *str);
# endif

extern void filemodestring (struct stat const *statp, char *str);

# ifdef __cplusplus
}
# endif

#endif
