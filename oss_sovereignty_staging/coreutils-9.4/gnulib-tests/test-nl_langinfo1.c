 

#include <config.h>

#include <langinfo.h>

#include "signature.h"
SIGNATURE_CHECK (nl_langinfo, char *, (nl_item));

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "c-strcase.h"
#include "macros.h"

 
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif

int
main (int argc, char *argv[])
{
  int pass = atoi (argv[1]);
   

  setlocale (LC_ALL, "");

   
  ASSERT (strlen (nl_langinfo (CODESET)) > 0);
  if (pass == 2)
    {
      const char *codeset = nl_langinfo (CODESET);
      ASSERT (c_strcasecmp (codeset, "UTF-8") == 0 || c_strcasecmp (codeset, "UTF8") == 0);
    }
   
  ASSERT (strlen (nl_langinfo (RADIXCHAR)) > 0);
  ASSERT (strlen (nl_langinfo (THOUSEP)) >= 0);
   
  ASSERT (strlen (nl_langinfo (D_T_FMT)) > 0);
  ASSERT (strlen (nl_langinfo (D_FMT)) > 0);
  ASSERT (strlen (nl_langinfo (T_FMT)) > 0);
  ASSERT (strlen (nl_langinfo (T_FMT_AMPM)) >= (pass == 0 ? 1 : 0));
  ASSERT (strlen (nl_langinfo (AM_STR)) >= (pass == 0 ? 1 : 0));
  ASSERT (strlen (nl_langinfo (PM_STR)) >= (pass == 0 ? 1 : 0));
  ASSERT (strlen (nl_langinfo (DAY_1)) > 0);
  ASSERT (strlen (nl_langinfo (DAY_2)) > 0);
  ASSERT (strlen (nl_langinfo (DAY_3)) > 0);
  ASSERT (strlen (nl_langinfo (DAY_4)) > 0);
  ASSERT (strlen (nl_langinfo (DAY_5)) > 0);
  ASSERT (strlen (nl_langinfo (DAY_6)) > 0);
  ASSERT (strlen (nl_langinfo (DAY_7)) > 0);
  ASSERT (strlen (nl_langinfo (ABDAY_1)) > 0);
  ASSERT (strlen (nl_langinfo (ABDAY_2)) > 0);
  ASSERT (strlen (nl_langinfo (ABDAY_3)) > 0);
  ASSERT (strlen (nl_langinfo (ABDAY_4)) > 0);
  ASSERT (strlen (nl_langinfo (ABDAY_5)) > 0);
  ASSERT (strlen (nl_langinfo (ABDAY_6)) > 0);
  ASSERT (strlen (nl_langinfo (ABDAY_7)) > 0);
  ASSERT (strlen (nl_langinfo (MON_1)) > 0);
  ASSERT (strlen (nl_langinfo (MON_2)) > 0);
  ASSERT (strlen (nl_langinfo (MON_3)) > 0);
  ASSERT (strlen (nl_langinfo (MON_4)) > 0);
  ASSERT (strlen (nl_langinfo (MON_5)) > 0);
  ASSERT (strlen (nl_langinfo (MON_6)) > 0);
  ASSERT (strlen (nl_langinfo (MON_7)) > 0);
  ASSERT (strlen (nl_langinfo (MON_8)) > 0);
  ASSERT (strlen (nl_langinfo (MON_9)) > 0);
  ASSERT (strlen (nl_langinfo (MON_10)) > 0);
  ASSERT (strlen (nl_langinfo (MON_11)) > 0);
  ASSERT (strlen (nl_langinfo (MON_12)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_1)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_2)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_3)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_4)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_5)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_6)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_7)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_8)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_9)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_10)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_11)) > 0);
  ASSERT (strlen (nl_langinfo (ALTMON_12)) > 0);
   
  ASSERT (strcmp (nl_langinfo (ALTMON_1), nl_langinfo (MON_1)) == 0);
  ASSERT (strcmp (nl_langinfo (ALTMON_2), nl_langinfo (MON_2)) == 0);
  ASSERT (strcmp (nl_langinfo (ALTMON_3), nl_langinfo (MON_3)) == 0);
  ASSERT (strcmp (nl_langinfo (ALTMON_4), nl_langinfo (MON_4)) == 0);
  ASSERT (strcmp (nl_langinfo (ALTMON_5), nl_langinfo (MON_5)) == 0);
  ASSERT (strcmp (nl_langinfo (ALTMON_6), nl_langinfo (MON_6)) == 0);
  ASSERT (strcmp (nl_langinfo (ALTMON_7), nl_langinfo (MON_7)) == 0);
  ASSERT (strcmp (nl_langinfo (ALTMON_8), nl_langinfo (MON_8)) == 0);
  ASSERT (strcmp (nl_langinfo (ALTMON_9), nl_langinfo (MON_9)) == 0);
  ASSERT (strcmp (nl_langinfo (ALTMON_10), nl_langinfo (MON_10)) == 0);
  ASSERT (strcmp (nl_langinfo (ALTMON_11), nl_langinfo (MON_11)) == 0);
  ASSERT (strcmp (nl_langinfo (ALTMON_12), nl_langinfo (MON_12)) == 0);
  ASSERT (strlen (nl_langinfo (ABMON_1)) > 0);
  ASSERT (strlen (nl_langinfo (ABMON_2)) > 0);
  ASSERT (strlen (nl_langinfo (ABMON_3)) > 0);
  ASSERT (strlen (nl_langinfo (ABMON_4)) > 0);
  ASSERT (strlen (nl_langinfo (ABMON_5)) > 0);
  ASSERT (strlen (nl_langinfo (ABMON_6)) > 0);
  ASSERT (strlen (nl_langinfo (ABMON_7)) > 0);
  ASSERT (strlen (nl_langinfo (ABMON_8)) > 0);
  ASSERT (strlen (nl_langinfo (ABMON_9)) > 0);
  ASSERT (strlen (nl_langinfo (ABMON_10)) > 0);
  ASSERT (strlen (nl_langinfo (ABMON_11)) > 0);
  ASSERT (strlen (nl_langinfo (ABMON_12)) > 0);
  ASSERT (strlen (nl_langinfo (ERA)) >= 0);
  ASSERT (strlen (nl_langinfo (ERA_D_FMT)) >= 0);
  ASSERT (strlen (nl_langinfo (ERA_D_T_FMT)) >= 0);
  ASSERT (strlen (nl_langinfo (ERA_T_FMT)) >= 0);
  ASSERT (nl_langinfo (ALT_DIGITS) != NULL);
   
  {
    const char *currency = nl_langinfo (CRNCYSTR);
    ASSERT (strlen (currency) >= 0);
#if !(defined MUSL_LIBC || defined __NetBSD__)
    if (pass > 0)
      ASSERT (strlen (currency) >= 1);
#endif
  }
   
  ASSERT (strlen (nl_langinfo (YESEXPR)) > 0);
  ASSERT (strlen (nl_langinfo (NOEXPR)) > 0);

  return 0;
}
