 

#include <config.h>
#include <stdio.h>
#include <sys/types.h>

#include "system.h"
#include "assure.h"
#include "file-type.h"
#include "filenamecat.h"
#include "ignore-value.h"
#include "remove.h"
#include "root-dev-ino.h"
#include "stat-time.h"
#include "write-any-file.h"
#include "xfts.h"
#include "yesno.h"

 
enum Prompt_action
  {
    PA_DESCEND_INTO_DIR = 2,
    PA_REMOVE_DIR
  };

 
#if ! HAVE_STRUCT_DIRENT_D_TYPE
 
# undef DT_UNKNOWN
# undef DT_DIR
# undef DT_LNK
# define DT_UNKNOWN 0
# define DT_DIR 1
# define DT_LNK 2
#endif

 
static int
cache_fstatat (int fd, char const *file, struct stat *st, int flag)
{
#if HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC
   
  if (0 <= st->st_atim.tv_nsec)
    return 0;
  if (st->st_atim.tv_nsec == -1)
    {
      if (fstatat (fd, file, st, flag) == 0)
        return 0;
      st->st_atim.tv_nsec = -2;
      st->st_ino = errno;
    }
  errno = st->st_ino;
  return -1;
#else
  return fstatat (fd, file, st, flag);
#endif
}

 
static inline struct stat *
cache_stat_init (struct stat *st)
{
#if HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC
  st->st_atim.tv_nsec = -1;
#endif
  return st;
}

 
static int
write_protected_non_symlink (int fd_cwd,
                             char const *file,
                             struct stat *buf)
{
  if (can_write_any_file ())
    return 0;
  if (cache_fstatat (fd_cwd, file, buf, AT_SYMLINK_NOFOLLOW) != 0)
    return -1;
  if (S_ISLNK (buf->st_mode))
    return 0;
   

   

   

  {
    if (faccessat (fd_cwd, file, W_OK, AT_EACCESS) == 0)
      return 0;

    return errno == EACCES ? 1 : -1;
  }
}

 
static int
get_dir_status (FTS const *fts, FTSENT const *ent, int *dir_status)
{
  if (*dir_status == DS_UNKNOWN)
    *dir_status = directory_status (fts->fts_cwd_fd, ent->fts_accpath);
  return *dir_status;
}

 
static enum RM_status
prompt (FTS const *fts, FTSENT const *ent, bool is_dir,
        struct rm_options const *x, enum Prompt_action mode,
        int *dir_status)
{
  int fd_cwd = fts->fts_cwd_fd;
  char const *full_name = ent->fts_path;
  char const *filename = ent->fts_accpath;
  struct stat st;
  struct stat *sbuf = &st;
  cache_stat_init (sbuf);

  int dirent_type = is_dir ? DT_DIR : DT_UNKNOWN;
  int write_protected = 0;

   
  if (ent->fts_number)
    return RM_USER_DECLINED;

  if (x->interactive == RMI_NEVER)
    return RM_OK;

  int wp_errno = 0;
  if (!x->ignore_missing_files
      && (x->interactive == RMI_ALWAYS || x->stdin_tty)
      && dirent_type != DT_LNK)
    {
      write_protected = write_protected_non_symlink (fd_cwd, filename, sbuf);
      wp_errno = errno;
    }

  if (write_protected || x->interactive == RMI_ALWAYS)
    {
      if (0 <= write_protected && dirent_type == DT_UNKNOWN)
        {
          if (cache_fstatat (fd_cwd, filename, sbuf, AT_SYMLINK_NOFOLLOW) == 0)
            {
              if (S_ISLNK (sbuf->st_mode))
                dirent_type = DT_LNK;
              else if (S_ISDIR (sbuf->st_mode))
                dirent_type = DT_DIR;
               
            }
          else
            {
               
              write_protected = -1;
              wp_errno = errno;
            }
        }

      if (0 <= write_protected)
        switch (dirent_type)
          {
          case DT_LNK:
             
            if (x->interactive != RMI_ALWAYS)
              return RM_OK;
            break;

          case DT_DIR:
              
            if ( ! (x->recursive
                    || (x->remove_empty_directories
                        && get_dir_status (fts, ent, dir_status) != 0)))
              {
                write_protected = -1;
                wp_errno = *dir_status <= 0 ? EISDIR : *dir_status;
              }
            break;
          }

      char const *quoted_name = quoteaf (full_name);

      if (write_protected < 0)
        {
          error (0, wp_errno, _("cannot remove %s"), quoted_name);
          return RM_ERROR;
        }

       
      if (dirent_type == DT_DIR
          && mode == PA_DESCEND_INTO_DIR
          && get_dir_status (fts, ent, dir_status) == DS_NONEMPTY)
        fprintf (stderr,
                 (write_protected
                  ? _("%s: descend into write-protected directory %s? ")
                  : _("%s: descend into directory %s? ")),
                 program_name, quoted_name);
      else if (0 < *dir_status)
        {
          if ( ! (x->remove_empty_directories && *dir_status == EACCES))
            {
              error (0, *dir_status, _("cannot remove %s"), quoted_name);
              return RM_ERROR;
            }

           
          if (mode == PA_DESCEND_INTO_DIR)
            return RM_OK;

          fprintf (stderr,
               _("%s: attempt removal of inaccessible directory %s? "),
                   program_name, quoted_name);
        }
      else
        {
          if (cache_fstatat (fd_cwd, filename, sbuf, AT_SYMLINK_NOFOLLOW) != 0)
            {
              error (0, errno, _("cannot remove %s"), quoted_name);
              return RM_ERROR;
            }

          fprintf (stderr,
                   (write_protected
                     
                    ? _("%s: remove write-protected %s %s? ")
                    : _("%s: remove %s %s? ")),
                   program_name, file_type (sbuf), quoted_name);
        }

      return yesno () ? RM_USER_ACCEPTED : RM_USER_DECLINED;
    }
  return RM_OK;
}

 
static inline bool
nonexistent_file_errno (int errnum)
{
   

  switch (errnum)
    {
    case EILSEQ:
    case EINVAL:
    case ENOENT:
    case ENOTDIR:
      return true;
    default:
      return false;
    }
}

 
static inline bool
ignorable_missing (struct rm_options const *x, int errnum)
{
  return x->ignore_missing_files && nonexistent_file_errno (errnum);
}

 
static void
fts_skip_tree (FTS *fts, FTSENT *ent)
{
  fts_set (fts, ent, FTS_SKIP);
   
  ignore_value (fts_read (fts));
}

 
static void
mark_ancestor_dirs (FTSENT *ent)
{
  FTSENT *p;
  for (p = ent->fts_parent; FTS_ROOTLEVEL <= p->fts_level; p = p->fts_parent)
    {
      if (p->fts_number)
        break;
      p->fts_number = 1;
    }
}

 
static enum RM_status
excise (FTS *fts, FTSENT *ent, struct rm_options const *x, bool is_dir)
{
  int flag = is_dir ? AT_REMOVEDIR : 0;
  if (unlinkat (fts->fts_cwd_fd, ent->fts_accpath, flag) == 0)
    {
      if (x->verbose)
        {
          printf ((is_dir
                   ? _("removed directory %s\n")
                   : _("removed %s\n")), quoteaf (ent->fts_path));
        }
      return RM_OK;
    }

   
  if (errno == EROFS)
    {
      struct stat st;
      if ( ! (fstatat (fts->fts_cwd_fd, ent->fts_accpath, &st,
                       AT_SYMLINK_NOFOLLOW)
              && errno == ENOENT))
        errno = EROFS;
    }

  if (ignorable_missing (x, errno))
    return RM_OK;

   
  if (ent->fts_info == FTS_DNR
      && (errno == ENOTEMPTY || errno == EISDIR || errno == ENOTDIR
          || errno == EEXIST)
      && ent->fts_errno != 0)
    errno = ent->fts_errno;
  error (0, errno, _("cannot remove %s"), quoteaf (ent->fts_path));
  mark_ancestor_dirs (ent);
  return RM_ERROR;
}

 
static enum RM_status
rm_fts (FTS *fts, FTSENT *ent, struct rm_options const *x)
{
  int dir_status = DS_UNKNOWN;

  switch (ent->fts_info)
    {
    case FTS_D:			 
      if (! x->recursive
          && !(x->remove_empty_directories
               && get_dir_status (fts, ent, &dir_status) != 0))
        {
           
          int err = x->remove_empty_directories ? ENOTEMPTY : EISDIR;
          error (0, err, _("cannot remove %s"), quoteaf (ent->fts_path));
          mark_ancestor_dirs (ent);
          fts_skip_tree (fts, ent);
          return RM_ERROR;
        }

       
      if (ent->fts_level == FTS_ROOTLEVEL)
        {
           
          if (dot_or_dotdot (last_component (ent->fts_accpath)))
            {
              error (0, 0,
                     _("refusing to remove %s or %s directory: skipping %s"),
                     quoteaf_n (0, "."), quoteaf_n (1, ".."),
                     quoteaf_n (2, ent->fts_path));
              fts_skip_tree (fts, ent);
              return RM_ERROR;
            }

           
          if (ROOT_DEV_INO_CHECK (x->root_dev_ino, ent->fts_statp))
            {
              ROOT_DEV_INO_WARN (ent->fts_path);
              fts_skip_tree (fts, ent);
              return RM_ERROR;
            }

           
          if (x->preserve_all_root)
            {
              bool failed = false;
              char *parent = file_name_concat (ent->fts_accpath, "..", nullptr);
              struct stat statbuf;

              if (!parent || lstat (parent, &statbuf))
                {
                  error (0, 0,
                         _("failed to stat %s: skipping %s"),
                         quoteaf_n (0, parent),
                         quoteaf_n (1, ent->fts_accpath));
                  failed = true;
                }

              free (parent);

              if (failed || fts->fts_dev != statbuf.st_dev)
                {
                  if (! failed)
                    {
                      error (0, 0,
                             _("skipping %s, since it's on a different device"),
                             quoteaf (ent->fts_path));
                      error (0, 0, _("and --preserve-root=all is in effect"));
                    }
                  fts_skip_tree (fts, ent);
                  return RM_ERROR;
                }
            }
        }

      {
        enum RM_status s = prompt (fts, ent, true  , x,
                                   PA_DESCEND_INTO_DIR, &dir_status);

        if (s == RM_USER_ACCEPTED && dir_status == DS_EMPTY)
          {
             
            s = excise (fts, ent, x, true);
            if (s == RM_OK)
              fts_skip_tree (fts, ent);
          }

        if (! (s == RM_OK || s == RM_USER_ACCEPTED))
          {
            mark_ancestor_dirs (ent);
            fts_skip_tree (fts, ent);
          }

        return s;
      }

    case FTS_F:			 
    case FTS_NS:		 
    case FTS_SL:		 
    case FTS_SLNONE:		 
    case FTS_DP:		 
    case FTS_DNR:		 
    case FTS_NSOK:		 
    case FTS_DEFAULT:		 
      {
         
        if (ent->fts_info == FTS_DP
            && x->one_file_system
            && FTS_ROOTLEVEL < ent->fts_level
            && ent->fts_statp->st_dev != fts->fts_dev)
          {
            mark_ancestor_dirs (ent);
            error (0, 0, _("skipping %s, since it's on a different device"),
                   quoteaf (ent->fts_path));
            return RM_ERROR;
          }

        bool is_dir = ent->fts_info == FTS_DP || ent->fts_info == FTS_DNR;
        enum RM_status s = prompt (fts, ent, is_dir, x, PA_REMOVE_DIR,
                                   &dir_status);
        if (! (s == RM_OK || s == RM_USER_ACCEPTED))
          return s;
        return excise (fts, ent, x, is_dir);
      }

    case FTS_DC:		 
      emit_cycle_warning (ent->fts_path);
      fts_skip_tree (fts, ent);
      return RM_ERROR;

    case FTS_ERR:
       
      error (0, ent->fts_errno, _("traversal failed: %s"),
             quotef (ent->fts_path));
      fts_skip_tree (fts, ent);
      return RM_ERROR;

    default:
      error (0, 0, _("unexpected failure: fts_info=%d: %s\n"
                     "please report to %s"),
             ent->fts_info,
             quotef (ent->fts_path),
             PACKAGE_BUGREPORT);
      abort ();
    }
}

 
enum RM_status
rm (char *const *file, struct rm_options const *x)
{
  enum RM_status rm_status = RM_OK;

  if (*file)
    {
      int bit_flags = (FTS_CWDFD
                       | FTS_NOSTAT
                       | FTS_PHYSICAL);

      if (x->one_file_system)
        bit_flags |= FTS_XDEV;

      FTS *fts = xfts_open (file, bit_flags, nullptr);

      while (true)
        {
          FTSENT *ent;

          ent = fts_read (fts);
          if (ent == nullptr)
            {
              if (errno != 0)
                {
                  error (0, errno, _("fts_read failed"));
                  rm_status = RM_ERROR;
                }
              break;
            }

          enum RM_status s = rm_fts (fts, ent, x);

          affirm (VALID_STATUS (s));
          UPDATE_STATUS (rm_status, s);
        }

      if (fts_close (fts) != 0)
        {
          error (0, errno, _("fts_close failed"));
          rm_status = RM_ERROR;
        }
    }

  return rm_status;
}
