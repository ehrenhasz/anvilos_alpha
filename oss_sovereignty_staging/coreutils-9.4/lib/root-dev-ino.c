 

#include <config.h>

#include "root-dev-ino.h"

#include <stdlib.h>

 
struct dev_ino *
get_root_dev_ino (struct dev_ino *root_d_i)
{
  struct stat statbuf;
  if (lstat ("/", &statbuf))
    return nullptr;
  root_d_i->st_ino = statbuf.st_ino;
  root_d_i->st_dev = statbuf.st_dev;
  return root_d_i;
}
