 

#ifndef _GL_ACL_H
#define _GL_ACL_H 1

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <sys/types.h>
#include <sys/stat.h>

bool acl_errno_valid (int) _GL_ATTRIBUTE_CONST;
int file_has_acl (char const *, struct stat const *);
int qset_acl (char const *, int, mode_t);
int set_acl (char const *, int, mode_t);
int qcopy_acl (char const *, int, char const *, int, mode_t);
int copy_acl (char const *, int, char const *, int, mode_t);
int chmod_or_fchmod (char const *, int, mode_t);

#endif
