 

#include <config.h>

#include "localcharset.h"

#include <locale.h>
#include <stdio.h>

int
main (void)
{
  setlocale (LC_ALL, "");
  printf ("%s\n", locale_charset ());

  return 0;
}
