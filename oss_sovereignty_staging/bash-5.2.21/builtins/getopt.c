 

 

#include <config.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include <stdio.h>
#include "memalloc.h"
#include "../bashintl.h"
#include "../shell.h"
#include "getopt.h"

 
char *sh_optarg = 0;

 

 
int sh_optind = 0;

 
static int sh_curopt;

 

static char *nextchar;
static int sh_charindex;

 

int sh_opterr = 1;

 

int sh_optopt = '?';

 
int sh_badopt = 0;

 

 
#define BADOPT(x)  fprintf (stderr, _("%s: illegal option -- %c\n"), argv[0], x)
#define NEEDARG(x) fprintf (stderr, _("%s: option requires an argument -- %c\n"), argv[0], x)

int
sh_getopt (argc, argv, optstring)
     int argc;
     char *const *argv;
     const char *optstring;
{
  char c, *temp;

  sh_optarg = 0;

  if (sh_optind >= argc || sh_optind < 0)	 
    {
      sh_optind = argc;
      return (EOF);
    }

   

  if (sh_optind == 0)
    {
      sh_optind = 1;
      nextchar = (char *)NULL;
    }

  if (nextchar == 0 || *nextchar == '\0')
    {
       
      if (sh_optind >= argc)
	return EOF;

      temp = argv[sh_optind];

       
      if (temp[0] == '-' && temp[1] == '-' && temp[2] == '\0')
	{
	  sh_optind++;
	  return EOF;
	}

       
      if (temp[0] != '-' || temp[1] == '\0')
	return EOF;

       
      nextchar = argv[sh_curopt = sh_optind] + 1;
      sh_charindex = 1;
    }

   

  c = *nextchar++; sh_charindex++;
  temp = strchr (optstring, c);

  sh_optopt = c;

   
  if (nextchar == 0 || *nextchar == '\0')
    {
      sh_optind++;
      nextchar = (char *)NULL;
    }

  if (sh_badopt = (temp == NULL || c == ':'))
    {
      if (sh_opterr)
	BADOPT (c);

      return '?';
    }

  if (temp[1] == ':')
    {
      if (nextchar && *nextchar)
	{
	   
	  sh_optarg = nextchar;
	   
	  sh_optind++;
	}
      else if (sh_optind == argc)
	{
	  if (sh_opterr)
	    NEEDARG (c);

	  sh_optopt = c;
	  sh_optarg = "";	 
	  c = (optstring[0] == ':') ? ':' : '?';
	}
      else
	 
	sh_optarg = argv[sh_optind++];
      nextchar = (char *)NULL;
    }
  return c;
}

void
sh_getopt_restore_state (argv)
     char **argv;
{
  if (nextchar)
    nextchar = argv[sh_curopt] + sh_charindex;
}

sh_getopt_state_t *
sh_getopt_alloc_istate ()
{
  sh_getopt_state_t *ret;

  ret = (sh_getopt_state_t *)xmalloc (sizeof (sh_getopt_state_t));
  return ret;
}

void
sh_getopt_dispose_istate (gs)
     sh_getopt_state_t *gs;
{
  free (gs);
}

sh_getopt_state_t *
sh_getopt_save_istate ()
{
  sh_getopt_state_t *ret;

  ret = sh_getopt_alloc_istate ();

  ret->gs_optarg = sh_optarg;
  ret->gs_optind = sh_optind;
  ret->gs_curopt = sh_curopt;
  ret->gs_nextchar = nextchar;		 
  ret->gs_charindex = sh_charindex;
  ret->gs_flags = 0;			 

  return ret;
}

void
sh_getopt_restore_istate (state)
     sh_getopt_state_t *state;
{
  sh_optarg = state->gs_optarg;
  sh_optind = state->gs_optind;
  sh_curopt = state->gs_curopt;
  nextchar = state->gs_nextchar;	 
  sh_charindex = state->gs_charindex;

  sh_getopt_dispose_istate (state);
}

#if 0
void
sh_getopt_debug_restore_state (argv)
     char **argv;
{
  if (nextchar && nextchar != argv[sh_curopt] + sh_charindex)
    {
      itrace("sh_getopt_debug_restore_state: resetting nextchar");
      nextchar = argv[sh_curopt] + sh_charindex;
    }
}
#endif
 
#ifdef TEST

 

int
main (argc, argv)
     int argc;
     char **argv;
{
  int c;
  int digit_sh_optind = 0;

  while (1)
    {
      int this_option_sh_optind = sh_optind ? sh_optind : 1;

      c = sh_getopt (argc, argv, "abc:d:0123456789");
      if (c == EOF)
	break;

      switch (c)
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  if (digit_sh_optind != 0 && digit_sh_optind != this_option_sh_optind)
	    printf ("digits occur in two different argv-elements.\n");
	  digit_sh_optind = this_option_sh_optind;
	  printf ("option %c\n", c);
	  break;

	case 'a':
	  printf ("option a\n");
	  break;

	case 'b':
	  printf ("option b\n");
	  break;

	case 'c':
	  printf ("option c with value `%s'\n", sh_optarg);
	  break;

	case '?':
	  break;

	default:
	  printf ("?? sh_getopt returned character code 0%o ??\n", c);
	}
    }

  if (sh_optind < argc)
    {
      printf ("non-option ARGV-elements: ");
      while (sh_optind < argc)
	printf ("%s ", argv[sh_optind++]);
      printf ("\n");
    }

  exit (0);
}

#endif  
