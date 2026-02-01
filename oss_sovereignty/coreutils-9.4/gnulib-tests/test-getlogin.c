 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (getlogin, char *, (void));

#include "test-getlogin.h"

int
main (void)
{
   
  char *buf = getlogin ();
  int err = buf ? 0 : errno;
#if defined __sun
  if (buf == NULL && err == 0)
    {
       
      fprintf (stderr, "Skipping test: no entry in /var/adm/utmpx.\n");
      exit (77);
    }
#endif
  test_getlogin_result (buf, err);

  return 0;
}
