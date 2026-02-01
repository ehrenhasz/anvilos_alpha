 

#include <config.h>

#include "acl.h"

#include "acl-internal.h"

#if USE_ACL && HAVE_ACL_GET_FILE  

# if HAVE_ACL_TYPE_EXTENDED  

 
int
acl_extended_nontrivial (acl_t acl)
{
   
  return (acl_entries (acl) > 0);
}

# else  

 
int
acl_access_nontrivial (acl_t acl)
{
   
#  if HAVE_ACL_FIRST_ENTRY  

  acl_entry_t ace;
  int got_one;

  for (got_one = acl_get_entry (acl, ACL_FIRST_ENTRY, &ace);
       got_one > 0;
       got_one = acl_get_entry (acl, ACL_NEXT_ENTRY, &ace))
    {
      acl_tag_t tag;
      if (acl_get_tag_type (ace, &tag) < 0)
        return -1;
      if (!(tag == ACL_USER_OBJ || tag == ACL_GROUP_OBJ || tag == ACL_OTHER))
        return 1;
    }
  return got_one;

#  elif HAVE_ACL_TO_SHORT_TEXT  
   

  int count = acl->acl_cnt;
  int i;

  for (i = 0; i < count; i++)
    {
      acl_entry_t ace = &acl->acl_entry[i];
      acl_tag_t tag = ace->ae_tag;

      if (!(tag == ACL_USER_OBJ || tag == ACL_GROUP_OBJ
            || tag == ACL_OTHER_OBJ))
        return 1;
    }
  return 0;

#  elif HAVE_ACL_FREE_TEXT  
   

  int count = acl->acl_num;
  acl_entry_t ace;

  for (ace = acl->acl_first; count > 0; ace = ace->next, count--)
    {
      acl_tag_t tag;
      acl_perm_t perm;

      tag = ace->entry->acl_type;
      if (!(tag == ACL_USER_OBJ || tag == ACL_GROUP_OBJ || tag == ACL_OTHER))
        return 1;

      perm = ace->entry->acl_perm;
       
      if ((perm & ~(ACL_READ | ACL_WRITE | ACL_EXECUTE)) != 0)
        return 1;
    }
  return 0;

#  else

  errno = ENOSYS;
  return -1;
#  endif
}

int
acl_default_nontrivial (acl_t acl)
{
   
  return (acl_entries (acl) > 0);
}

# endif

#elif USE_ACL && HAVE_FACL && defined GETACL  

 
int
acl_nontrivial (int count, aclent_t *entries)
{
  int i;

  for (i = 0; i < count; i++)
    {
      aclent_t *ace = &entries[i];

       
      if (!(ace->a_type == USER_OBJ
            || ace->a_type == GROUP_OBJ
            || ace->a_type == OTHER_OBJ
             
            || ace->a_type == CLASS_OBJ))
        return 1;
    }
  return 0;
}

# ifdef ACE_GETACL

 
#  define NEW_ACE_WRITEA_DATA (NEW_ACE_WRITE_DATA | NEW_ACE_APPEND_DATA)

 
int
acl_ace_nontrivial (int count, ace_t *entries)
{
  int i;

   
  int old_convention = 0;

  for (i = 0; i < count; i++)
    if (entries[i].a_flags & (OLD_ACE_OWNER | OLD_ACE_GROUP | OLD_ACE_OTHER))
      {
        old_convention = 1;
        break;
      }

  if (old_convention)
     
    for (i = 0; i < count; i++)
      {
        ace_t *ace = &entries[i];

         
        if (!(ace->a_type == OLD_ALLOW
              && (ace->a_flags == OLD_ACE_OWNER
                  || ace->a_flags == OLD_ACE_GROUP
                  || ace->a_flags == OLD_ACE_OTHER)))
          return 1;
      }
  else
    {
       
      unsigned int access_masks[6] =
        {
          0,  
          0,  
          0,  
          0,  
          0,  
          0   
        };

      for (i = 0; i < count; i++)
        {
          ace_t *ace = &entries[i];
          unsigned int index1;
          unsigned int index2;

          if (ace->a_type == NEW_ACE_ACCESS_ALLOWED_ACE_TYPE)
            index1 = 1;
          else if (ace->a_type == NEW_ACE_ACCESS_DENIED_ACE_TYPE)
            index1 = 0;
          else
            return 1;

          if (ace->a_flags == NEW_ACE_OWNER)
            index2 = 0;
          else if (ace->a_flags == (NEW_ACE_GROUP | NEW_ACE_IDENTIFIER_GROUP))
            index2 = 2;
          else if (ace->a_flags == NEW_ACE_EVERYONE)
            index2 = 4;
          else
            return 1;

          access_masks[index1 + index2] |= ace->a_access_mask;
        }

       
      if (access_masks[0] & access_masks[1])
        return 1;
      if (access_masks[2] & access_masks[3])
        return 1;
      if (access_masks[4] & access_masks[5])
        return 1;

       
      if ((NEW_ACE_WRITE_NAMED_ATTRS
           | NEW_ACE_WRITE_ATTRIBUTES
           | NEW_ACE_WRITE_ACL
           | NEW_ACE_WRITE_OWNER)
          & ~ access_masks[1])
        return 1;
      access_masks[1] &= ~(NEW_ACE_WRITE_NAMED_ATTRS
                           | NEW_ACE_WRITE_ATTRIBUTES
                           | NEW_ACE_WRITE_ACL
                           | NEW_ACE_WRITE_OWNER);
      if ((NEW_ACE_READ_NAMED_ATTRS
           | NEW_ACE_READ_ATTRIBUTES
           | NEW_ACE_READ_ACL
           | NEW_ACE_SYNCHRONIZE)
          & ~ access_masks[5])
        return 1;
      access_masks[5] &= ~(NEW_ACE_READ_NAMED_ATTRS
                           | NEW_ACE_READ_ATTRIBUTES
                           | NEW_ACE_READ_ACL
                           | NEW_ACE_SYNCHRONIZE);

       
      switch ((access_masks[0] | access_masks[1])
              & ~(NEW_ACE_READ_NAMED_ATTRS
                  | NEW_ACE_READ_ATTRIBUTES
                  | NEW_ACE_READ_ACL
                  | NEW_ACE_SYNCHRONIZE))
        {
        case 0:
        case NEW_ACE_READ_DATA:
        case                     NEW_ACE_WRITEA_DATA:
        case NEW_ACE_READ_DATA | NEW_ACE_WRITEA_DATA:
        case                                           NEW_ACE_EXECUTE:
        case NEW_ACE_READ_DATA |                       NEW_ACE_EXECUTE:
        case                     NEW_ACE_WRITEA_DATA | NEW_ACE_EXECUTE:
        case NEW_ACE_READ_DATA | NEW_ACE_WRITEA_DATA | NEW_ACE_EXECUTE:
          break;
        default:
          return 1;
        }
      switch ((access_masks[2] | access_masks[3])
              & ~(NEW_ACE_READ_NAMED_ATTRS
                  | NEW_ACE_READ_ATTRIBUTES
                  | NEW_ACE_READ_ACL
                  | NEW_ACE_SYNCHRONIZE))
        {
        case 0:
        case NEW_ACE_READ_DATA:
        case                     NEW_ACE_WRITEA_DATA:
        case NEW_ACE_READ_DATA | NEW_ACE_WRITEA_DATA:
        case                                           NEW_ACE_EXECUTE:
        case NEW_ACE_READ_DATA |                       NEW_ACE_EXECUTE:
        case                     NEW_ACE_WRITEA_DATA | NEW_ACE_EXECUTE:
        case NEW_ACE_READ_DATA | NEW_ACE_WRITEA_DATA | NEW_ACE_EXECUTE:
          break;
        default:
          return 1;
        }
      switch ((access_masks[4] | access_masks[5])
              & ~(NEW_ACE_WRITE_NAMED_ATTRS
                  | NEW_ACE_WRITE_ATTRIBUTES
                  | NEW_ACE_WRITE_ACL
                  | NEW_ACE_WRITE_OWNER))
        {
        case 0:
        case NEW_ACE_READ_DATA:
        case                     NEW_ACE_WRITEA_DATA:
        case NEW_ACE_READ_DATA | NEW_ACE_WRITEA_DATA:
        case                                           NEW_ACE_EXECUTE:
        case NEW_ACE_READ_DATA |                       NEW_ACE_EXECUTE:
        case                     NEW_ACE_WRITEA_DATA | NEW_ACE_EXECUTE:
        case NEW_ACE_READ_DATA | NEW_ACE_WRITEA_DATA | NEW_ACE_EXECUTE:
          break;
        default:
          return 1;
        }

       
      if (((access_masks[0] & NEW_ACE_WRITE_DATA) != 0)
          != ((access_masks[0] & NEW_ACE_APPEND_DATA) != 0))
        return 1;
      if (((access_masks[2] & NEW_ACE_WRITE_DATA) != 0)
          != ((access_masks[2] & NEW_ACE_APPEND_DATA) != 0))
        return 1;
      if (((access_masks[4] & NEW_ACE_WRITE_DATA) != 0)
          != ((access_masks[4] & NEW_ACE_APPEND_DATA) != 0))
        return 1;
    }

  return 0;
}

# endif

#elif USE_ACL && HAVE_GETACL  

 
int
acl_nontrivial (int count, struct acl_entry *entries)
{
  int i;

  if (count > 3)
    return 1;

  for (i = 0; i < count; i++)
    {
      struct acl_entry *ace = &entries[i];

      if (ace->uid != ACL_NSUSER && ace->gid != ACL_NSGROUP)
        return 1;
    }
  return 0;
}

# if HAVE_ACLV_H  

 
int
aclv_nontrivial (int count, struct acl *entries)
{
  int i;

  for (i = 0; i < count; i++)
    {
      struct acl *ace = &entries[i];

       
      if (!(ace->a_type == USER_OBJ  
            || ace->a_type == GROUP_OBJ  
            || ace->a_type == CLASS_OBJ
            || ace->a_type == OTHER_OBJ))
        return 1;
    }
  return 0;
}

# endif

#elif USE_ACL && (HAVE_ACLX_GET || HAVE_STATACL)  

 
int
acl_nontrivial (struct acl *a)
{
   
  return (acl_last (a) != a->acl_ext ? 1 : 0);
}

# if HAVE_ACLX_GET && defined ACL_AIX_WIP  

 
int
acl_nfs4_nontrivial (nfs4_acl_int_t *a)
{
#  if 1  
  return (a->aclEntryN > 0 ? 1 : 0);
#  else
  int count = a->aclEntryN;
  int i;

  for (i = 0; i < count; i++)
    {
      nfs4_ace_int_t *ace = &a->aclEntry[i];

      if (!((ace->flags & ACE4_ID_SPECIAL) != 0
            && (ace->aceWho.special_whoid == ACE4_WHO_OWNER
                || ace->aceWho.special_whoid == ACE4_WHO_GROUP
                || ace->aceWho.special_whoid == ACE4_WHO_EVERYONE)
            && ace->aceType == ACE4_ACCESS_ALLOWED_ACE_TYPE
            && ace->aceFlags == 0
            && (ace->aceMask & ~(ACE4_READ_DATA | ACE4_LIST_DIRECTORY
                                 | ACE4_WRITE_DATA | ACE4_ADD_FILE
                                 | ACE4_EXECUTE)) == 0))
        return 1;
    }
  return 0;
#  endif
}

# endif

#elif USE_ACL && HAVE_ACLSORT  

 
int
acl_nontrivial (int count, struct acl *entries)
{
  int i;

  for (i = 0; i < count; i++)
    {
      struct acl *ace = &entries[i];

       
      if (!(ace->a_type == USER_OBJ  
            || ace->a_type == GROUP_OBJ  
            || ace->a_type == CLASS_OBJ
            || ace->a_type == OTHER_OBJ))
        return 1;
    }
  return 0;
}

#endif

void
free_permission_context (struct permission_context *ctx)
{
#if USE_ACL
# if HAVE_ACL_GET_FILE  
  if (ctx->acl)
    acl_free (ctx->acl);
#  if !HAVE_ACL_TYPE_EXTENDED
  if (ctx->default_acl)
    acl_free (ctx->default_acl);
#  endif

# elif defined GETACL  
  free (ctx->entries);
#  ifdef ACE_GETACL
  free (ctx->ace_entries);
#  endif

# elif HAVE_GETACL  

#  if HAVE_ACLV_H
#  endif

# elif HAVE_STATACL  

# elif HAVE_ACLSORT  

# endif
#endif
}
