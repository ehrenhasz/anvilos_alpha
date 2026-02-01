 

#include <config.h>

#include "physmem.h"

#include <stdio.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  printf ("Total memory:     %12.f B = %6.f MiB\n",
          physmem_total (), physmem_total () / (1024.0 * 1024.0));
  printf ("Available memory: %12.f B = %6.f MiB\n",
          physmem_available (), physmem_available () / (1024.0 * 1024.0));
  ASSERT (physmem_total () >= physmem_available ());
  ASSERT (physmem_available () >= 4 * 1024 * 1024);

  return 0;
}
