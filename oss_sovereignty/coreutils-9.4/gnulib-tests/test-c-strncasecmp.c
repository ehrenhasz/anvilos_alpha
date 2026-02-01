 

#include <config.h>

#include "c-strcase.h"
#include "c-ctype.h"

#include <locale.h>
#include <string.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  if (argc > 1)
    {
       
      if (setlocale (LC_ALL, "") == NULL)
        return 1;
    }

  ASSERT (c_strncasecmp ("paragraph", "Paragraph", 1000000) == 0);
  ASSERT (c_strncasecmp ("paragraph", "Paragraph", 9) == 0);

  ASSERT (c_strncasecmp ("paragrapH", "parAgRaph", 1000000) == 0);
  ASSERT (c_strncasecmp ("paragrapH", "parAgRaph", 9) == 0);

  ASSERT (c_strncasecmp ("paragraph", "paraLyzed", 10) < 0);
  ASSERT (c_strncasecmp ("paragraph", "paraLyzed", 9) < 0);
  ASSERT (c_strncasecmp ("paragraph", "paraLyzed", 5) < 0);
  ASSERT (c_strncasecmp ("paragraph", "paraLyzed", 4) == 0);
  ASSERT (c_strncasecmp ("paraLyzed", "paragraph", 10) > 0);
  ASSERT (c_strncasecmp ("paraLyzed", "paragraph", 9) > 0);
  ASSERT (c_strncasecmp ("paraLyzed", "paragraph", 5) > 0);
  ASSERT (c_strncasecmp ("paraLyzed", "paragraph", 4) == 0);

  ASSERT (c_strncasecmp ("para", "paragraph", 10) < 0);
  ASSERT (c_strncasecmp ("para", "paragraph", 9) < 0);
  ASSERT (c_strncasecmp ("para", "paragraph", 5) < 0);
  ASSERT (c_strncasecmp ("para", "paragraph", 4) == 0);
  ASSERT (c_strncasecmp ("paragraph", "para", 10) > 0);
  ASSERT (c_strncasecmp ("paragraph", "para", 9) > 0);
  ASSERT (c_strncasecmp ("paragraph", "para", 5) > 0);
  ASSERT (c_strncasecmp ("paragraph", "para", 4) == 0);

   

  ASSERT (c_strncasecmp ("\311mily", "\351mile", 4) < 0);
  ASSERT (c_strncasecmp ("\351mile", "\311mily", 4) > 0);

   

  ASSERT (c_strncasecmp ("\303\266zg\303\274r", "\303\226ZG\303\234R", 99) > 0);  
  ASSERT (c_strncasecmp ("\303\226ZG\303\234R", "\303\266zg\303\274r", 99) < 0);  

#if C_CTYPE_ASCII
   
  ASSERT (c_strncasecmp ("turkish", "TURK\304\260SH", 7) < 0);
  ASSERT (c_strncasecmp ("TURK\304\260SH", "turkish", 7) > 0);
#endif

  return 0;
}
