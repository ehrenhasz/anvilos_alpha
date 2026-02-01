 

#include <config.h>

 
#include "yesno.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "closein.h"
#include "binary-io.h"

 
int
main (int argc, char **argv)
{
  int i = 1;

   
  atexit (close_stdin);
   
  set_binary_mode (0, O_BINARY);

  if (1 < argc)
    i = atoi (argv[1]);
  if (!i)
    {
      i = 1;
      close (0);
    }
  while (i--)
    puts (yesno () ? "Y" : "N");
  return 0;
}
