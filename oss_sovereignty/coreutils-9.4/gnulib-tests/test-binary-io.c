 

#include <config.h>

#include "binary-io.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
   
  {
    int fd =
      open ("t-bin-out0.tmp", O_CREAT | O_TRUNC | O_RDWR | O_BINARY, 0600);
    if (write (fd, "Hello\n", 6) < 0)
      exit (1);
    close (fd);
  }
  {
    struct stat statbuf;
    if (stat ("t-bin-out0.tmp", &statbuf) < 0)
      exit (1);
    ASSERT (statbuf.st_size == 6);
  }

  switch (argv[1][0])
    {
    case '1':
       
      set_binary_mode (1, O_BINARY);
      fputs ("Hello\n", stdout);
      break;

    default:
      break;
    }

  return 0;
}
