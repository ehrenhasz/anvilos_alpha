 

#include <config.h>

 
#include <stdlib.h>

#include <stddef.h>

#if ENABLE_NLS
# include <sys/types.h>
# include <limits.h>
# include <string.h>
# if HAVE_LANGINFO_YESEXPR
#  include <langinfo.h>
# endif
# include <regex.h>
# include "gettext.h"
# define _(msgid) gettext (msgid)
# define N_(msgid) gettext_noop (msgid)

# if HAVE_LANGINFO_YESEXPR
 
static const char *
localized_pattern (const char *english_pattern, nl_item nl_index,
                   bool posixly_correct)
{
  const char *translated_pattern;

   

   
  if (posixly_correct)
    {
      translated_pattern = nl_langinfo (nl_index);
       
      if (translated_pattern != NULL && translated_pattern[0] != '\0')
        return translated_pattern;
   }

   
  translated_pattern = _(english_pattern);
  if (translated_pattern == english_pattern)
    {
       
      translated_pattern = nl_langinfo (nl_index);
       
      if (translated_pattern != NULL && translated_pattern[0] != '\0')
        return translated_pattern;
       
      translated_pattern = english_pattern;
    }
  return translated_pattern;
}
# else
#  define localized_pattern(english_pattern,nl_index,posixly_correct) \
     _(english_pattern)
# endif

static int
try (const char *response, const char *pattern, char **lastp, regex_t *re)
{
  if (*lastp == NULL || strcmp (pattern, *lastp) != 0)
    {
      char *safe_pattern;

       
      if (*lastp != NULL)
        {
           
          regfree (re);
          free (*lastp);
          *lastp = NULL;
        }
       
      safe_pattern = strdup (pattern);
      if (safe_pattern == NULL)
        return -1;
       
      if (regcomp (re, safe_pattern, REG_EXTENDED) != 0)
        {
          free (safe_pattern);
          return -1;
        }
      *lastp = safe_pattern;
    }

   
  return regexec (re, response, 0, NULL, 0) == 0;
}
#endif


int
rpmatch (const char *response)
{
#if ENABLE_NLS
   

   
  static char *last_yesexpr, *last_noexpr;
  static regex_t cached_yesre, cached_nore;

# if HAVE_LANGINFO_YESEXPR
  bool posixly_correct = (getenv ("POSIXLY_CORRECT") != NULL);
# endif

  const char *yesexpr, *noexpr;
  int result;

   
  yesexpr = localized_pattern (N_("^[yY]"), YESEXPR, posixly_correct);
  result = try (response, yesexpr, &last_yesexpr, &cached_yesre);
  if (result < 0)
    return -1;
  if (result)
    return 1;

   
  noexpr = localized_pattern (N_("^[nN]"), NOEXPR, posixly_correct);
  result = try (response, noexpr, &last_noexpr, &cached_nore);
  if (result < 0)
    return -1;
  if (result)
    return 0;

  return -1;
#else
   
  return (*response == 'y' || *response == 'Y' ? 1
          : *response == 'n' || *response == 'N' ? 0 : -1);
#endif
}
