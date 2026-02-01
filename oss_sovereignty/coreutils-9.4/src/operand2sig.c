 


 

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "system.h"
#include "quote.h"
#include "sig2str.h"
#include "operand2sig.h"

extern int
operand2sig (char const *operand, char *signame)
{
  int signum;

  if (ISDIGIT (*operand))
    {
       
          signum &= signum >= 0xFF ? 0xFF : 0x7F;
        }
    }
  else
    {
       
      char *upcased = xstrdup (operand);
      char *p;
      for (p = upcased; *p; p++)
        if (strchr ("abcdefghijklmnopqrstuvwxyz", *p))
          *p += 'A' - 'a';

       
      if (!(str2sig (upcased, &signum) == 0
            || (upcased[0] == 'S' && upcased[1] == 'I' && upcased[2] == 'G'
                && str2sig (upcased + 3, &signum) == 0)))
        signum = -1;

      free (upcased);
    }

  if (signum < 0 || sig2str (signum, signame) != 0)
    {
      error (0, 0, _("%s: invalid signal"), quote (operand));
      return -1;
    }

  return signum;
}
