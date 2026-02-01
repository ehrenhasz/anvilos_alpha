 

#include <config.h>

#include "acl.h"

#include <stdlib.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  const char *file;
  int mode;

  ASSERT (argc == 3);

  file = argv[1];
  mode = strtol (argv[2], NULL, 8);

  set_acl (file, -1, mode);

  return 0;
}
