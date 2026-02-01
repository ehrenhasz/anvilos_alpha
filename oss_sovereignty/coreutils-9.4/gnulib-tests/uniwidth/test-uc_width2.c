 

#include <config.h>

#include "uniwidth.h"

#include <stdio.h>

#include "macros.h"

 
static char current_width;
 
static ucs4_t current_start;
static ucs4_t current_end;

static void
finish_interval (void)
{
  if (current_width != 0)
    {
      if (current_start == current_end)
        printf ("%04X\t\t%c\n", (unsigned) current_start, current_width);
      else
        printf ("%04X..%04X\t%c\n", (unsigned) current_start,
                (unsigned) current_end, current_width);
      current_width = 0;
    }
}

static void
add_to_interval (ucs4_t uc, char width)
{
  if (current_width == width && uc == current_end + 1)
    current_end = uc;
  else
    {
      finish_interval ();
      current_width = width;
      current_start = current_end = uc;
    }
}

int
main ()
{
  ucs4_t uc;

  for (uc = 0; uc < 0x110000; uc++)
    {
      int w1 = uc_width (uc, "UTF-8");
      int w2 = uc_width (uc, "GBK");
      char width =
        (w1 == 0 && w2 == 0 ? '0' :
         w1 == 1 && w2 == 1 ? '1' :
         w1 == 1 && w2 == 2 ? 'A' :
         w1 == 2 && w2 == 2 ? '2' :
         0);
      if (width == 0)
        {
           
          ASSERT (w1 < 0 && w2 < 0);
        }
      else
        add_to_interval (uc, width);
    }
  finish_interval ();

  return 0;
}
