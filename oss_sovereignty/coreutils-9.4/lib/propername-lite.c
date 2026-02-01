 
#include "propername.h"

#include "c-strcase.h"
#include "gettext.h"
#include "localcharset.h"

 

char const *
proper_name_lite (char const *name_ascii, char const *name_utf8)
{
  char const *translation = gettext (name_ascii);
  return (translation != name_ascii ? translation
          : c_strcasecmp (locale_charset (), "UTF-8") == 0 ? name_utf8
          : name_ascii);
}
