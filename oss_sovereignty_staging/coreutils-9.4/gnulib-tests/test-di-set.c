 

#include <config.h>

#include "di-set.h"

#include "macros.h"

int
main (void)
{
  struct di_set *dis = di_set_alloc ();
  ASSERT (dis);
  di_set_free (dis);  
  dis = di_set_alloc ();
  ASSERT (dis);

  ASSERT (di_set_lookup (dis, 2, 5) == 0);  
  ASSERT (di_set_insert (dis, 2, 5) == 1);  
  ASSERT (di_set_insert (dis, 2, 5) == 0);  
  ASSERT (di_set_insert (dis, 3, 5) == 1);  
  ASSERT (di_set_insert (dis, 2, 8) == 1);  
  ASSERT (di_set_lookup (dis, 2, 5) == 1);  

   
  ASSERT (di_set_insert (dis, 5, (ino_t) -1) == 1);
  ASSERT (di_set_insert (dis, 5, (ino_t) -1) == 0);  

  {
    unsigned int i;
    for (i = 0; i < 3000; i++)
      ASSERT (di_set_insert (dis, 9, i) == 1);
    for (i = 0; i < 3000; i++)
      ASSERT (di_set_insert (dis, 9, i) == 0);  
  }

  di_set_free (dis);

  return 0;
}
