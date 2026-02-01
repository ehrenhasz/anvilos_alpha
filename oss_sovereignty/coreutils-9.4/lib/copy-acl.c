 

#include <config.h>

#include "acl.h"

#include <errno.h>

#include "quote.h"
#include "error.h"
#include "gettext.h"
#define _(msgid) gettext (msgid)


 

int
copy_acl (const char *src_name, int source_desc, const char *dst_name,
          int dest_desc, mode_t mode)
{
  int ret = qcopy_acl (src_name, source_desc, dst_name, dest_desc, mode);
  switch (ret)
    {
    case -2:
      error (0, errno, "%s", quote (src_name));
      break;

    case -1:
      error (0, errno, _("preserving permissions for %s"), quote (dst_name));
      break;

    default:
      break;
    }
  return ret;
}
