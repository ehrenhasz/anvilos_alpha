 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (getdtablesize, int, (void));

#include "macros.h"

 
#if __GNUC__ >= 13
# pragma GCC diagnostic ignored "-Wanalyzer-fd-leak"
#endif

int
main (int argc, char *argv[])
{
  ASSERT (getdtablesize () >= 3);
  ASSERT (dup2 (0, getdtablesize() - 1) == getdtablesize () - 1);
  ASSERT (dup2 (0, getdtablesize()) == -1);

  return 0;
}
