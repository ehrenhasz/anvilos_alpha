 

#include <config.h>

#include "acl.h"

#include "acl-internal.h"

#if USE_ACL
# if ! defined HAVE_ACL_FROM_MODE && defined HAVE_ACL_FROM_TEXT  
#  if HAVE_ACL_GET_FILE && !HAVE_ACL_TYPE_EXTENDED

static acl_t
acl_from_mode (mode_t mode)
{
#  if HAVE_ACL_FREE_TEXT  
  char acl_text[] = "u::---,g::---,o::---,";
#  else  
  char acl_text[] = "u::---,g::---,o::---";
#  endif

  if (mode & S_IRUSR) acl_text[ 3] = 'r';
  if (mode & S_IWUSR) acl_text[ 4] = 'w';
  if (mode & S_IXUSR) acl_text[ 5] = 'x';
  if (mode & S_IRGRP) acl_text[10] = 'r';
  if (mode & S_IWGRP) acl_text[11] = 'w';
  if (mode & S_IXGRP) acl_text[12] = 'x';
  if (mode & S_IROTH) acl_text[17] = 'r';
  if (mode & S_IWOTH) acl_text[18] = 'w';
  if (mode & S_IXOTH) acl_text[19] = 'x';

  return acl_from_text (acl_text);
}
#  endif
# endif

# if HAVE_FACL && defined GETACL  
static int
set_acls_from_mode (const char *name, int desc, mode_t mode, bool *must_chmod)
{
#  ifdef ACE_GETACL
   

   
  int convention;

  {
     
    enum
      {
        alloc_init = 4000 / sizeof (ace_t),  
        alloc_max = MIN (INT_MAX, SIZE_MAX / sizeof (ace_t))
      };
    ace_t buf[alloc_init];
    size_t alloc = alloc_init;
    ace_t *entries = buf;
    ace_t *malloced = NULL;
    int count;

    for (;;)
      {
        count = (desc != -1
                 ? facl (desc, ACE_GETACL, alloc, entries)
                 : acl (name, ACE_GETACL, alloc, entries));
        if (count < 0 && errno == ENOSPC)
          {
             
            free (malloced);
            if (alloc > alloc_max / 2)
              {
                errno = ENOMEM;
                return -1;
              }
            alloc = 2 * alloc;  
            entries = malloced = (ace_t *) malloc (alloc * sizeof (ace_t));
            if (entries == NULL)
              {
                errno = ENOMEM;
                return -1;
              }
            continue;
          }
        break;
      }

    if (count <= 0)
      convention = -1;
    else
      {
        int i;

        convention = 0;
        for (i = 0; i < count; i++)
          if (entries[i].a_flags & (OLD_ACE_OWNER | OLD_ACE_GROUP | OLD_ACE_OTHER))
            {
              convention = 1;
              break;
            }
      }
    free (malloced);
  }

  if (convention >= 0)
    {
      ace_t entries[6];
      int count;
      int ret;

      if (convention)
        {
           
          entries[0].a_type = OLD_ALLOW;
          entries[0].a_flags = OLD_ACE_OWNER;
          entries[0].a_who = 0;  
          entries[0].a_access_mask = (mode >> 6) & 7;
          entries[1].a_type = OLD_ALLOW;
          entries[1].a_flags = OLD_ACE_GROUP;
          entries[1].a_who = 0;  
          entries[1].a_access_mask = (mode >> 3) & 7;
          entries[2].a_type = OLD_ALLOW;
          entries[2].a_flags = OLD_ACE_OTHER;
          entries[2].a_who = 0;
          entries[2].a_access_mask = mode & 7;
          count = 3;
        }
      else
        {
           
          entries[0].a_type = NEW_ACE_ACCESS_DENIED_ACE_TYPE;
          entries[0].a_flags = NEW_ACE_OWNER;
          entries[0].a_who = 0;  
          entries[0].a_access_mask = 0;
          entries[1].a_type = NEW_ACE_ACCESS_ALLOWED_ACE_TYPE;
          entries[1].a_flags = NEW_ACE_OWNER;
          entries[1].a_who = 0;  
          entries[1].a_access_mask = NEW_ACE_WRITE_NAMED_ATTRS
                                     | NEW_ACE_WRITE_ATTRIBUTES
                                     | NEW_ACE_WRITE_ACL
                                     | NEW_ACE_WRITE_OWNER;
          if (mode & 0400)
            entries[1].a_access_mask |= NEW_ACE_READ_DATA;
          else
            entries[0].a_access_mask |= NEW_ACE_READ_DATA;
          if (mode & 0200)
            entries[1].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          else
            entries[0].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          if (mode & 0100)
            entries[1].a_access_mask |= NEW_ACE_EXECUTE;
          else
            entries[0].a_access_mask |= NEW_ACE_EXECUTE;
          entries[2].a_type = NEW_ACE_ACCESS_DENIED_ACE_TYPE;
          entries[2].a_flags = NEW_ACE_GROUP | NEW_ACE_IDENTIFIER_GROUP;
          entries[2].a_who = 0;  
          entries[2].a_access_mask = 0;
          entries[3].a_type = NEW_ACE_ACCESS_ALLOWED_ACE_TYPE;
          entries[3].a_flags = NEW_ACE_GROUP | NEW_ACE_IDENTIFIER_GROUP;
          entries[3].a_who = 0;  
          entries[3].a_access_mask = 0;
          if (mode & 0040)
            entries[3].a_access_mask |= NEW_ACE_READ_DATA;
          else
            entries[2].a_access_mask |= NEW_ACE_READ_DATA;
          if (mode & 0020)
            entries[3].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          else
            entries[2].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          if (mode & 0010)
            entries[3].a_access_mask |= NEW_ACE_EXECUTE;
          else
            entries[2].a_access_mask |= NEW_ACE_EXECUTE;
          entries[4].a_type = NEW_ACE_ACCESS_DENIED_ACE_TYPE;
          entries[4].a_flags = NEW_ACE_EVERYONE;
          entries[4].a_who = 0;
          entries[4].a_access_mask = NEW_ACE_WRITE_NAMED_ATTRS
                                     | NEW_ACE_WRITE_ATTRIBUTES
                                     | NEW_ACE_WRITE_ACL
                                     | NEW_ACE_WRITE_OWNER;
          entries[5].a_type = NEW_ACE_ACCESS_ALLOWED_ACE_TYPE;
          entries[5].a_flags = NEW_ACE_EVERYONE;
          entries[5].a_who = 0;
          entries[5].a_access_mask = NEW_ACE_READ_NAMED_ATTRS
                                     | NEW_ACE_READ_ATTRIBUTES
                                     | NEW_ACE_READ_ACL
                                     | NEW_ACE_SYNCHRONIZE;
          if (mode & 0004)
            entries[5].a_access_mask |= NEW_ACE_READ_DATA;
          else
            entries[4].a_access_mask |= NEW_ACE_READ_DATA;
          if (mode & 0002)
            entries[5].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          else
            entries[4].a_access_mask |= NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA;
          if (mode & 0001)
            entries[5].a_access_mask |= NEW_ACE_EXECUTE;
          else
            entries[4].a_access_mask |= NEW_ACE_EXECUTE;
          count = 6;
        }
      if (desc != -1)
        ret = facl (desc, ACE_SETACL, count, entries);
      else
        ret = acl (name, ACE_SETACL, count, entries);
      if (ret < 0 && errno != EINVAL && errno != ENOTSUP)
        {
          if (errno == ENOSYS)
            {
              *must_chmod = true;
              return 0;
            }
          return -1;
        }
      if (ret == 0)
        return 0;
    }
#  endif

  {
    aclent_t entries[3];
    int ret;

    entries[0].a_type = USER_OBJ;
    entries[0].a_id = 0;  
    entries[0].a_perm = (mode >> 6) & 7;
    entries[1].a_type = GROUP_OBJ;
    entries[1].a_id = 0;  
    entries[1].a_perm = (mode >> 3) & 7;
    entries[2].a_type = OTHER_OBJ;
    entries[2].a_id = 0;
    entries[2].a_perm = mode & 7;

    if (desc != -1)
      ret = facl (desc, SETACL,
                  sizeof (entries) / sizeof (aclent_t), entries);
    else
      ret = acl (name, SETACL,
                 sizeof (entries) / sizeof (aclent_t), entries);
    if (ret < 0)
      {
        if (errno == ENOSYS || errno == EOPNOTSUPP)
          {
            *must_chmod = true;
            return 0;
          }
        return -1;
      }
    return 0;
  }
}

# elif HAVE_GETACL  
static int
context_acl_from_mode (struct permission_context *ctx, const char *name, int desc)
{
  struct stat statbuf;
  int ret;

  if (desc != -1)
    ret = fstat (desc, &statbuf);
  else
    ret = stat (name, &statbuf);
  if (ret < 0)
    return -1;

  ctx->entries[0].uid = statbuf.st_uid;
  ctx->entries[0].gid = ACL_NSGROUP;
  ctx->entries[0].mode = (ctx->mode >> 6) & 7;
  ctx->entries[1].uid = ACL_NSUSER;
  ctx->entries[1].gid = statbuf.st_gid;
  ctx->entries[1].mode = (ctx->mode >> 3) & 7;
  ctx->entries[2].uid = ACL_NSUSER;
  ctx->entries[2].gid = ACL_NSGROUP;
  ctx->entries[2].mode = ctx->mode & 7;
  ctx->count = 3;
  return 0;
}

#  if HAVE_ACLV_H  
static int
context_aclv_from_mode (struct permission_context *ctx)
{
  int ret;

  ctx->aclv_entries[0].a_type = USER_OBJ;
  ctx->aclv_entries[0].a_id = 0;  
  ctx->aclv_entries[0].a_perm = (ctx->mode >> 6) & 7;
  ctx->aclv_entries[1].a_type = GROUP_OBJ;
  ctx->aclv_entries[1].a_id = 0;  
  ctx->aclv_entries[1].a_perm = (ctx->mode >> 3) & 7;
  ctx->aclv_entries[2].a_type = CLASS_OBJ;
  ctx->aclv_entries[2].a_id = 0;
  ctx->aclv_entries[2].a_perm = (ctx->mode >> 3) & 7;
  ctx->aclv_entries[3].a_type = OTHER_OBJ;
  ctx->aclv_entries[3].a_id = 0;
  ctx->aclv_entries[3].a_perm = ctx->mode & 7;
  ctx->aclv_count = 4;

  ret = aclsort (ctx->aclv_count, 1, ctx->aclv_entries);
  if (ret > 0)
    abort ();
  return ret;
}
#  endif

# elif HAVE_ACLX_GET && defined ACL_AIX_WIP  
static int
set_acls_from_mode (const char *name, int desc, mode_t mode, bool *must_chmod)
{
  acl_type_list_t types;
  size_t types_size = sizeof (types);
  acl_type_t type;

  if (aclx_gettypes (name, &types, &types_size) < 0
      || types.num_entries == 0)
    {
      *must_chmod = true;
      return 0;
    }

   
  type = types.entries[0];
  if (type.u64 == ACL_AIXC)
    {
      union { struct acl a; char room[128]; } u;
      int ret;

      u.a.acl_len = (char *) &u.a.acl_ext[0] - (char *) &u.a;  
      u.a.acl_mode = mode & ~(S_IXACL | 0777);
      u.a.u_access = (mode >> 6) & 7;
      u.a.g_access = (mode >> 3) & 7;
      u.a.o_access = mode & 7;

      if (desc != -1)
        ret = aclx_fput (desc, SET_ACL | SET_MODE_S_BITS,
                         type, &u.a, u.a.acl_len, mode);
      else
        ret = aclx_put (name, SET_ACL | SET_MODE_S_BITS,
                        type, &u.a, u.a.acl_len, mode);
      if (!(ret < 0 && errno == ENOSYS))
        return ret;
    }
  else if (type.u64 == ACL_NFS4)
    {
      union { nfs4_acl_int_t a; char room[128]; } u;
      nfs4_ace_int_t *ace;
      int ret;

      u.a.aclVersion = NFS4_ACL_INT_STRUCT_VERSION;
      u.a.aclEntryN = 0;
      ace = &u.a.aclEntry[0];
      {
        ace->flags = ACE4_ID_SPECIAL;
        ace->aceWho.special_whoid = ACE4_WHO_OWNER;
        ace->aceType = ACE4_ACCESS_ALLOWED_ACE_TYPE;
        ace->aceFlags = 0;
        ace->aceMask =
          (mode & 0400 ? ACE4_READ_DATA | ACE4_LIST_DIRECTORY : 0)
          | (mode & 0200
             ? ACE4_WRITE_DATA | ACE4_ADD_FILE | ACE4_APPEND_DATA
               | ACE4_ADD_SUBDIRECTORY
             : 0)
          | (mode & 0100 ? ACE4_EXECUTE : 0);
        ace->aceWhoString[0] = '\0';
        ace->entryLen = (char *) &ace->aceWhoString[4] - (char *) ace;
        ace = (nfs4_ace_int_t *) (char *) &ace->aceWhoString[4];
        u.a.aclEntryN++;
      }
      {
        ace->flags = ACE4_ID_SPECIAL;
        ace->aceWho.special_whoid = ACE4_WHO_GROUP;
        ace->aceType = ACE4_ACCESS_ALLOWED_ACE_TYPE;
        ace->aceFlags = 0;
        ace->aceMask =
          (mode & 0040 ? ACE4_READ_DATA | ACE4_LIST_DIRECTORY : 0)
          | (mode & 0020
             ? ACE4_WRITE_DATA | ACE4_ADD_FILE | ACE4_APPEND_DATA
               | ACE4_ADD_SUBDIRECTORY
             : 0)
          | (mode & 0010 ? ACE4_EXECUTE : 0);
        ace->aceWhoString[0] = '\0';
        ace->entryLen = (char *) &ace->aceWhoString[4] - (char *) ace;
        ace = (nfs4_ace_int_t *) (char *) &ace->aceWhoString[4];
        u.a.aclEntryN++;
      }
      {
        ace->flags = ACE4_ID_SPECIAL;
        ace->aceWho.special_whoid = ACE4_WHO_EVERYONE;
        ace->aceType = ACE4_ACCESS_ALLOWED_ACE_TYPE;
        ace->aceFlags = 0;
        ace->aceMask =
          (mode & 0004 ? ACE4_READ_DATA | ACE4_LIST_DIRECTORY : 0)
          | (mode & 0002
             ? ACE4_WRITE_DATA | ACE4_ADD_FILE | ACE4_APPEND_DATA
               | ACE4_ADD_SUBDIRECTORY
             : 0)
          | (mode & 0001 ? ACE4_EXECUTE : 0);
        ace->aceWhoString[0] = '\0';
        ace->entryLen = (char *) &ace->aceWhoString[4] - (char *) ace;
        ace = (nfs4_ace_int_t *) (char *) &ace->aceWhoString[4];
        u.a.aclEntryN++;
      }
      u.a.aclLength = (char *) ace - (char *) &u.a;

      if (desc != -1)
        ret = aclx_fput (desc, SET_ACL | SET_MODE_S_BITS,
                         type, &u.a, u.a.aclLength, mode);
      else
        ret = aclx_put (name, SET_ACL | SET_MODE_S_BITS,
                        type, &u.a, u.a.aclLength, mode);
      if (!(ret < 0 && errno == ENOSYS))
        return ret;
    }

  *must_chmod = true;
  return 0;
}

# elif HAVE_STATACL  
static int
context_acl_from_mode (struct permission_context *ctx)
{
  ctx->u.a.acl_len = (char *) &ctx->u.a.acl_ext[0] - (char *) &ctx->u.a;  
  ctx->u.a.acl_mode = ctx->mode & ~(S_IXACL | 0777);
  ctx->u.a.u_access = (ctx->mode >> 6) & 7;
  ctx->u.a.g_access = (ctx->mode >> 3) & 7;
  ctx->u.a.o_access = ctx->mode & 7;
  ctx->have_u = true;
  return 0;
}

# elif HAVE_ACLSORT  
static int
context_acl_from_mode (struct permission_context *ctx)
{
  int ret;

  ctx->entries[0].a_type = USER_OBJ;
  ctx->entries[0].a_id = 0;  
  ctx->entries[0].a_perm = (ctx->mode >> 6) & 7;
  ctx->entries[1].a_type = GROUP_OBJ;
  ctx->entries[1].a_id = 0;  
  ctx->entries[1].a_perm = (ctx->mode >> 3) & 7;
  ctx->entries[2].a_type = CLASS_OBJ;
  ctx->entries[2].a_id = 0;
  ctx->entries[2].a_perm = (ctx->mode >> 3) & 7;
  ctx->entries[3].a_type = OTHER_OBJ;
  ctx->entries[3].a_id = 0;
  ctx->entries[3].a_perm = ctx->mode & 7;
  ctx->count = 4;

  ret = aclsort (ctx->count, 1, entries);
  if (ret > 0)
    abort ();
  return ret;
}
# endif

static int
set_acls (struct permission_context *ctx, const char *name, int desc,
          int from_mode, bool *must_chmod, bool *acls_set)
{
  int ret = 0;

# if HAVE_ACL_GET_FILE
   
   
#  if !HAVE_ACL_TYPE_EXTENDED
   

#   ifndef HAVE_ACL_FROM_TEXT
#    error Must have acl_from_text (see POSIX 1003.1e draft 17).
#   endif
#   ifndef HAVE_ACL_DELETE_DEF_FILE
#    error Must have acl_delete_def_file (see POSIX 1003.1e draft 17).
#   endif

  if (! ctx->acls_not_supported)
    {
      if (ret == 0 && from_mode)
        {
          if (ctx->acl)
            acl_free (ctx->acl);
          ctx->acl = acl_from_mode (ctx->mode);
          if (ctx->acl == NULL)
            ret = -1;
        }

      if (ret == 0 && ctx->acl)
        {
          if (HAVE_ACL_SET_FD && desc != -1)
            ret = acl_set_fd (desc, ctx->acl);
          else
            ret = acl_set_file (name, ACL_TYPE_ACCESS, ctx->acl);
          if (ret != 0)
            {
              if (! acl_errno_valid (errno))
                {
                  ctx->acls_not_supported = true;
                  if (from_mode || acl_access_nontrivial (ctx->acl) == 0)
                    ret = 0;
                }
            }
          else
            {
              *acls_set = true;
              if (S_ISDIR(ctx->mode))
                {
                  if (! from_mode && ctx->default_acl &&
                      acl_default_nontrivial (ctx->default_acl))
                    ret = acl_set_file (name, ACL_TYPE_DEFAULT,
                                        ctx->default_acl);
                  else
                    ret = acl_delete_def_file (name);
                }
            }
        }
    }

#   if HAVE_ACL_TYPE_NFS4   

   

   

#   endif

#  else  
   

   

  if (ctx->acl == NULL)
    {
      acl_t acl;

       
      if (HAVE_ACL_GET_FD && desc != -1)
        acl = acl_get_fd (desc);
      else
        acl = acl_get_file (name, ACL_TYPE_EXTENDED);
      if (acl)
        {
          acl_free (acl);

          acl = acl_init (0);
          if (acl)
            {
              if (HAVE_ACL_SET_FD && desc != -1)
                ret = acl_set_fd (desc, acl);
              else
                ret = acl_set_file (name, ACL_TYPE_EXTENDED, acl);
              acl_free (acl);
            }
          else
            ret = -1;
        }
    }
  else
    {
      if (HAVE_ACL_SET_FD && desc != -1)
        ret = acl_set_fd (desc, ctx->acl);
      else
        ret = acl_set_file (name, ACL_TYPE_EXTENDED, ctx->acl);
      if (ret != 0)
        {
          if (! acl_errno_valid (errno)
              && ! acl_extended_nontrivial (ctx->acl))
            ret = 0;
        }
    }
  *acls_set = true;

#  endif

# elif defined GETACL  

   

   

  if (from_mode)
    return set_acls_from_mode (name, desc, ctx->mode, must_chmod);

  if (ret == 0 && ctx->count)
    {
      if (desc != -1)
        ret = facl (desc, SETACL, ctx->count, ctx->entries);
      else
        ret = acl (name, SETACL, ctx->count, ctx->entries);
      if (ret < 0)
        {
          if ((errno == ENOSYS || errno == EOPNOTSUPP || errno == EINVAL)
              && acl_nontrivial (ctx->count, ctx->entries) == 0)
            ret = 0;
        }
      else
        *acls_set = true;
    }

#  ifdef ACE_GETACL
  if (ret == 0 && ctx->ace_count)
    {
      if (desc != -1)
        ret = facl (desc, ACE_SETACL, ctx->ace_count, ctx->ace_entries);
      else
        ret = acl (name, ACE_SETACL, ctx->ace_count, ctx->ace_entries);
      if (ret < 0)
        {
          if ((errno == ENOSYS || errno == EINVAL || errno == ENOTSUP)
              && acl_ace_nontrivial (ctx->ace_count, ctx->ace_entries) == 0)
            ret = 0;
        }
      else
        *acls_set = true;
    }
#  endif

# elif HAVE_GETACL  

  if (from_mode)
    ret = context_acl_from_mode (ctx, name, desc);

  if (ret == 0 && ctx->count > 0)
    {
      if (desc != -1)
        ret = fsetacl (desc, ctx->count, ctx->entries);
      else
        ret = setacl (name, ctx->count, ctx->entries);
      if (ret < 0)
        {
          if ((errno == ENOSYS || errno == EOPNOTSUPP || errno == ENOTSUP)
              && (from_mode || !acl_nontrivial (ctx->count, ctx->entries)))
            ret = 0;
        }
      else
        *acls_set = true;
    }

#  if HAVE_ACLV_H
  if (from_mode)
    ret = context_aclv_from_mode (ctx);

  if (ret == 0 && ctx->aclv_count > 0)
    {
      ret = acl ((char *) name, ACL_SET, ctx->aclv_count, ctx->aclv_entries);
      if (ret < 0)
        {
          if ((errno == ENOSYS || errno == EOPNOTSUPP || errno == EINVAL)
              && (from_mode || !aclv_nontrivial (ctx->aclv_count, ctx->aclv_entries)))
            ret = 0;
        }
      else
        *acls_set = true;
    }
#  endif

# elif HAVE_ACLX_GET && ACL_AIX_WIP  

   

  if (from_mode)
    ret = set_acls_from_mode (name, desc, mode, must_chmod);

# elif HAVE_STATACL  

  if (from_mode)
    ret = context_acl_from_mode (ctx);

  if (ret == 0 && ctx->have_u)
    {
      if (desc != -1)
        ret = fchacl (desc, &ctx->u.a, ctx->u.a.acl_len);
      else
        ret = chacl ((char *) name, &ctx->u.a, ctx->u.a.acl_len);
      if (ret < 0)
        {
          if (errno == ENOSYS && from_mode)
            ret = 0;
        }
      else
        *acls_set = true;
    }

# elif HAVE_ACLSORT  

  if (from_mode)
    ret = context_acl_from_mode (ctx);

  if (ret == 0 && ctx->count)
    {
      ret = acl ((char *) name, ACL_SET, ctx->count, ctx->entries);
      if (ret != 0)
        {
          if (!acl_nontrivial (ctx->count, ctx->entries))
            ret = 0;
        }
      else
        *acls_set = true;
    }

# else   

   

# endif

  return ret;
}
#endif

 

int
chmod_or_fchmod (const char *name, int desc, mode_t mode)
{
  if (HAVE_FCHMOD && desc != -1)
    return fchmod (desc, mode);
  else
    return chmod (name, mode);
}

 

int
set_permissions (struct permission_context *ctx, const char *name, int desc)
{
  _GL_UNUSED bool acls_set = false;
  bool early_chmod;
  bool must_chmod = false;
  int ret = 0;

#if USE_ACL
# if HAVE_STATACL
   
   

  early_chmod = false;
# else
   
   

  early_chmod = (! MODE_INSIDE_ACL || (ctx->mode & (S_ISUID | S_ISGID | S_ISVTX)));
# endif
#else
   

  early_chmod = true;
#endif

  if (early_chmod)
    {
      ret = chmod_or_fchmod (name, desc, ctx->mode);
      if (ret != 0)
        return -1;
    }

#if USE_ACL
  ret = set_acls (ctx, name, desc, false, &must_chmod, &acls_set);
  if (! acls_set)
    {
      int saved_errno = ret ? errno : 0;

       

      ret = set_acls (ctx, name, desc, true, &must_chmod, &acls_set);
      if (! acls_set)
        must_chmod = true;

      if (saved_errno)
        {
          errno = saved_errno;
          ret = -1;
        }
    }
#endif

  if (must_chmod && ! early_chmod)
    {
      int saved_errno = ret ? errno : 0;

      ret = chmod_or_fchmod (name, desc, ctx->mode);

      if (saved_errno)
        {
          errno = saved_errno;
          ret = -1;
        }
    }

  return ret;
}
