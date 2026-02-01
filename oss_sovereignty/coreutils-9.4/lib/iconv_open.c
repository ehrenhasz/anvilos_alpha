 
#include <iconv.h>

#include <errno.h>
#include <string.h>
#include "c-ctype.h"
#include "c-strcase.h"

#define SIZEOF(a) (sizeof(a) / sizeof(a[0]))

 
#define mapping_lookup rpl_iconv_open_mapping_lookup

 

#define ICONV_FLAVOR_AIX "iconv_open-aix.h"
#define ICONV_FLAVOR_HPUX "iconv_open-hpux.h"
#define ICONV_FLAVOR_IRIX "iconv_open-irix.h"
#define ICONV_FLAVOR_OSF "iconv_open-osf.h"
#define ICONV_FLAVOR_SOLARIS "iconv_open-solaris.h"
#define ICONV_FLAVOR_ZOS "iconv_open-zos.h"

#ifdef ICONV_FLAVOR
# include ICONV_FLAVOR
#endif

iconv_t
rpl_iconv_open (const char *tocode, const char *fromcode)
#undef iconv_open
{
  char fromcode_upper[32];
  char tocode_upper[32];
  char *fromcode_upper_end;
  char *tocode_upper_end;

#if REPLACE_ICONV_UTF
   
  if (c_toupper (fromcode[0]) == 'U'
      && c_toupper (fromcode[1]) == 'T'
      && c_toupper (fromcode[2]) == 'F'
      && fromcode[3] == '-')
    {
      if (c_toupper (tocode[0]) == 'U'
          && c_toupper (tocode[1]) == 'T'
          && c_toupper (tocode[2]) == 'F'
          && tocode[3] == '-')
        {
          if (strcmp (fromcode + 4, "8") == 0)
            {
              if (c_strcasecmp (tocode + 4, "16BE") == 0)
                return _ICONV_UTF8_UTF16BE;
              if (c_strcasecmp (tocode + 4, "16LE") == 0)
                return _ICONV_UTF8_UTF16LE;
              if (c_strcasecmp (tocode + 4, "32BE") == 0)
                return _ICONV_UTF8_UTF32BE;
              if (c_strcasecmp (tocode + 4, "32LE") == 0)
                return _ICONV_UTF8_UTF32LE;
            }
          else if (strcmp (tocode + 4, "8") == 0)
            {
              if (c_strcasecmp (fromcode + 4, "16BE") == 0)
                return _ICONV_UTF16BE_UTF8;
              if (c_strcasecmp (fromcode + 4, "16LE") == 0)
                return _ICONV_UTF16LE_UTF8;
              if (c_strcasecmp (fromcode + 4, "32BE") == 0)
                return _ICONV_UTF32BE_UTF8;
              if (c_strcasecmp (fromcode + 4, "32LE") == 0)
                return _ICONV_UTF32LE_UTF8;
            }
        }
    }
#endif

   

   
  {
    iconv_t cd = iconv_open (tocode, fromcode);
    if (cd != (iconv_t)(-1))
      return cd;
  }

   
  {
    const char *p = fromcode;
    char *q = fromcode_upper;
    while ((*q = c_toupper (*p)) != '\0')
      {
        p++;
        q++;
        if (q == &fromcode_upper[SIZEOF (fromcode_upper)])
          {
            errno = EINVAL;
            return (iconv_t)(-1);
          }
      }
    fromcode_upper_end = q;
  }

  {
    const char *p = tocode;
    char *q = tocode_upper;
    while ((*q = c_toupper (*p)) != '\0')
      {
        p++;
        q++;
        if (q == &tocode_upper[SIZEOF (tocode_upper)])
          {
            errno = EINVAL;
            return (iconv_t)(-1);
          }
      }
    tocode_upper_end = q;
  }

#ifdef ICONV_FLAVOR
   
  {
    const struct mapping *m =
      mapping_lookup (fromcode_upper, fromcode_upper_end - fromcode_upper);

    fromcode = (m != NULL ? m->vendor_name : fromcode_upper);
  }
  {
    const struct mapping *m =
      mapping_lookup (tocode_upper, tocode_upper_end - tocode_upper);

    tocode = (m != NULL ? m->vendor_name : tocode_upper);
  }
#else
  fromcode = fromcode_upper;
  tocode = tocode_upper;
#endif

  return iconv_open (tocode, fromcode);
}
