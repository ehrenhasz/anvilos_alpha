 

#include <config.h>

#include "mbsalign.h"
#include "macros.h"
#include <stdlib.h>
#include <locale.h>

int
main (void)
{
  char dest[4 * 16 + 1];
  size_t width, n;

#ifdef __ANDROID__
   
  if (MB_CUR_MAX == 1)
#endif
    {
       
      width = 4;
      n = mbsalign ("t\tés", dest, sizeof dest, &width, MBS_ALIGN_LEFT, 0);
      ASSERT (n == 4);
    }

   
  width = 4;
  n = mbsalign ("es", dest, sizeof dest, &width, MBS_ALIGN_CENTER, 0);
  ASSERT (*dest == ' ' && *(dest + n - 1) == ' ');
  ASSERT (n == 4);

   
  width = 4;
  n = mbsalign ("es", dest, sizeof dest, &width, MBS_ALIGN_CENTER,
                MBA_NO_RIGHT_PAD);
  ASSERT (n == 3);
  ASSERT (*dest == ' ' && *(dest + n - 1) == 's');

   
  width = 4;
  n = mbsalign ("es", dest, sizeof dest, &width, MBS_ALIGN_LEFT,
                MBA_NO_RIGHT_PAD);
  ASSERT (n == 2);
  ASSERT (*dest == 'e' && *(dest + n - 1) == 's');

   
  width = 4;
  n = mbsalign ("es", dest, sizeof dest, &width, MBS_ALIGN_CENTER,
                MBA_NO_LEFT_PAD | MBA_NO_RIGHT_PAD);
  ASSERT (n == 2);
  ASSERT (*dest == 'e' && *(dest + n - 1) == 's');

   
  width = 4;
  n = mbsalign ("es", dest, sizeof dest, &width, MBS_ALIGN_CENTER,
                MBA_NO_LEFT_PAD);
  ASSERT (n == 3);
  ASSERT (*dest == 'e' && *(dest + n - 1) == ' ');

  if (setlocale (LC_ALL, "en_US.UTF8"))
    {
       
      width = 4;
      n = mbsalign ("t\xe1\xe2s", dest, sizeof dest, &width, MBS_ALIGN_LEFT, 0);
      ASSERT (n == (size_t) -1);

       
      width = 4;
      n = mbsalign ("t\xe1\xe2s", dest, sizeof dest, &width,
                    MBS_ALIGN_LEFT, MBA_UNIBYTE_FALLBACK);
      ASSERT (n == 4);

       
      width = 4;
      n = mbsalign ("és", dest, sizeof dest, &width, MBS_ALIGN_CENTER, 0);
      ASSERT (n == 5);
      ASSERT (*dest == ' ' && *(dest + n - 1) == ' ');

       
      width = 4;
      n = mbsalign ("és", dest, sizeof dest, &width, MBS_ALIGN_LEFT, 0);
      ASSERT (n == 5);
      ASSERT (*(dest + n - 1) == ' ' && *(dest + n - 2) == ' ');

       
      width = 4;
      n = mbsalign ("és", dest, sizeof dest, &width, MBS_ALIGN_RIGHT, 0);
      ASSERT (n == 5);
      ASSERT (*(dest) == ' ' && *(dest + 1) == ' ');

       
      width = 4;                 
      n = mbsalign ("日月火水", dest, sizeof dest, &width,
                    MBS_ALIGN_LEFT, 0);
      ASSERT (n == 6);           

       
      width = 3;                 
      n = mbsalign ("¹²³⁴", dest, sizeof dest, &width, MBS_ALIGN_LEFT, 0);
      ASSERT (n == 6);           

       
      width = 4;                 
      n = mbsalign ("¹²³⁴", dest, 0, &width, MBS_ALIGN_LEFT, 0);
      ASSERT (n == 9);           

       
      width = 4;                 
      n = mbsalign ("¹²³", dest, 0, &width, MBS_ALIGN_LEFT, 0);
      ASSERT (width == 3);

       
      width = 4;
      n = mbsalign ("t\tés"   , dest, sizeof dest,
                    &width, MBS_ALIGN_LEFT, 0);
      ASSERT (n == 7);

       
      width = 4;
      n = mbsalign ("t\tés", dest, sizeof dest, &width, MBS_ALIGN_LEFT,
                    MBA_UNIBYTE_ONLY);
      ASSERT (n == 4);
    }

  return 0;
}
