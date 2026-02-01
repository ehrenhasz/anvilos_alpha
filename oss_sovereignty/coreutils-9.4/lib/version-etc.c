 

#include <config.h>

 
#include "version-etc.h"

#include <stdarg.h>
#include <stdio.h>

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

#include "gettext.h"
#define _(msgid) gettext (msgid)

 
#if ! defined PACKAGE && defined PACKAGE_TARNAME
# define PACKAGE PACKAGE_TARNAME
#endif

enum { COPYRIGHT_YEAR = 2023 };

 

 
void
version_etc_arn (FILE *stream,
                 const char *command_name, const char *package,
                 const char *version,
                 const char * const * authors, size_t n_authors)
{
  if (command_name)
    fprintf (stream, "%s (%s) %s\n", command_name, package, version);
  else
    fprintf (stream, "%s %s\n", package, version);

#ifdef PACKAGE_PACKAGER
# ifdef PACKAGE_PACKAGER_VERSION
  fprintf (stream, _("Packaged by %s (%s)\n"), PACKAGE_PACKAGER,
           PACKAGE_PACKAGER_VERSION);
# else
  fprintf (stream, _("Packaged by %s\n"), PACKAGE_PACKAGER);
# endif
#endif

   
  fprintf (stream, version_etc_copyright, _("(C)"), COPYRIGHT_YEAR);

  fputs ("\n", stream);

   
  fprintf (stream, _("\
License GPLv3+: GNU GPL version 3 or later <%s>.\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
"),
           "https:

  fputs ("\n", stream);

  switch (n_authors)
    {
    case 0:
       
      break;
    case 1:
       
      fprintf (stream, _("Written by %s.\n"), authors[0]);
      break;
    case 2:
       
      fprintf (stream, _("Written by %s and %s.\n"), authors[0], authors[1]);
      break;
    case 3:
       
      fprintf (stream, _("Written by %s, %s, and %s.\n"),
               authors[0], authors[1], authors[2]);
      break;
    case 4:
       
      fprintf (stream, _("Written by %s, %s, %s,\nand %s.\n"),
               authors[0], authors[1], authors[2], authors[3]);
      break;
    case 5:
       
      fprintf (stream, _("Written by %s, %s, %s,\n%s, and %s.\n"),
               authors[0], authors[1], authors[2], authors[3], authors[4]);
      break;
    case 6:
       
      fprintf (stream, _("Written by %s, %s, %s,\n%s, %s, and %s.\n"),
               authors[0], authors[1], authors[2], authors[3], authors[4],
               authors[5]);
      break;
    case 7:
       
      fprintf (stream, _("Written by %s, %s, %s,\n%s, %s, %s, and %s.\n"),
               authors[0], authors[1], authors[2], authors[3], authors[4],
               authors[5], authors[6]);
      break;
    case 8:
       
      fprintf (stream, _("\
Written by %s, %s, %s,\n%s, %s, %s, %s,\nand %s.\n"),
                authors[0], authors[1], authors[2], authors[3], authors[4],
                authors[5], authors[6], authors[7]);
      break;
    case 9:
      /* TRANSLATORS: Each %s denotes an author name.
         You can use line breaks, estimating that each author name occupies
         ca. 16 screen columns and that a screen line has ca. 80 columns.  */
      fprintf (stream, _("\
Written by %s, %s, %s,\n%s, %s, %s, %s,\n%s, and %s.\n"),
               authors[0], authors[1], authors[2], authors[3], authors[4],
               authors[5], authors[6], authors[7], authors[8]);
      break;
    default:
      /* 10 or more authors.  Use an abbreviation, since the human reader
         will probably not want to read the entire list anyway.  */
      /* TRANSLATORS: Each %s denotes an author name.
         You can use line breaks, estimating that each author name occupies
         ca. 16 screen columns and that a screen line has ca. 80 columns.  */
      fprintf (stream, _("\
Written by %s, %s, %s,\n%s, %s, %s, %s,\n%s, %s, and others.\n"),
                authors[0], authors[1], authors[2], authors[3], authors[4],
                authors[5], authors[6], authors[7], authors[8]);
      break;
    }
}

/* Display the --version information the standard way.  See the initial
   comment to this module, for more information.

   Author names are given in the NULL-terminated array AUTHORS. */
void
version_etc_ar (FILE *stream,
                const char *command_name, const char *package,
                const char *version, const char * const * authors)
{
  size_t n_authors;

  for (n_authors = 0; authors[n_authors]; n_authors++)
    ;
  version_etc_arn (stream, command_name, package, version, authors, n_authors);
}

/* Display the --version information the standard way.  See the initial
   comment to this module, for more information.

   Author names are given in the NULL-terminated va_list AUTHORS. */
void
version_etc_va (FILE *stream,
                const char *command_name, const char *package,
                const char *version, va_list authors)
{
  size_t n_authors;
  const char *authtab[10];

  for (n_authors = 0;
       n_authors < 10
         && (authtab[n_authors] = va_arg (authors, const char *)) != NULL;
       n_authors++)
    ;
  version_etc_arn (stream, command_name, package, version,
                   authtab, n_authors);
}


/* Display the --version information the standard way.

   If COMMAND_NAME is NULL, the PACKAGE is assumed to be the name of
   the program.  The formats are therefore:

   PACKAGE VERSION

   or

   COMMAND_NAME (PACKAGE) VERSION.

   The authors names are passed as separate arguments, with an additional
   NULL argument at the end.  */
void
version_etc (FILE *stream,
             const char *command_name, const char *package,
             const char *version, /* const char *author1, ...*/ ...)
{
  va_list authors;

  va_start (authors, version);
  version_etc_va (stream, command_name, package, version, authors);
  va_end (authors);
}

void
emit_bug_reporting_address (void)
{
  fputs ("\n", stdout);
  /* TRANSLATORS: The placeholder indicates the bug-reporting address
     for this package.  Please add _another line_ saying
     "Report translation bugs to <...>\n" with the address for translation
     bugs (typically your translation team's web or email address).  */
  printf (_("Report bugs to: %s\n"), PACKAGE_BUGREPORT);
#ifdef PACKAGE_PACKAGER_BUG_REPORTS
  printf (_("Report %s bugs to: %s\n"), PACKAGE_PACKAGER,
          PACKAGE_PACKAGER_BUG_REPORTS);
#endif
#ifdef PACKAGE_URL
  printf (_("%s home page: <%s>\n"), PACKAGE_NAME, PACKAGE_URL);
#else
  printf (_("%s home page: <%s>\n"),
          PACKAGE_NAME, "https:
#endif
  printf (_("General help using GNU software: <%s>\n"),
          "https://www.gnu.org/gethelp/");
}
