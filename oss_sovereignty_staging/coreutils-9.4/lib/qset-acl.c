 

#include <config.h>

#define ACL_INTERNAL_INLINE _GL_EXTERN_INLINE

#include <string.h>
#include "acl.h"

#include "acl-internal.h"

 

int
qset_acl (char const *name, int desc, mode_t mode)
{
  struct permission_context ctx;
  int ret;

  memset (&ctx, 0, sizeof ctx);
  ctx.mode = mode;
  ret = set_permissions (&ctx, name, desc);
  free_permission_context (&ctx);
  return ret;
}
