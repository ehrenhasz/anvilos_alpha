 

#include <config.h>

#include "mkdir-p.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)

#include "dirchownmod.h"
#include "dirname.h"
#include "error.h"
#include "quote.h"
#include "mkancesdirs.h"
#include "savewd.h"

#ifndef HAVE_FCHMOD
# define HAVE_FCHMOD false
#endif

 

bool
make_dir_parents (char *dir,
                  struct savewd *wd,
                  int (*make_ancestor) (char const *, char const *, void *),
                  void *options,
                  mode_t mode,
                  void (*announce) (char const *, void *),
                  mode_t mode_bits,
                  uid_t owner,
                  gid_t group,
                  bool preserve_existing)
{
  int mkdir_errno = (IS_ABSOLUTE_FILE_NAME (dir) ? 0 : savewd_errno (wd));

  if (mkdir_errno == 0)
    {
      ptrdiff_t prefix_len = 0;
      int savewd_chdir_options = (HAVE_FCHMOD ? SAVEWD_CHDIR_SKIP_READABLE : 0);

      if (make_ancestor)
        {
          prefix_len = mkancesdirs (dir, wd, make_ancestor, options);
          if (prefix_len < 0)
            {
              if (prefix_len < -1)
                return true;
              mkdir_errno = errno;
            }
        }

      if (0 <= prefix_len)
        {
           
          bool keep_owner = owner == (uid_t) -1 && group == (gid_t) -1;
          bool keep_special_mode_bits =
            ((mode_bits & (S_ISUID | S_ISGID)) | (mode & S_ISVTX)) == 0;
          mode_t mkdir_mode = mode;
          if (! keep_owner)
            mkdir_mode &= ~ (S_IRWXG | S_IRWXO);
          else if (! keep_special_mode_bits)
            mkdir_mode &= ~ (S_IWGRP | S_IWOTH);

          if (mkdir (dir + prefix_len, mkdir_mode) == 0)
            {
               
              bool umask_must_be_ok = (mode & mode_bits & S_IRWXUGO) == 0;

              announce (dir, options);
              preserve_existing = (keep_owner & keep_special_mode_bits
                                   & umask_must_be_ok);
              savewd_chdir_options |= SAVEWD_CHDIR_NOFOLLOW;
            }
          else
            {
              mkdir_errno = errno;
              mkdir_mode = -1;
            }

          if (preserve_existing)
            {
              if (mkdir_errno == 0)
                return true;
              if (mkdir_errno != ENOENT && make_ancestor)
                {
                  struct stat st;
                  if (stat (dir + prefix_len, &st) == 0)
                    {
                      if (S_ISDIR (st.st_mode))
                        return true;
                    }
                  else if (mkdir_errno == EEXIST
                           && errno != ENOENT && errno != ENOTDIR)
                    {
                      error (0, errno, _("cannot stat %s"), quote (dir));
                      return false;
                    }
                }
            }
          else
            {
              int open_result[2];
              int chdir_result =
                savewd_chdir (wd, dir + prefix_len,
                              savewd_chdir_options, open_result);
              if (chdir_result < -1)
                return true;
              else
                {
                  bool chdir_ok = (chdir_result == 0);
                  char const *subdir = (chdir_ok ? "." : dir + prefix_len);
                  if (dirchownmod (open_result[0], subdir, mkdir_mode,
                                   owner, group, mode, mode_bits)
                      == 0)
                    return true;

                  if (mkdir_errno == 0
                      || (mkdir_errno != ENOENT && make_ancestor
                          && errno != ENOTDIR))
                    {
                      error (0, errno,
                             _(keep_owner
                               ? "cannot change permissions of %s"
                               : "cannot change owner and permissions of %s"),
                             quote (dir));
                      return false;
                    }
                }
            }
        }
    }

  error (0, mkdir_errno, _("cannot create directory %s"), quote (dir));
  return false;
}
