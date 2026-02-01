 

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

  ASSERT (c_strcasecmp ("paragraph", "Paragraph") == 0);

  ASSERT (c_strcasecmp ("paragrapH", "parAgRaph") == 0);

  ASSERT (c_strcasecmp ("paragraph", "paraLyzed") < 0);
  ASSERT (c_strcasecmp ("paraLyzed", "paragraph") > 0);

  ASSERT (c_strcasecmp ("para", "paragraph") < 0);
  ASSERT (c_strcasecmp ("paragraph", "para") > 0);

   

  ASSERT (c_strcasecmp ("\311mile", "\351mile") < 0);
  ASSERT (c_strcasecmp ("\351mile", "\311mile") > 0);

   

  ASSERT (c_strcasecmp ("\303\266zg\303\274r", "\303\226ZG\303\234R") > 0);  
  ASSERT (c_strcasecmp ("\303\226ZG\303\234R", "\303\266zg\303\274r") < 0);  

#if C_CTYPE_ASCII
   
  ASSERT (c_strcasecmp ("turkish", "TURK\304\260SH") < 0);
  ASSERT (c_strcasecmp ("TURK\304\260SH", "turkish") > 0);
#endif

  return 0;
}
