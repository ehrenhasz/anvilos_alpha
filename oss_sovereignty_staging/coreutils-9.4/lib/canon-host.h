 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <stdlib.h>

char *canon_host (char const *host)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
char *canon_host_r (char const *host, int *cherror)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;

const char *ch_strerror (void);
# define ch_strerror_r(cherror) gai_strerror (cherror);

#endif  
