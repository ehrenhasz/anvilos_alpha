 

#include <config.h>

#include <alloca.h>

#if HAVE_ALLOCA

static void
do_allocation (int n)
{
  void *volatile ptr = alloca (n);
  (void) ptr;
}

void (*func) (int) = do_allocation;

#endif

int
main ()
{
#if HAVE_ALLOCA
  int i;

   
  for (i = 0; i < 100000; i++)
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
#endif

  return 0;
}
