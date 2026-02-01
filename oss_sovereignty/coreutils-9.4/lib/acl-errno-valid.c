 

#include <config.h>

#include <acl.h>

#include <errno.h>

 
bool
acl_errno_valid (int errnum)
{
   
  switch (errnum)
    {
    case EBUSY: return false;
    case EINVAL: return false;
#if defined __APPLE__ && defined __MACH__
    case ENOENT: return false;
#endif
    case ENOSYS: return false;

#if defined ENOTSUP && ENOTSUP != EOPNOTSUPP
# if ENOTSUP != ENOSYS  
    case ENOTSUP: return false;
# endif
#endif

    case EOPNOTSUPP: return false;
    default: return true;
    }
}
