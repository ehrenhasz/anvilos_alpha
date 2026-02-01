 

#ifndef SELINUX_LABEL_H
#define SELINUX_LABEL_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <selinux/selinux.h>
#include <errno.h>

_GL_INLINE_HEADER_BEGIN
#ifndef SE_LABEL_INLINE
# define SE_LABEL_INLINE _GL_INLINE
#endif

 
#ifndef _GL_ATTRIBUTE_MAYBE_UNUSED
# if 0  
#  define _GL_ATTRIBUTE_MAYBE_UNUSED [[__maybe_unused__]]
# elif defined __GNUC__ || defined __clang__
#  define _GL_ATTRIBUTE_MAYBE_UNUSED __attribute__ ((__unused__))
# else
#  define _GL_ATTRIBUTE_MAYBE_UNUSED
# endif
#endif

#define SELABEL_CTX_FILE 0

struct selabel_handle;

SE_LABEL_INLINE int
selabel_lookup (_GL_ATTRIBUTE_MAYBE_UNUSED struct selabel_handle *hnd,
                _GL_ATTRIBUTE_MAYBE_UNUSED char **context,
                _GL_ATTRIBUTE_MAYBE_UNUSED char const *key,
                _GL_ATTRIBUTE_MAYBE_UNUSED int type)
{ errno = ENOTSUP; return -1; }

SE_LABEL_INLINE struct selabel_handle *
selabel_open (_GL_ATTRIBUTE_MAYBE_UNUSED int backend,
              _GL_ATTRIBUTE_MAYBE_UNUSED struct selinux_opt *options,
              _GL_ATTRIBUTE_MAYBE_UNUSED unsigned nopt)
{ errno = ENOTSUP; return 0; }

SE_LABEL_INLINE void
selabel_close (_GL_ATTRIBUTE_MAYBE_UNUSED struct selabel_handle *hnd)
{ errno = ENOTSUP; }

_GL_INLINE_HEADER_END

#endif
