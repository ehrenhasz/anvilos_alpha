 

#include <config.h>

#include <stdio.h>

#include <errno.h>
#include <unistd.h>

#undef remove

 
int
rpl_remove (char const *name)
{
   
  int result = rmdir (name);
  if (result && errno == ENOTDIR)
    result = unlink (name);
  return result;
}
