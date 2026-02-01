 

#include <config.h>

#include <string.h>

#include <locale.h>

#include "macros.h"

int
main ()
{
   
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

   
  {
    const char input[] = "\312\276\300\375 \312\276\300\375 \312\276\300\375";  
    const char *result = mbschr (input, ' ');
    ASSERT (result == input + 4);
  }

  {
    const char input[] = "\312\276\300\375";  
    const char *result = mbschr (input, ' ');
    ASSERT (result == NULL);
  }

   
  {
    const char input[] = "\272\305123\324\313\320\320\241\243";  
    const char *result = mbschr (input, '2');
    ASSERT (result == input + 3);
  }

   
  {
    const char input[] = "\203\062\332\066123\324\313\320\320\241\243";  
    const char *result = mbschr (input, '2');
    ASSERT (result == input + 5);
  }

  {
    const char input[] = "\312\300\275\347\304\343\272\303\243\241";  
    const char *result = mbschr (input, '!');
    ASSERT (result == NULL);
  }

  return 0;
}
