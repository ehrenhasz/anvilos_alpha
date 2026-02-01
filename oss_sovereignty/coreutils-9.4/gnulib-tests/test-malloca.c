 

#include <config.h>

#include "malloca.h"

#include <stdlib.h>

static void
do_allocation (int n)
{
  void *volatile ptr = malloca (n);
  freea (ptr);
  safe_alloca (n);
}

void (*func) (int) = do_allocation;

int
main ()
{
  int i;

   
  unsetenv ("MALLOC_PERTURB_");

   
  for (i = 0; i < 50000; i++)
    {
       
      func (34);
      func (134);
      func (399);
      func (510823);
      func (129321);
      func (0);
      func (4070);
      func (4095);
      func (1);
      func (16582);
    }

  return 0;
}
