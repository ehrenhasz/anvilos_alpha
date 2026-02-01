 

#include <config.h>

#include "file-type.h"

#include <gettext.h>

char const *
file_type (struct stat const *st)
{
  return gettext (c_file_type (st));
}
