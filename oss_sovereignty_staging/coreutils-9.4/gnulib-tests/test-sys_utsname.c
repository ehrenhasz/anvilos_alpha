 

#include <config.h>

#include <sys/utsname.h>

#include <string.h>

int
main ()
{
  struct utsname buf;

  strcpy (buf.sysname, "Linux");
  strcpy (buf.nodename, "hobbybox");
  strcpy (buf.release, "3.141.592");
  strcpy (buf.version, "GENERIC");
  strcpy (buf.machine, "i586");

  return 0;
}
