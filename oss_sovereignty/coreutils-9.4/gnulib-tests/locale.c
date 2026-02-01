 

#include <config.h>

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

 
#undef setlocale

 

static const char *
defaulted_getenv (const char *variable)
{
  const char *value = getenv (variable);
  return (value != NULL ? value : "");
}

static void
print_category (int category, const char *variable)
{
  const char *value = defaulted_getenv (variable);
  if (value[0] != '\0' && defaulted_getenv ("LC_ALL")[0] == '\0')
     
    printf ("%s=%s\n", variable, value);
  else
    printf ("%s=\"%s\"\n", variable, setlocale (category, NULL));
}

int
main (void)
{
  setlocale (LC_ALL, "");

  printf ("LANG=%s\n", defaulted_getenv ("LANG"));
  print_category (LC_CTYPE, "LC_CTYPE");
  print_category (LC_NUMERIC, "LC_NUMERIC");
  print_category (LC_TIME, "LC_TIME");
  print_category (LC_COLLATE, "LC_COLLATE");
  print_category (LC_MONETARY, "LC_MONETARY");
  print_category (LC_MESSAGES, "LC_MESSAGES");
#ifdef LC_PAPER
  print_category (LC_PAPER, "LC_PAPER");
#endif
#ifdef LC_NAME
  print_category (LC_NAME, "LC_NAME");
#endif
#ifdef LC_ADDRESS
  print_category (LC_ADDRESS, "LC_ADDRESS");
#endif
#ifdef LC_TELEPHONE
  print_category (LC_TELEPHONE, "LC_TELEPHONE");
#endif
#ifdef LC_MEASUREMENT
  print_category (LC_MEASUREMENT, "LC_MEASUREMENT");
#endif
#ifdef LC_IDENTIFICATION
  print_category (LC_IDENTIFICATION, "LC_IDENTIFICATION");
#endif

  printf ("LC_ALL=%s\n", defaulted_getenv ("LC_ALL"));

  return 0;
}
