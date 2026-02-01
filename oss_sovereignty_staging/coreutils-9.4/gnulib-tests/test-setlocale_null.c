 

#include <config.h>

 
#include <locale.h>

 
static char buf[SETLOCALE_NULL_ALL_MAX];

int
main ()
{
   
  return setlocale_null_r (LC_ALL, buf, sizeof (buf)) != 0;
}
