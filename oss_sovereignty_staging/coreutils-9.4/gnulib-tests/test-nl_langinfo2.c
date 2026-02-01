 

#include <config.h>

#include <langinfo.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "c-strcase.h"
#include "c-strcasestr.h"
#include "macros.h"

int
main (int argc, char *argv[])
{
#if HAVE_WORKING_USELOCALE
   
  int pass;
  bool skipped_all = true;

   
   
  const char *c_CODESET = nl_langinfo (CODESET);
   
  const char *c_RADIXCHAR = nl_langinfo (RADIXCHAR);
   
  const char *c_T_FMT_AMPM = nl_langinfo (T_FMT_AMPM);
  const char *c_AM_STR = nl_langinfo (AM_STR);
  const char *c_PM_STR = nl_langinfo (PM_STR);
   
  const char *c_CRNCYSTR = nl_langinfo (CRNCYSTR);
   
  const char *c_YESEXPR = nl_langinfo (YESEXPR);

   
  (void) c_CODESET;
  ASSERT (strcmp (c_RADIXCHAR, ".") == 0);
  ASSERT (strlen (c_T_FMT_AMPM) > 0);
  ASSERT (strlen (c_AM_STR) > 0);
  ASSERT (strlen (c_PM_STR) > 0);
  ASSERT (strlen (c_CRNCYSTR) <= 1);  
  ASSERT (c_strcasestr (c_YESEXPR, "y"  ) != NULL);

  for (pass = 1; pass <= 2; pass++)
    {
       
      const char *fr_locale_name =
        getenv (pass == 1 ? "LOCALE_FR" : "LOCALE_FR_UTF8");
      if (strcmp (fr_locale_name, "none") != 0)
        {
           
          locale_t fr_locale = newlocale (LC_ALL_MASK, fr_locale_name, NULL);
          if (fr_locale != NULL)
            {
              uselocale (fr_locale);

               

               
              const char *fr_CODESET = nl_langinfo (CODESET);
              if (pass == 1)
                ASSERT (strstr (fr_CODESET, "8859") != NULL);
              else if (pass == 2)
                ASSERT (c_strcasecmp (fr_CODESET, "UTF-8") == 0
                        || c_strcasecmp (fr_CODESET, "UTF8") == 0);

               
              #if !defined MUSL_LIBC

               
              const char *fr_RADIXCHAR = nl_langinfo (RADIXCHAR);
              ASSERT (strcmp (fr_RADIXCHAR, ",") == 0);

               
               
              #if !((defined __APPLE__ && defined __MACH__) || defined __sun)
              const char *fr_T_FMT_AMPM = nl_langinfo (T_FMT_AMPM);
              const char *fr_AM_STR = nl_langinfo (AM_STR);
              const char *fr_PM_STR = nl_langinfo (PM_STR);
              ASSERT (strlen (fr_T_FMT_AMPM) == 0
                      || strcmp (fr_T_FMT_AMPM, "%I:%M:%S") == 0);
              ASSERT (strlen (fr_AM_STR) == 0);
              ASSERT (strlen (fr_PM_STR) == 0);
              #endif

               
               
              #if !(defined __APPLE__ && defined __MACH__)
              const char *fr_CRNCYSTR = nl_langinfo (CRNCYSTR);
              if (pass == 2)
                ASSERT (strlen (fr_CRNCYSTR) >= 1
                        && strcmp (fr_CRNCYSTR + 1, "€") == 0);
              #endif

              #endif

               
               
              #if !defined MUSL_LIBC
              const char *fr_YESEXPR = nl_langinfo (YESEXPR);
              ASSERT (c_strcasestr (fr_YESEXPR, "o"  ) != NULL);
              #endif

              skipped_all = false;
            }
        }
    }

  if (skipped_all)
    {
      fputs ("Skipping test: French locale is not installed\n", stderr);
      return 77;
    }

  return 0;
#else
  fputs ("Skipping test: uselocale() not available\n", stderr);
  return 77;
#endif
}
