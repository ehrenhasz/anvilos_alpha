 

#include <config.h>

#include <dirent.h>

 
_GL_UNUSED static DIR *dir;
static struct dirent d;
static ino_t i;

int
main (void)
{
  return d.d_name[0] + i;
}
