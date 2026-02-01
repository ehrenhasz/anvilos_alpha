 

#include <config.h>

#include "closein.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "binary-io.h"
#include "ignore-value.h"

 
int
main (int argc, char **argv)
{
  char buf[7];
  atexit (close_stdin);

   
  set_binary_mode (0, O_BINARY);

  if (argc > 2)
    close (0);

  if (argc > 1)
    ignore_value (fread (buf, 1, 6, stdin));
  return 0;
}
