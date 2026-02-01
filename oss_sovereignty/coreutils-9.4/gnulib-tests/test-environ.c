 

#include <config.h>

#include <unistd.h>

#include <string.h>

int
main ()
{
   
  char **remaining_variables = environ;
  char *string;

  for (; (string = *remaining_variables) != NULL; remaining_variables++)
    {
      if (strncmp (string, "PATH=", 5) == 0)
         
        return 0;
    }
   
  return 1;
}
