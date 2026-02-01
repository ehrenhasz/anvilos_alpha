 

#include <config.h>

#include "fcntl-safer.h"

#include <fcntl.h>
#include "unistd-safer.h"

int
creat_safer (char const *file, mode_t mode)
{
  return fd_safer (creat (file, mode));
}
