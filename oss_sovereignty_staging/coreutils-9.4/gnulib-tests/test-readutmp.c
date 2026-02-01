 

#include <config.h>

#include "readutmp.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "idx.h"
#include "xalloc.h"

#define ELEMENT STRUCT_UTMP
#define COMPARE(entry1, entry2) \
  _GL_CMP (UT_TIME_MEMBER (entry1), UT_TIME_MEMBER (entry2))
#define STATIC static
#include "array-mergesort.h"

#include "macros.h"

int
main (int argc, char *argv[])
{
  STRUCT_UTMP *entries;
  idx_t num_entries;

  if (read_utmp (UTMP_FILE, &num_entries, &entries, 0) < 0)
    {
      #if READ_UTMP_SUPPORTED
      fprintf (stderr, "Skipping test: cannot open %s\n", UTMP_FILE);
      #else
      fprintf (stderr, "Skipping test: neither <utmpx.h> nor <utmp.h> is available\n");
      #endif
      return 77;
    }

  printf ("Here are the read_utmp results.\n");
  printf ("Flags: B = Boot, U = User Process\n");
  printf ("\n");
  printf ("                                                              Termi‐      Flags\n");
  printf ("    Time (GMT)             User          Device        PID    nation Exit  B U  Host\n");
  printf ("------------------- ------------------ ----------- ---------- ------ ----  - -  ----\n");

   
  if (num_entries > 0)
    {
       
      merge_sort_inplace (entries, num_entries,
                          XNMALLOC (num_entries, STRUCT_UTMP));

      idx_t boot_time_count = 0;
      idx_t i;
      for (i = 0; i < num_entries; i++)
        {
          const STRUCT_UTMP *entry = &entries[i];

          char *user = extract_trimmed_name (entry);
          const char *device = entry->ut_line;
          long pid = UT_PID (entry);
          int termination = UT_EXIT_E_TERMINATION (entry);
          int exit = UT_EXIT_E_EXIT (entry);
          const char *host = entry->ut_host;

          time_t tim = UT_TIME_MEMBER (entry);
          struct tm *gmt = gmtime (&tim);
          char timbuf[100];
          if (gmt == NULL
              || strftime (timbuf, sizeof (timbuf), "%Y-%m-%d %H:%M:%S", gmt)
                 == 0)
            strcpy (timbuf, "---");

          printf ("%-19s %-18s %-11s %10ld %4d   %3d   %c %c  %s\n",
                  timbuf,
                  user,
                  device,
                  pid,
                  termination,
                  exit,
                  UT_TYPE_BOOT_TIME (entry) ? 'X' : ' ',
                  UT_TYPE_USER_PROCESS (entry) ? 'X' : ' ',
                  host);

          if (UT_TYPE_BOOT_TIME (entry))
            boot_time_count++;
        }
      fflush (stdout);

       
      time_t first = UT_TIME_MEMBER (&entries[0]);
      time_t last = UT_TIME_MEMBER (&entries[num_entries - 1]);
      time_t now = time (NULL);
      ASSERT (first >= now - 157680000);
      ASSERT (last <= now + 604800);

       
      ASSERT (boot_time_count <= 1);

       
      ASSERT (boot_time_count >= 1);
    }

  free (entries);

  return 0;
}
