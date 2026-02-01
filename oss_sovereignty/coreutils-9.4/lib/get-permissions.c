 

#include <config.h>

#include <string.h>
#include "acl.h"

#include "acl-internal.h"

 

int
get_permissions (const char *name, int desc, mode_t mode,
                 struct permission_context *ctx)
{
  memset (ctx, 0, sizeof *ctx);
  ctx->mode = mode;

#if USE_ACL && HAVE_ACL_GET_FILE
   
   
# if !HAVE_ACL_TYPE_EXTENDED
   

  if (HAVE_ACL_GET_FD && desc != -1)
    ctx->acl = acl_get_fd (desc);
  else
    ctx->acl = acl_get_file (name, ACL_TYPE_ACCESS);
  if (ctx->acl == NULL)
    return acl_errno_valid (errno) ? -1 : 0;

   

  if (S_ISDIR (mode))
    {
      ctx->default_acl = acl_get_file (name, ACL_TYPE_DEFAULT);
      if (ctx->default_acl == NULL)
        return -1;
    }

#  if HAVE_ACL_TYPE_NFS4   

   

#  endif

# else  
   

   

  if (HAVE_ACL_GET_FD && desc != -1)
    ctx->acl = acl_get_fd (desc);
  else
    ctx->acl = acl_get_file (name, ACL_TYPE_EXTENDED);
  if (ctx->acl == NULL)
    return acl_errno_valid (errno) ? -1 : 0;

# endif

#elif USE_ACL && defined GETACL  

   
# ifdef ACE_GETACL
   
  for (;;)
    {
      int ret;

      if (desc != -1)
        ret = facl (desc, ACE_GETACLCNT, 0, NULL);
      else
        ret = acl (name, ACE_GETACLCNT, 0, NULL);
      if (ret < 0)
        {
          if (errno == ENOSYS || errno == EINVAL)
            ret = 0;
          else
            return -1;
        }
      ctx->ace_count = ret;

      if (ctx->ace_count == 0)
        break;

      ctx->ace_entries = (ace_t *) malloc (ctx->ace_count * sizeof (ace_t));
      if (ctx->ace_entries == NULL)
        {
          errno = ENOMEM;
          return -1;
        }

      if (desc != -1)
        ret = facl (desc, ACE_GETACL, ctx->ace_count, ctx->ace_entries);
      else
        ret = acl (name, ACE_GETACL, ctx->ace_count, ctx->ace_entries);
      if (ret < 0)
        {
          if (errno == ENOSYS || errno == EINVAL)
            {
              free (ctx->ace_entries);
              ctx->ace_entries = NULL;
              ctx->ace_count = 0;
              break;
            }
          else
            return -1;
        }
      if (ret <= ctx->ace_count)
        {
          ctx->ace_count = ret;
          break;
        }
       
      free (ctx->ace_entries);
      ctx->ace_entries = NULL;
    }
# endif

  for (;;)
    {
      int ret;

      if (desc != -1)
        ret = facl (desc, GETACLCNT, 0, NULL);
      else
        ret = acl (name, GETACLCNT, 0, NULL);
      if (ret < 0)
        {
          if (errno == ENOSYS || errno == ENOTSUP || errno == EOPNOTSUPP)
            ret = 0;
          else
            return -1;
        }
      ctx->count = ret;

      if (ctx->count == 0)
        break;

      ctx->entries = (aclent_t *) malloc (ctx->count * sizeof (aclent_t));
      if (ctx->entries == NULL)
        {
          errno = ENOMEM;
          return -1;
        }

      if (desc != -1)
        ret = facl (desc, GETACL, ctx->count, ctx->entries);
      else
        ret = acl (name, GETACL, ctx->count, ctx->entries);
      if (ret < 0)
        {
          if (errno == ENOSYS || errno == ENOTSUP || errno == EOPNOTSUPP)
            {
              free (ctx->entries);
              ctx->entries = NULL;
              ctx->count = 0;
              break;
            }
          else
            return -1;
        }
      if (ret <= ctx->count)
        {
          ctx->count = ret;
          break;
        }
       
      free (ctx->entries);
      ctx->entries = NULL;
    }

#elif USE_ACL && HAVE_GETACL  

  {
    int ret;

    if (desc != -1)
      ret = fgetacl (desc, NACLENTRIES, ctx->entries);
    else
      ret = getacl (name, NACLENTRIES, ctx->entries);
    if (ret < 0)
      {
        if (errno == ENOSYS || errno == EOPNOTSUPP || errno == ENOTSUP)
          ret = 0;
        else
          return -1;
      }
    else if (ret > NACLENTRIES)
       
      abort ();
    ctx->count = ret;

# if HAVE_ACLV_H
    ret = acl ((char *) name, ACL_GET, NACLVENTRIES, ctx->aclv_entries);
    if (ret < 0)
      {
        if (errno == ENOSYS || errno == EOPNOTSUPP || errno == EINVAL)
          ret = 0;
        else
          return -2;
      }
    else if (ret > NACLVENTRIES)
       
      abort ();
    ctx->aclv_count = ret;
# endif
  }

#elif USE_ACL && HAVE_ACLX_GET && ACL_AIX_WIP  

   

#elif USE_ACL && HAVE_STATACL  

  {
    int ret;
    if (desc != -1)
      ret = fstatacl (desc, STX_NORMAL, &ctx->u.a, sizeof ctx->u);
    else
      ret = statacl ((char *) name, STX_NORMAL, &ctx->u.a, sizeof ctx->u);
    if (ret == 0)
      ctx->have_u = true;
  }

#elif USE_ACL && HAVE_ACLSORT  

  {
    int ret = acl ((char *) name, ACL_GET, NACLENTRIES, ctx->entries);
    if (ret < 0)
      return -1;
    else if (ret > NACLENTRIES)
       
      abort ();
    ctx->count = ret;
  }

#endif

  return 0;

}
