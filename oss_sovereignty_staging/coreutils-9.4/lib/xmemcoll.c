 

#include <config.h>

#include <errno.h>
#include <stdlib.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)

#include "error.h"
#include "exitfail.h"
#include "memcoll.h"
#include "quotearg.h"
#include "xmemcoll.h"

static void
collate_error (int collation_errno,
               char const *s1, size_t s1len,
               char const *s2, size_t s2len)
{
  error (0, collation_errno, _("string comparison failed"));
  error (0, 0, _("Set LC_ALL='C' to work around the problem."));
  error (exit_failure, 0,
         _("The strings compared were %s and %s."),
         quotearg_n_style_mem (0, locale_quoting_style, s1, s1len),
         quotearg_n_style_mem (1, locale_quoting_style, s2, s2len));
}

 

int
xmemcoll (char *s1, size_t s1len, char *s2, size_t s2len)
{
  int diff = memcoll (s1, s1len, s2, s2len);
  int collation_errno = errno;
  if (collation_errno)
    collate_error (collation_errno, s1, s1len, s2, s2len);
  return diff;
}

 

int
xmemcoll0 (char const *s1, size_t s1size, char const *s2, size_t s2size)
{
  int diff = memcoll0 (s1, s1size, s2, s2size);
  int collation_errno = errno;
  if (collation_errno)
    collate_error (collation_errno, s1, s1size - 1, s2, s2size - 1);
  return diff;
}
