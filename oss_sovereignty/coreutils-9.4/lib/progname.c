 
#undef ENABLE_RELOCATABLE  
#include "progname.h"

#include <errno.h>  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


 
const char *program_name = NULL;

 
void
set_program_name (const char *argv0)
{
   
  const char *slash;
  const char *base;

   
  if (argv0 == NULL)
    {
       
      fputs ("A NULL argv[0] was passed through an exec system call.\n",
             stderr);
      abort ();
    }

  slash = strrchr (argv0, '/');
  base = (slash != NULL ? slash + 1 : argv0);
  if (base - argv0 >= 7 && strncmp (base - 7, "/.libs/", 7) == 0)
    {
      argv0 = base;
      if (strncmp (base, "lt-", 3) == 0)
        {
          argv0 = base + 3;
           
#if HAVE_DECL_PROGRAM_INVOCATION_SHORT_NAME
          program_invocation_short_name = (char *) argv0;
#endif
        }
    }

   

  program_name = argv0;

   
#if HAVE_DECL_PROGRAM_INVOCATION_NAME
  program_invocation_name = (char *) argv0;
#endif
}
