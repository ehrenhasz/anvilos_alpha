 

 

#include "config.h"

#include "stdc.h"

#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "bashansi.h"

#include "version.h"
#include "conftypes.h"

#define RFLAG	0x0001
#define VFLAG	0x0002
#define MFLAG	0x0004
#define PFLAG	0x0008
#define SFLAG	0x0010
#define LFLAG	0x0020
#define XFLAG	0x0040

extern int optind;
extern char *optarg;

extern char *dist_version;
extern int patch_level;

extern char *shell_version_string PARAMS((void));
extern void show_shell_version PARAMS((int));

char *shell_name = "bash";
char *progname;

static void
usage()
{
  fprintf(stderr, "%s: usage: %s [-hrvpmlsx]\n", progname, progname);
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  int opt, oflags;
  char dv[128], *rv;

  if (progname = strrchr (argv[0], '/'))
    progname++;
  else
    progname = argv[0];

  oflags = 0;
  while ((opt = getopt(argc, argv, "hrvmpslx")) != EOF)
    {
      switch (opt)
	{
	case 'h':
	  usage ();
	  exit (0);
	case 'r':
	  oflags |= RFLAG;	 
	  break;
	case 'v':
	  oflags |= VFLAG;	 
	  break;
	case 'm':
	  oflags |= MFLAG;	 
	  break;
	case 'p':
	  oflags |= PFLAG;	 
	  break;
	case 's':		 
	  oflags |= SFLAG;
	  break;
	case 'l':		 
	  oflags |= LFLAG;
	  break;
	case 'x':		 
	  oflags |= XFLAG;
	  break;
	default:
	  usage ();
	  exit (2);
	}
    }

  argc -= optind;
  argv += optind;

  if (argc > 0)
    {
      usage ();
      exit (2);
    }

     
  if (oflags == 0)
    oflags = SFLAG;

  if (oflags & (RFLAG|VFLAG))
    {
      strcpy (dv, dist_version);
      rv = strchr (dv, '.');
      if (rv)
        *rv++ = '\0';
      else
        rv = "00";
    }
  if (oflags & RFLAG)
    printf ("%s\n", dv);
  else if (oflags & VFLAG)
    printf ("%s\n", rv);
  else if (oflags & MFLAG)
    printf ("%s\n", MACHTYPE);
  else if (oflags & PFLAG)
    printf ("%d\n", patch_level);
  else if (oflags & SFLAG)
    printf ("%s\n", shell_version_string ());
  else if (oflags & LFLAG)
    show_shell_version (0);
  else if (oflags & XFLAG)
    show_shell_version (1);

  exit (0);
}
