 

#include <config.h>

#include "acl.h"

#include <errno.h>

#include "quote.h"
#include "error.h"
#include "gettext.h"
#define _(msgid) gettext (msgid)

 

int
set_acl (char const *name, int desc, mode_t mode)
{
  int ret = qset_acl (name, desc, mode);
  if (ret != 0)
    error (0, errno, _("setting permissions for %s"), quote (name));
  return ret;
}
