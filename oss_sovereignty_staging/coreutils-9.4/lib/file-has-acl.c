 

 
#if (__GNUC__ == 4 && 6 <= __GNUC_MINOR__) || 4 < __GNUC__
# pragma GCC diagnostic ignored "-Wsuggest-attribute=const"
#endif

#include <config.h>

#include "acl.h"

#include "acl-internal.h"
#include "attribute.h"
#include "minmax.h"

#if USE_ACL && HAVE_LINUX_XATTR_H && HAVE_LISTXATTR
# include <stdckdint.h>
# include <string.h>
# include <arpa/inet.h>
# include <sys/xattr.h>
# include <linux/xattr.h>
# ifndef XATTR_NAME_NFSV4_ACL
#  define XATTR_NAME_NFSV4_ACL "system.nfs4_acl"
# endif
# ifndef XATTR_NAME_POSIX_ACL_ACCESS
#  define XATTR_NAME_POSIX_ACL_ACCESS "system.posix_acl_access"
# endif
# ifndef XATTR_NAME_POSIX_ACL_DEFAULT
#  define XATTR_NAME_POSIX_ACL_DEFAULT "system.posix_acl_default"
# endif

enum {
   
  ACE4_ACCESS_DENIED_ACE_TYPE  = 0x00000001,
  ACE4_IDENTIFIER_GROUP        = 0x00000040
};

 

ATTRIBUTE_PURE static bool
have_xattr (char const *attr, char const *listbuf, ssize_t listsize)
{
  char const *blim = listbuf + listsize;
  for (char const *b = listbuf; b < blim; b += strlen (b) + 1)
    for (char const *a = attr; *a == *b; a++, b++)
      if (!*a)
        return true;
  return false;
}

 

static int
acl_nfs4_nontrivial (uint32_t *xattr, ssize_t nbytes)
{
  enum { BYTES_PER_NETWORK_UINT = 4};

   
  nbytes -= BYTES_PER_NETWORK_UINT;
  if (nbytes < 0)
    return -1;
  uint32_t num_aces = ntohl (*xattr++);
  if (6 < num_aces)
    return 1;
  int ace_found = 0;

  for (int ace_n = 0; ace_n < num_aces; ace_n++)
    {
       
      nbytes -= 4 * BYTES_PER_NETWORK_UINT;
      if (nbytes < 0)
        return -1;
      uint32_t type = ntohl (xattr[0]);
      uint32_t flag = ntohl (xattr[1]);
      uint32_t wholen = ntohl (xattr[3]);
      xattr += 4;
      int whowords = (wholen / BYTES_PER_NETWORK_UINT
                      + (wholen % BYTES_PER_NETWORK_UINT != 0));
      int64_t wholen4 = whowords;
      wholen4 *= BYTES_PER_NETWORK_UINT;

       
      if (ACE4_ACCESS_DENIED_ACE_TYPE < type)
        return 1;

       
      if (flag & ~ACE4_IDENTIFIER_GROUP)
        return 1;

       
      if (nbytes - wholen4 < 0)
        return -1;
      nbytes -= wholen4;

       
      int who2
        = (wholen == 6 && memcmp (xattr, "OWNER@", 6) == 0 ? 0
           : wholen == 6 && memcmp (xattr, "GROUP@", 6) == 0 ? 2
           : wholen == 9 && memcmp (xattr, "EVERYONE@", 9) == 0 ? 4
           : -1);
      if (who2 < 0)
        return 1;
      int ace_found_bit = 1 << (who2 | type);
      if (ace_found & ace_found_bit)
        return 1;
      ace_found |= ace_found_bit;

      xattr += whowords;
    }

  return 0;
}
#endif

 

int
file_has_acl (char const *name, struct stat const *sb)
{
#if USE_ACL
  if (! S_ISLNK (sb->st_mode))
    {

# if HAVE_LINUX_XATTR_H && HAVE_LISTXATTR
      int initial_errno = errno;

       
      typedef uint32_t trivial_NFSv4_xattr_buf[2 * (6 + 6 + 7)];

       
      union {
        trivial_NFSv4_xattr_buf xattr;
        char ch[sizeof (trivial_NFSv4_xattr_buf)];
      } stackbuf;

      char *listbuf = stackbuf.ch;
      ssize_t listbufsize = sizeof stackbuf.ch;
      char *heapbuf = NULL;
      ssize_t listsize;

       
      while ((listsize = listxattr (name, listbuf, listbufsize)) < 0
             && errno == ERANGE)
        {
          free (heapbuf);
          ssize_t newsize = listxattr (name, NULL, 0);
          if (newsize <= 0)
            return newsize;

           
          bool overflow = ckd_add (&listbufsize, listbufsize, listbufsize >> 1);
          listbufsize = MAX (listbufsize, newsize);
          if (overflow || SIZE_MAX < listbufsize)
            {
              errno = ENOMEM;
              return -1;
            }

          listbuf = heapbuf = malloc (listbufsize);
          if (!listbuf)
            return -1;
        }

       
      bool nfsv4_acl
        = 0 < listsize && have_xattr (XATTR_NAME_NFSV4_ACL, listbuf, listsize);
      int ret
        = (listsize <= 0 ? listsize
           : (nfsv4_acl
              || have_xattr (XATTR_NAME_POSIX_ACL_ACCESS, listbuf, listsize)
              || (S_ISDIR (sb->st_mode)
                  && have_xattr (XATTR_NAME_POSIX_ACL_DEFAULT,
                                 listbuf, listsize))));
      free (heapbuf);

       
      if (nfsv4_acl)
        {
          ret = getxattr (name, XATTR_NAME_NFSV4_ACL,
                          stackbuf.xattr, sizeof stackbuf.xattr);
          if (ret < 0)
            switch (errno)
              {
              case ENODATA: return 0;
              case ERANGE : return 1;  
              }
          else
            {
               
              ret = acl_nfs4_nontrivial (stackbuf.xattr, ret);
              if (ret < 0)
                {
                  errno = EINVAL;
                  return ret;
                }
              errno = initial_errno;
            }
        }
      if (ret < 0)
        return - acl_errno_valid (errno);
      return ret;

# elif HAVE_ACL_GET_FILE

       
       
      int ret;

      if (HAVE_ACL_EXTENDED_FILE)  
        {
           
          ret = acl_extended_file (name);
        }
      else  
        {
#  if HAVE_ACL_TYPE_EXTENDED  
           
          acl_t acl = acl_get_file (name, ACL_TYPE_EXTENDED);
          if (acl)
            {
              ret = acl_extended_nontrivial (acl);
              acl_free (acl);
            }
          else
            ret = -1;
#  else  
          acl_t acl = acl_get_file (name, ACL_TYPE_ACCESS);
          if (acl)
            {
              int saved_errno;

              ret = acl_access_nontrivial (acl);
              saved_errno = errno;
              acl_free (acl);
              errno = saved_errno;
#   if HAVE_ACL_FREE_TEXT  
               
#   else  
               
              if (ret == 0 && S_ISDIR (sb->st_mode))
                {
                  acl = acl_get_file (name, ACL_TYPE_DEFAULT);
                  if (acl)
                    {
#    ifdef __CYGWIN__  
                      ret = acl_access_nontrivial (acl);
                      saved_errno = errno;
                      acl_free (acl);
                      errno = saved_errno;
#    else
                      ret = (0 < acl_entries (acl));
                      acl_free (acl);
#    endif
                    }
                  else
                    ret = -1;
                }
#   endif
            }
          else
            ret = -1;
#  endif
        }
      if (ret < 0)
        return - acl_errno_valid (errno);
      return ret;

# elif HAVE_FACL && defined GETACL  

#  if defined ACL_NO_TRIVIAL

       
      return acl_trivial (name);

#  else  

       
      {
         
        enum
          {
            alloc_init = 4000 / sizeof (aclent_t),  
            alloc_max = MIN (INT_MAX, SIZE_MAX / sizeof (aclent_t))
          };
        aclent_t buf[alloc_init];
        size_t alloc = alloc_init;
        aclent_t *entries = buf;
        aclent_t *malloced = NULL;
        int count;

        for (;;)
          {
            count = acl (name, GETACL, alloc, entries);
            if (count < 0 && errno == ENOSPC)
              {
                 
                free (malloced);
                if (alloc > alloc_max / 2)
                  {
                    errno = ENOMEM;
                    return -1;
                  }
                alloc = 2 * alloc;  
                entries = malloced =
                  (aclent_t *) malloc (alloc * sizeof (aclent_t));
                if (entries == NULL)
                  {
                    errno = ENOMEM;
                    return -1;
                  }
                continue;
              }
            break;
          }
        if (count < 0)
          {
            if (errno == ENOSYS || errno == ENOTSUP)
              ;
            else
              {
                free (malloced);
                return -1;
              }
          }
        else if (count == 0)
          ;
        else
          {
             
            if (count > 4)
              {
                free (malloced);
                return 1;
              }

            if (acl_nontrivial (count, entries))
              {
                free (malloced);
                return 1;
              }
          }
        free (malloced);
      }

#   ifdef ACE_GETACL
       
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
            count = acl (name, ACE_GETACL, alloc, entries);
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
        if (count < 0)
          {
            if (errno == ENOSYS || errno == EINVAL)
              ;
            else
              {
                free (malloced);
                return -1;
              }
          }
        else if (count == 0)
          ;
        else
          {
             
            if (count > 6)
              {
                free (malloced);
                return 1;
              }

            if (acl_ace_nontrivial (count, entries))
              {
                free (malloced);
                return 1;
              }
          }
        free (malloced);
      }
#   endif

      return 0;
#  endif

# elif HAVE_GETACL  

      {
        struct acl_entry entries[NACLENTRIES];
        int count;

        count = getacl (name, NACLENTRIES, entries);

        if (count < 0)
          {
             
            if (errno == ENOSYS || errno == EOPNOTSUPP || errno == ENOTSUP)
              ;
            else
              return -1;
          }
        else if (count == 0)
          return 0;
        else  
          {
            if (count > NACLENTRIES)
               
              abort ();

             
            if (count > 3)
              return 1;

            {
              struct stat statbuf;

              if (stat (name, &statbuf) == -1 && errno != EOVERFLOW)
                return -1;

              return acl_nontrivial (count, entries);
            }
          }
      }

#  if HAVE_ACLV_H  

      {
        struct acl entries[NACLVENTRIES];
        int count;

        count = acl ((char *) name, ACL_GET, NACLVENTRIES, entries);

        if (count < 0)
          {
             
            if (errno == ENOSYS || errno == EOPNOTSUPP || errno == EINVAL)
              ;
            else
              return -1;
          }
        else if (count == 0)
          return 0;
        else  
          {
            if (count > NACLVENTRIES)
               
              abort ();

             
            if (count > 4)
              return 1;

            return aclv_nontrivial (count, entries);
          }
      }

#  endif

# elif HAVE_ACLX_GET && defined ACL_AIX_WIP  

      acl_type_t type;
      char aclbuf[1024];
      void *acl = aclbuf;
      size_t aclsize = sizeof (aclbuf);
      mode_t mode;

      for (;;)
        {
           
          type.u64 = ACL_ANY;
          if (aclx_get (name, 0, &type, aclbuf, &aclsize, &mode) >= 0)
            break;
          if (errno == ENOSYS)
            return 0;
          if (errno != ENOSPC)
            {
              if (acl != aclbuf)
                free (acl);
              return -1;
            }
          aclsize = 2 * aclsize;
          if (acl != aclbuf)
            free (acl);
          acl = malloc (aclsize);
          if (acl == NULL)
            {
              errno = ENOMEM;
              return -1;
            }
        }

      if (type.u64 == ACL_AIXC)
        {
          int result = acl_nontrivial ((struct acl *) acl);
          if (acl != aclbuf)
            free (acl);
          return result;
        }
      else if (type.u64 == ACL_NFS4)
        {
          int result = acl_nfs4_nontrivial ((nfs4_acl_int_t *) acl);
          if (acl != aclbuf)
            free (acl);
          return result;
        }
      else
        {
           
          if (acl != aclbuf)
            free (acl);
          errno = EINVAL;
          return -1;
        }

# elif HAVE_STATACL  

      union { struct acl a; char room[4096]; } u;

      if (statacl ((char *) name, STX_NORMAL, &u.a, sizeof (u)) < 0)
        return -1;

      return acl_nontrivial (&u.a);

# elif HAVE_ACLSORT  

      {
        struct acl entries[NACLENTRIES];
        int count;

        count = acl ((char *) name, ACL_GET, NACLENTRIES, entries);

        if (count < 0)
          {
            if (errno == ENOSYS || errno == ENOTSUP)
              ;
            else
              return -1;
          }
        else if (count == 0)
          return 0;
        else  
          {
            if (count > NACLENTRIES)
               
              abort ();

             
            if (count > 4)
              return 1;

            return acl_nontrivial (count, entries);
          }
      }

# endif
    }
#endif

  return 0;
}
