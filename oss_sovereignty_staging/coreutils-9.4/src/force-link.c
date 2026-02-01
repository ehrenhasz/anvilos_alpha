 

 

#include <config.h>
#include "system.h"

#include "force-link.h"

#include <tempname.h>

 

static char const simple_pattern[] = "CuXXXXXX";
enum { x_suffix_len = sizeof "XXXXXX" - 1 };

 

enum { smallsize = 256 };

 

static char *
samedir_template (char const *dstname, char buf[smallsize])
{
  ptrdiff_t dstdirlen = last_component (dstname) - dstname;
  size_t dsttmpsize = dstdirlen + sizeof simple_pattern;
  char *dsttmp;
  if (dsttmpsize <= smallsize)
    dsttmp = buf;
  else
    {
      dsttmp = malloc (dsttmpsize);
      if (!dsttmp)
        return dsttmp;
    }
  strcpy (mempcpy (dsttmp, dstname, dstdirlen), simple_pattern);
  return dsttmp;
}


 

struct link_arg
{
  int srcdir;
  char const *srcname;
  int dstdir;
  int flags;
};

static int
try_link (char *dest, void *arg)
{
  struct link_arg *a = arg;
  return linkat (a->srcdir, a->srcname, a->dstdir, dest, a->flags);
}

 
extern int
force_linkat (int srcdir, char const *srcname,
              int dstdir, char const *dstname, int flags, bool force,
              int linkat_errno)
{
  if (linkat_errno < 0)
    linkat_errno = (linkat (srcdir, srcname, dstdir, dstname, flags) == 0
                    ? 0 : errno);
  if (!force || linkat_errno != EEXIST)
    return linkat_errno;

  char buf[smallsize];
  char *dsttmp = samedir_template (dstname, buf);
  if (! dsttmp)
    return errno;
  struct link_arg arg = { srcdir, srcname, dstdir, flags };
  int err;

  if (try_tempname_len (dsttmp, 0, &arg, try_link, x_suffix_len) != 0)
    err = errno;
  else
    {
      err = renameat (dstdir, dsttmp, dstdir, dstname) == 0 ? -1 : errno;
       
      unlinkat (dstdir, dsttmp, 0);
    }

  if (dsttmp != buf)
    free (dsttmp);
  return err;
}


 

struct symlink_arg
{
  char const *srcname;
  int dstdir;
};

static int
try_symlink (char *dest, void *arg)
{
  struct symlink_arg *a = arg;
  return symlinkat (a->srcname, a->dstdir, dest);
}

 
extern int
force_symlinkat (char const *srcname, int dstdir, char const *dstname,
                 bool force, int symlinkat_errno)
{
  if (symlinkat_errno < 0)
    symlinkat_errno = symlinkat (srcname, dstdir, dstname) == 0 ? 0 : errno;
  if (!force || symlinkat_errno != EEXIST)
    return symlinkat_errno;

  char buf[smallsize];
  char *dsttmp = samedir_template (dstname, buf);
  if (!dsttmp)
    return errno;
  struct symlink_arg arg = { srcname, dstdir };
  int err;

  if (try_tempname_len (dsttmp, 0, &arg, try_symlink, x_suffix_len) != 0)
    err = errno;
  else if (renameat (dstdir, dsttmp, dstdir, dstname) != 0)
    {
      err = errno;
      unlinkat (dstdir, dsttmp, 0);
    }
  else
    {
       
      err = -1;
    }

  if (dsttmp != buf)
    free (dsttmp);
  return err;
}
