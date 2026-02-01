 

#include <config.h>

#include "write-any-file.h"
#include "priv-set.h"
#include "root-uid.h"

#include <unistd.h>

 

bool
can_write_any_file (void)
{
  static bool initialized;
  static bool can_write;

  if (! initialized)
    {
      bool can = false;
#if defined PRIV_FILE_DAC_WRITE
      can = (priv_set_ismember (PRIV_FILE_DAC_WRITE) == 1);
#else
       
      can = (geteuid () == ROOT_UID);
#endif
      can_write = can;
      initialized = true;
    }

  return can_write;
}
