 

#include <config.h>

#include <string.h>

#include <locale.h>

#include "macros.h"

int
main ()
{
   
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

  ASSERT (mbscasecmp ("paragraph", "Paragraph") == 0);

  ASSERT (mbscasecmp ("paragrapH", "parAgRaph") == 0);

  ASSERT (mbscasecmp ("paragraph", "paraLyzed") < 0);
  ASSERT (mbscasecmp ("paraLyzed", "paragraph") > 0);

  ASSERT (mbscasecmp ("para", "paragraph") < 0);
  ASSERT (mbscasecmp ("paragraph", "para") > 0);

   

  ASSERT (mbscasecmp ("\303\266zg\303\274r", "\303\226ZG\303\234R") == 0);  
  ASSERT (mbscasecmp ("\303\226ZG\303\234R", "\303\266zg\303\274r") == 0);  

   
  ASSERT (mbscasecmp ("turkish", "TURK\304\260SH") == 0);
  ASSERT (mbscasecmp ("TURK\304\260SH", "turkish") == 0);

  return 0;
}
