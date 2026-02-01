 

#include <config.h>
#include "buffer-lcm.h"

 

size_t
buffer_lcm (size_t a, size_t b, size_t lcm_max)
{
  size_t size;

   
  if (!a)
    size = b ? b : 8 * 1024;
  else
    {
      if (b)
        {
           

          size_t lcm, m, n, q, r;

           
          for (m = a, n = b;  (r = m % n) != 0;  m = n, n = r)
            continue;

           
          q = a / n;
          lcm = q * b;
          if (lcm <= lcm_max && lcm / b == q)
            return lcm;
        }

      size = a;
    }

  return size <= lcm_max ? size : lcm_max;
}
