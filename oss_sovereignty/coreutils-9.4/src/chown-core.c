 

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "system.h"
#include "assure.h"
#include "chown-core.h"
#include "ignore-value.h"
#include "root-dev-ino.h"
#include "xfts.h"

#define FTSENT_IS_DIRECTORY(E)	\
  ((E)->fts_info == FTS_D	\
   || (E)->fts_info == FTS_DC	\
   || (E)->fts_info == FTS_DP	\
   || (E)->fts_info == FTS_DNR)

enum RCH_status
  {
     
    RC_ok = 2,

     
    RC_excluded,

     
    RC_inode_changed,

     
    RC_do_ordinary_chown,

     
    RC_error
  };

extern void
chopt_init (struct Chown_option *chopt)
{
  chopt->verbosity = V_off;
  chopt->root_dev_ino = nullptr;
  chopt->affect_symlink_referent = true;
  chopt->recurse = false;
  chopt->force_silent = false;
  chopt->user_name = nullptr;
  chopt->group_name = nullptr;
}

extern void
chopt_free (struct Chown_option *chopt)
{
  free (chopt->user_name);
  free (chopt->group_name);
}

 

static char *
uid_to_str (uid_t uid)
{
  char buf[INT_BUFSIZE_BOUND (intmax_t)];
  return xstrdup (TYPE_SIGNED (uid_t) ? imaxtostr (uid, buf)
                  : umaxtostr (uid, buf));
}

 

static char *
gid_to_str (gid_t gid)
{
  char buf[INT_BUFSIZE_BOUND (intmax_t)];
  return xstrdup (TYPE_SIGNED (gid_t) ? imaxtostr (gid, buf)
                  : umaxtostr (gid, buf));
}

 

extern char *
gid_to_name (gid_t gid)
{
  struct group *grp = getgrgid (gid);
  return grp ? xstrdup (grp->gr_name) : gid_to_str (gid);
}

 

extern char *
uid_to_name (uid_t uid)
{
  struct passwd *pwd = getpwuid (uid);
  return pwd ? xstrdup (pwd->pw_name) : uid_to_str (uid);
}

 

static char *
user_group_str (char const *user, char const *group)
{
  char *spec = nullptr;

  if (user)
    {
      if (group)
        {
          spec = xmalloc (strlen (user) + 1 + strlen (group) + 1);
          stpcpy (stpcpy (stpcpy (spec, user), ":"), group);
        }
      else
        {
          spec = xstrdup (user);
        }
    }
  else if (group)
    {
      spec = xstrdup (group);
    }

  return spec;
}

 

static void
describe_change (char const *file, enum Change_status changed,
                 char const *old_user, char const *old_group,
                 char const *user, char const *group)
{
  char const *fmt;
  char *old_spec;
  char *spec;

  if (changed == CH_NOT_APPLIED)
    {
      printf (_("neither symbolic link %s nor referent has been changed\n"),
              quoteaf (file));
      return;
    }

  spec = user_group_str (user, group);
  old_spec = user_group_str (user ? old_user : nullptr,
                             group ? old_group : nullptr);

  switch (changed)
    {
    case CH_SUCCEEDED:
      fmt = (user ? _("changed ownership of %s from %s to %s\n")
             : group ? _("changed group of %s from %s to %s\n")
             : _("no change to ownership of %s\n"));
      break;
    case CH_FAILED:
      if (old_spec)
        {
          fmt = (user ? _("failed to change ownership of %s from %s to %s\n")
                 : group ? _("failed to change group of %s from %s to %s\n")
                 : _("failed to change ownership of %s\n"));
        }
      else
        {
          fmt = (user ? _("failed to change ownership of %s to %s\n")
                 : group ? _("failed to change group of %s to %s\n")
                 : _("failed to change ownership of %s\n"));
          free (old_spec);
          old_spec = spec;
          spec = nullptr;
        }
      break;
    case CH_NO_CHANGE_REQUESTED:
      fmt = (user ? _("ownership of %s retained as %s\n")
             : group ? _("group of %s retained as %s\n")
             : _("ownership of %s retained\n"));
      break;
    default:
      affirm (false);
    }

  printf (fmt, quoteaf (file), old_spec, spec);

  free (old_spec);
  free (spec);
}

 

static enum RCH_status
restricted_chown (int cwd_fd, char const *file,
                  struct stat const *orig_st,
                  uid_t uid, gid_t gid,
                  uid_t required_uid, gid_t required_gid)
{
  enum RCH_status status = RC_ok;
  struct stat st;
  int open_flags = O_NONBLOCK | O_NOCTTY;
  int fd;

  if (required_uid == (uid_t) -1 && required_gid == (gid_t) -1)
    return RC_do_ordinary_chown;

  if (! S_ISREG (orig_st->st_mode))
    {
      if (S_ISDIR (orig_st->st_mode))
        open_flags |= O_DIRECTORY;
      else
        return RC_do_ordinary_chown;
    }

  fd = openat (cwd_fd, file, O_RDONLY | open_flags);
  if (! (0 <= fd
         || (errno == EACCES && S_ISREG (orig_st->st_mode)
             && 0 <= (fd = openat (cwd_fd, file, O_WRONLY | open_flags)))))
    return (errno == EACCES ? RC_do_ordinary_chown : RC_error);

  if (fstat (fd, &st) != 0)
    status = RC_error;
  else if (! SAME_INODE (*orig_st, st))
    status = RC_inode_changed;
  else if ((required_uid == (uid_t) -1 || required_uid == st.st_uid)
           && (required_gid == (gid_t) -1 || required_gid == st.st_gid))
    {
      if (fchown (fd, uid, gid) == 0)
        {
          status = (close (fd) == 0
                    ? RC_ok : RC_error);
          return status;
        }
      else
        {
          status = RC_error;
        }
    }

  int saved_errno = errno;
  close (fd);
  errno = saved_errno;
  return status;
}

 
static bool
change_file_owner (FTS *fts, FTSENT *ent,
                   uid_t uid, gid_t gid,
                   uid_t required_uid, gid_t required_gid,
                   struct Chown_option const *chopt)
{
  char const *file_full_name = ent->fts_path;
  char const *file = ent->fts_accpath;
  struct stat const *file_stats;
  struct stat stat_buf;
  bool ok = true;
  bool do_chown;
  bool symlink_changed = true;

  switch (ent->fts_info)
    {
    case FTS_D:
      if (chopt->recurse)
        {
          if (ROOT_DEV_INO_CHECK (chopt->root_dev_ino, ent->fts_statp))
            {
               
              ROOT_DEV_INO_WARN (file_full_name);
               
              fts_set (fts, ent, FTS_SKIP);
               
              ignore_value (fts_read (fts));
              return false;
            }
          return true;
        }
      break;

    case FTS_DP:
      if (! chopt->recurse)
        return true;
      break;

    case FTS_NS:
       
      if (ent->fts_level == 0 && ent->fts_number == 0)
        {
          ent->fts_number = 1;
          fts_set (fts, ent, FTS_AGAIN);
          return true;
        }
      if (! chopt->force_silent)
        error (0, ent->fts_errno, _("cannot access %s"),
               quoteaf (file_full_name));
      ok = false;
      break;

    case FTS_ERR:
      if (! chopt->force_silent)
        error (0, ent->fts_errno, "%s", quotef (file_full_name));
      ok = false;
      break;

    case FTS_DNR:
      if (! chopt->force_silent)
        error (0, ent->fts_errno, _("cannot read directory %s"),
               quoteaf (file_full_name));
      ok = false;
      break;

    case FTS_DC:		 
      if (cycle_warning_required (fts, ent))
        {
          emit_cycle_warning (file_full_name);
          return false;
        }
      break;

    default:
      break;
    }

  if (!ok)
    {
      do_chown = false;
      file_stats = nullptr;
    }
  else if (required_uid == (uid_t) -1 && required_gid == (gid_t) -1
           && chopt->verbosity == V_off
           && ! chopt->root_dev_ino
           && ! chopt->affect_symlink_referent)
    {
      do_chown = true;
      file_stats = ent->fts_statp;
    }
  else
    {
      file_stats = ent->fts_statp;

       
      if (chopt->affect_symlink_referent && S_ISLNK (file_stats->st_mode))
        {
          if (fstatat (fts->fts_cwd_fd, file, &stat_buf, 0) != 0)
            {
              if (! chopt->force_silent)
                error (0, errno, _("cannot dereference %s"),
                       quoteaf (file_full_name));
              ok = false;
            }

          file_stats = &stat_buf;
        }

      do_chown = (ok
                  && (required_uid == (uid_t) -1
                      || required_uid == file_stats->st_uid)
                  && (required_gid == (gid_t) -1
                      || required_gid == file_stats->st_gid));
    }

   
  if (ok
      && FTSENT_IS_DIRECTORY (ent)
      && ROOT_DEV_INO_CHECK (chopt->root_dev_ino, file_stats))
    {
      ROOT_DEV_INO_WARN (file_full_name);
      return false;
    }

  if (do_chown)
    {
      if ( ! chopt->affect_symlink_referent)
        {
          ok = (lchownat (fts->fts_cwd_fd, file, uid, gid) == 0);

           
          if (!ok && errno == EOPNOTSUPP)
            {
              ok = true;
              symlink_changed = false;
            }
        }
      else
        {
           

          enum RCH_status err
            = restricted_chown (fts->fts_cwd_fd, file, file_stats, uid, gid,
                                required_uid, required_gid);
          switch (err)
            {
            case RC_ok:
              break;

            case RC_do_ordinary_chown:
              ok = (chownat (fts->fts_cwd_fd, file, uid, gid) == 0);
              break;

            case RC_error:
              ok = false;
              break;

            case RC_inode_changed:
               
            case RC_excluded:
              do_chown = false;
              ok = false;
              break;

            default:
              unreachable ();
            }
        }

       

      if (do_chown && !ok && ! chopt->force_silent)
        error (0, errno, (uid != (uid_t) -1
                          ? _("changing ownership of %s")
                          : _("changing group of %s")),
               quoteaf (file_full_name));
    }

  if (chopt->verbosity != V_off)
    {
      bool changed =
        ((do_chown && ok && symlink_changed)
         && ! ((uid == (uid_t) -1 || uid == file_stats->st_uid)
               && (gid == (gid_t) -1 || gid == file_stats->st_gid)));

      if (changed || chopt->verbosity == V_high)
        {
          enum Change_status ch_status =
            (!ok ? CH_FAILED
             : !symlink_changed ? CH_NOT_APPLIED
             : !changed ? CH_NO_CHANGE_REQUESTED
             : CH_SUCCEEDED);
          char *old_usr = (file_stats
                           ? uid_to_name (file_stats->st_uid) : nullptr);
          char *old_grp = (file_stats
                           ? gid_to_name (file_stats->st_gid) : nullptr);
          char *new_usr = chopt->user_name
                          ? chopt->user_name : uid != -1
                                               ? uid_to_str (uid) : nullptr;
          char *new_grp = chopt->group_name
                          ? chopt->group_name : gid != -1
                                               ? gid_to_str (gid) : nullptr;
          describe_change (file_full_name, ch_status,
                           old_usr, old_grp,
                           new_usr, new_grp);
          free (old_usr);
          free (old_grp);
          if (new_usr != chopt->user_name)
            free (new_usr);
          if (new_grp != chopt->group_name)
            free (new_grp);
        }
    }

  if ( ! chopt->recurse)
    fts_set (fts, ent, FTS_SKIP);

  return ok;
}

 
extern bool
chown_files (char **files, int bit_flags,
             uid_t uid, gid_t gid,
             uid_t required_uid, gid_t required_gid,
             struct Chown_option const *chopt)
{
  bool ok = true;

   
  int stat_flags = ((required_uid != (uid_t) -1 || required_gid != (gid_t) -1
                     || chopt->affect_symlink_referent
                     || chopt->verbosity != V_off)
                    ? 0
                    : FTS_NOSTAT);

  FTS *fts = xfts_open (files, bit_flags | stat_flags, nullptr);

  while (true)
    {
      FTSENT *ent;

      ent = fts_read (fts);
      if (ent == nullptr)
        {
          if (errno != 0)
            {
               
              if (! chopt->force_silent)
                error (0, errno, _("fts_read failed"));
              ok = false;
            }
          break;
        }

      ok &= change_file_owner (fts, ent, uid, gid,
                               required_uid, required_gid, chopt);
    }

  if (fts_close (fts) != 0)
    {
      error (0, errno, _("fts_close failed"));
      ok = false;
    }

  return ok;
}
