 

#include <config.h>

#include "system.h"
#include "relpath.h"


 
ATTRIBUTE_PURE
static int
path_common_prefix (char const *path1, char const *path2)
{
  int i = 0;
  int ret = 0;

   
  if ((path1[1] == '/') != (path2[1] == '/'))
    return 0;

  while (*path1 && *path2)
    {
      if (*path1 != *path2)
        break;
      if (*path1 == '/')
        ret = i + 1;
      path1++;
      path2++;
      i++;
    }

  if ((!*path1 && !*path2)
      || (!*path1 && *path2 == '/')
      || (!*path2 && *path1 == '/'))
    ret = i;

  return ret;
}

 
static bool
buffer_or_output (char const *str, char **pbuf, size_t *plen)
{
  if (*pbuf)
    {
      size_t slen = strlen (str);
      if (slen >= *plen)
        return true;
      memcpy (*pbuf, str, slen + 1);
      *pbuf += slen;
      *plen -= slen;
    }
  else
    {
      fputs (str, stdout);
    }

  return false;
}

 
bool
relpath (char const *can_fname, char const *can_reldir, char *buf, size_t len)
{
  bool buf_err = false;

   
  int common_index = path_common_prefix (can_reldir, can_fname);
  if (!common_index)
    return false;

  char const *relto_suffix = can_reldir + common_index;
  char const *fname_suffix = can_fname + common_index;

   
  if (*relto_suffix == '/')
    relto_suffix++;
  if (*fname_suffix == '/')
    fname_suffix++;

   
  if (*relto_suffix)
    {
      buf_err |= buffer_or_output ("..", &buf, &len);
      for (; *relto_suffix; ++relto_suffix)
        {
          if (*relto_suffix == '/')
            buf_err |= buffer_or_output ("/..", &buf, &len);
        }

      if (*fname_suffix)
        {
          buf_err |= buffer_or_output ("/", &buf, &len);
          buf_err |= buffer_or_output (fname_suffix, &buf, &len);
        }
    }
  else
    {
        buf_err |= buffer_or_output (*fname_suffix ? fname_suffix : ".",
                                     &buf, &len);
    }

  if (buf_err)
    error (0, ENAMETOOLONG, "%s", _("generating relative path"));

  return !buf_err;
}
