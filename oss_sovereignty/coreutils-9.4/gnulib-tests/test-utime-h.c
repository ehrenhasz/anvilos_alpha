 

#include <config.h>

#include <utime.h>

 
struct utimbuf b;

int
main (void)
{
  b.actime = b.modtime = 1493490248;  

  return 0;
}
