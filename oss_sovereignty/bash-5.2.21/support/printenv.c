 

 

#if defined (HAVE_CONFIG_H)
#  include  <config.h>
#endif

#include "bashansi.h"
#include <stdio.h>		 

extern char **environ;

int
main (argc, argv) 
     int argc;
     char **argv;
{
  register char **envp, *eval;
  int len;

  argv++;
  argc--;

   
  if (argc == 0)
    {
      for (envp = environ; *envp; envp++)
	puts (*envp);
      exit (0);
    }

   
  len = strlen (*argv);
  for (envp = environ; *envp; envp++)
    {
      if (**argv == **envp && strncmp (*envp, *argv, len) == 0)
	{
	  eval = *envp + len;
	   
	  if (*eval == '=')
	    {
	      puts (eval + 1);
	      exit (0);
	    }
	}
    }
  exit (1);
}
  
