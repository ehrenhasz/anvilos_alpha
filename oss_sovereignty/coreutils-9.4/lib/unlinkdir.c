 

#include <config.h>

#include "unlinkdir.h"
#include "priv-set.h"
#include "root-uid.h"
#include <unistd.h>

#if ! UNLINK_CANNOT_UNLINK_DIR

 

bool
cannot_unlink_dir (void)
{
  static bool initialized;
  static bool cannot;

  if (! initialized)
    {
# if defined PRIV_SYS_LINKDIR
       
      cannot = (priv_set_ismember (PRIV_SYS_LINKDIR) == 0);
# else
       
      cannot = (geteuid () != ROOT_UID);
# endif
      initialized = true;
    }

  return cannot;
}

#endif
