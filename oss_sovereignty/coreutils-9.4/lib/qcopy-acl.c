 

#include <config.h>

#include "acl.h"

#include "acl-internal.h"

#if USE_XATTR

# include <attr/libattr.h>

 

static int
is_attr_permissions (const char *name, struct error_context *ctx)
{
  return attr_copy_action (name, ctx) == ATTR_ACTION_PERMISSIONS;
}

#endif   

 

int
qcopy_acl (const char *src_name, int source_desc, const char *dst_name,
           int dest_desc, mode_t mode)
{
  int ret;

#ifdef USE_XATTR
   
  ret = chmod_or_fchmod (dst_name, dest_desc, mode);
   
  if (ret == 0)
    ret = source_desc <= 0 || dest_desc <= 0
      ? attr_copy_file (src_name, dst_name, is_attr_permissions, NULL)
      : attr_copy_fd (src_name, source_desc, dst_name, dest_desc,
                      is_attr_permissions, NULL);
#else
   
  struct permission_context ctx;

  ret = get_permissions (src_name, source_desc, mode, &ctx);
  if (ret != 0)
    return -2;
  ret = set_permissions (&ctx, dst_name, dest_desc);
  free_permission_context (&ctx);
#endif
  return ret;
}
