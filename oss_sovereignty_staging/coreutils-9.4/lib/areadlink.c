 

#include <config.h>

 
#include "areadlink.h"

#include "careadlinkat.h"

#include <stdlib.h>
#include <unistd.h>

 
static ssize_t
careadlinkatcwd (int fd, char const *filename, char *buffer,
                 size_t buffer_size)
{
   
  if (fd != AT_FDCWD)
    abort ();
  return readlink (filename, buffer, buffer_size);
}

 

char *
areadlink (char const *filename)
{
  return careadlinkat (AT_FDCWD, filename, NULL, 0, NULL, careadlinkatcwd);
}
