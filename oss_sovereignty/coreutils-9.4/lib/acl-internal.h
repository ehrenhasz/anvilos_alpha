 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include "acl.h"

#include <stdlib.h>

 
#if HAVE_SYS_ACL_H
# include <sys/acl.h>
#endif
#if defined HAVE_FACL && ! defined GETACLCNT && defined ACL_CNT
# define GETACLCNT ACL_CNT
#endif

 
#ifdef HAVE_ACL_LIBACL_H
# include <acl/libacl.h>
#endif

 
#if HAVE_ACLV_H
# include <sys/types.h>
# include <aclv.h>
 
extern int acl (char *, int, int, struct acl *);
extern int aclsort (int, int, struct acl *);
#endif

#include <errno.h>

#include <limits.h>
#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

#ifndef HAVE_FCHMOD
# define HAVE_FCHMOD false
# define fchmod(fd, mode) (-1)
#endif

_GL_INLINE_HEADER_BEGIN
#ifndef ACL_INTERNAL_INLINE
# define ACL_INTERNAL_INLINE _GL_INLINE
#endif

#if USE_ACL

# if HAVE_ACL_GET_FILE
 
 

#  ifndef MIN_ACL_ENTRIES
#   define MIN_ACL_ENTRIES 4
#  endif

 
#  ifdef HAVE_ACL_GET_FD
 
#   if HAVE_ACL_FREE_TEXT  
ACL_INTERNAL_INLINE acl_t
rpl_acl_get_fd (int fd)
{
  return acl_get_fd (fd, ACL_TYPE_ACCESS);
}
#    undef acl_get_fd
#    define acl_get_fd rpl_acl_get_fd
#   endif
#  else
#   define HAVE_ACL_GET_FD false
#   undef acl_get_fd
#   define acl_get_fd(fd) (NULL)
#  endif

 
#  ifdef HAVE_ACL_SET_FD
 
#   if HAVE_ACL_FREE_TEXT  
ACL_INTERNAL_INLINE int
rpl_acl_set_fd (int fd, acl_t acl)
{
  return acl_set_fd (fd, ACL_TYPE_ACCESS, acl);
}
#    undef acl_set_fd
#    define acl_set_fd rpl_acl_set_fd
#   endif
#  else
#   define HAVE_ACL_SET_FD false
#   undef acl_set_fd
#   define acl_set_fd(fd, acl) (-1)
#  endif

 
#  if ! HAVE_ACL_FREE_TEXT
#   define acl_free_text(buf) acl_free (buf)
#  endif

 
 
#  if !defined HAVE_ACL_EXTENDED_FILE || defined __CYGWIN__
#   undef HAVE_ACL_EXTENDED_FILE
#   define HAVE_ACL_EXTENDED_FILE false
#   define acl_extended_file(name) (-1)
#  endif

#  if ! defined HAVE_ACL_FROM_MODE && ! defined HAVE_ACL_FROM_TEXT
#   define acl_from_mode (NULL)
#  endif

 
#  if (HAVE_ACL_COPY_EXT_NATIVE && HAVE_ACL_CREATE_ENTRY_NP) || defined __sgi  
#   define MODE_INSIDE_ACL 0
#  endif

 
 
#  if !HAVE_ACL_ENTRIES
#   define acl_entries rpl_acl_entries
extern int acl_entries (acl_t);
#  endif

#  if HAVE_ACL_TYPE_EXTENDED  
 
extern int acl_extended_nontrivial (acl_t);
#  else
 
extern int acl_access_nontrivial (acl_t);

 
extern int acl_default_nontrivial (acl_t);
#  endif

# elif HAVE_FACL && defined GETACL  

 
#  if defined __CYGWIN__  
#   define MODE_INSIDE_ACL 0
#  endif

 
extern int acl_nontrivial (int count, aclent_t *entries) _GL_ATTRIBUTE_PURE;

#  ifdef ACE_GETACL  

 
extern int acl_ace_nontrivial (int count, ace_t *entries) _GL_ATTRIBUTE_PURE;

 
 
#   define OLD_ALLOW 0
#   define OLD_DENY  1
#   define NEW_ACE_ACCESS_ALLOWED_ACE_TYPE 0  
#   define NEW_ACE_ACCESS_DENIED_ACE_TYPE  1  
 
#   define OLD_ACE_OWNER            0x0100
#   define OLD_ACE_GROUP            0x0200
#   define OLD_ACE_OTHER            0x0400
#   define NEW_ACE_OWNER            0x1000
#   define NEW_ACE_GROUP            0x2000
#   define NEW_ACE_IDENTIFIER_GROUP 0x0040
#   define NEW_ACE_EVERYONE         0x4000
 
#   define NEW_ACE_READ_DATA         0x001  
#   define NEW_ACE_WRITE_DATA        0x002  
#   define NEW_ACE_APPEND_DATA       0x004
#   define NEW_ACE_READ_NAMED_ATTRS  0x008
#   define NEW_ACE_WRITE_NAMED_ATTRS 0x010
#   define NEW_ACE_EXECUTE           0x020
#   define NEW_ACE_DELETE_CHILD      0x040
#   define NEW_ACE_READ_ATTRIBUTES   0x080
#   define NEW_ACE_WRITE_ATTRIBUTES  0x100
#   define NEW_ACE_DELETE          0x10000
#   define NEW_ACE_READ_ACL        0x20000
#   define NEW_ACE_WRITE_ACL       0x40000
#   define NEW_ACE_WRITE_OWNER     0x80000
#   define NEW_ACE_SYNCHRONIZE    0x100000

#  endif

# elif HAVE_GETACL  

 
extern int acl_nontrivial (int count, struct acl_entry *entries);

#  if HAVE_ACLV_H  

 
extern int aclv_nontrivial (int count, struct acl *entries);

#  endif

# elif HAVE_ACLX_GET && 0  

 

# elif HAVE_STATACL  

 
extern int acl_nontrivial (struct acl *a);

# elif HAVE_ACLSORT  

 
extern int acl_nontrivial (int count, struct acl *entries);

# endif

 
# ifndef MODE_INSIDE_ACL
#  define MODE_INSIDE_ACL 1
# endif

#endif

struct permission_context {
  mode_t mode;
#if USE_ACL
# if HAVE_ACL_GET_FILE  
  acl_t acl;
#  if !HAVE_ACL_TYPE_EXTENDED
  acl_t default_acl;
#  endif
  bool acls_not_supported;

# elif defined GETACL  
  int count;
  aclent_t *entries;
#  ifdef ACE_GETACL
  int ace_count;
  ace_t *ace_entries;
#  endif

# elif HAVE_GETACL  
  struct acl_entry entries[NACLENTRIES];
  int count;
#  if HAVE_ACLV_H
  struct acl aclv_entries[NACLVENTRIES];
  int aclv_count;
#  endif

# elif HAVE_STATACL  
  union { struct acl a; char room[4096]; } u;
  bool have_u;

# elif HAVE_ACLSORT  
  struct acl entries[NACLENTRIES];
  int count;

# endif
#endif
};

int get_permissions (const char *, int, mode_t, struct permission_context *);
int set_permissions (struct permission_context *, const char *, int);
void free_permission_context (struct permission_context *);

_GL_INLINE_HEADER_END
