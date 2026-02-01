 

 

#include <config.h>

#include <stdio.h>

#include "stdc.h"

#include "version.h"
#include "patchlevel.h"
#include "conftypes.h"

#include "bashintl.h"

extern char *shell_name;

 
const char * const dist_version = DISTVERSION;
const int patch_level = PATCHLEVEL;
const int build_version = BUILDVERSION;
#ifdef RELSTATUS
const char * const release_status = RELSTATUS;
#else
const char * const release_status = (char *)0;
#endif
const char * const sccs_version = SCCSVERSION;

const char * const bash_copyright = N_("Copyright (C) 2022 Free Software Foundation, Inc.");
const char * const bash_license = N_("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n");

 
int shell_compatibility_level = DEFAULT_COMPAT_LEVEL;

 

 
extern char *shell_version_string PARAMS((void));
extern void show_shell_version PARAMS((int));

 
char *
shell_version_string ()
{
  static char tt[32] = { '\0' };

  if (tt[0] == '\0')
    {
      if (release_status)
#if HAVE_SNPRINTF
	snprintf (tt, sizeof (tt), "%s.%d(%d)-%s", dist_version, patch_level, build_version, release_status);
#else
	sprintf (tt, "%s.%d(%d)-%s", dist_version, patch_level, build_version, release_status);
#endif
      else
#if HAVE_SNPRINTF
	snprintf (tt, sizeof (tt), "%s.%d(%d)", dist_version, patch_level, build_version);
#else
	sprintf (tt, "%s.%d(%d)", dist_version, patch_level, build_version);
#endif
    }
  return tt;
}

void
show_shell_version (extended)
     int extended;
{
  printf (_("GNU bash, version %s (%s)\n"), shell_version_string (), MACHTYPE);
  if (extended)
    {
      printf ("%s\n", _(bash_copyright));
      printf ("%s\n", _(bash_license));
      printf ("%s\n", _("This is free software; you are free to change and redistribute it."));
      printf ("%s\n", _("There is NO WARRANTY, to the extent permitted by law."));
    }
}
