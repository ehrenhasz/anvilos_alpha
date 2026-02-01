 
# include <sys/types.h>

 
# @INCLUDE_NEXT@ @NEXT_SYS_UIO_H@

#endif

#ifndef _@GUARD_PREFIX@_SYS_UIO_H
#define _@GUARD_PREFIX@_SYS_UIO_H

#if !@HAVE_SYS_UIO_H@
 
 
# include <sys/types.h>

# ifdef __cplusplus
extern "C" {
# endif

# if !GNULIB_defined_struct_iovec
 
struct iovec {
  void *iov_base;
  size_t iov_len;
};
#  define GNULIB_defined_struct_iovec 1
# endif

# ifdef __cplusplus
}
# endif

#endif

#endif  
#endif  
