 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <stdlib.h>
# include <sys/types.h>

struct mode_change *mode_compile (const char *)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
struct mode_change *mode_create_from_ref (const char *)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
mode_t mode_adjust (mode_t, bool, mode_t, struct mode_change const *,
                    mode_t *)
  _GL_ATTRIBUTE_PURE;

#endif
