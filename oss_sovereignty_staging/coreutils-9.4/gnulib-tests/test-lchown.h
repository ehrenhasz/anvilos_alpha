 

#include "nap.h"

#if !HAVE_GETGID
# define getgid() ((gid_t) -1)
#endif

#if !HAVE_GETEGID
# define getegid() ((gid_t) -1)
#endif

#ifndef HAVE_LCHMOD
# define HAVE_LCHMOD 0
#endif

#ifndef CHOWN_CHANGE_TIME_BUG
# define CHOWN_CHANGE_TIME_BUG 0
#endif

 

static int
test_lchown (int (*func) (char const *, uid_t, gid_t), bool print)
{
  struct stat st1;
  struct stat st2;
  gid_t *gids = NULL;
  int gids_count;
  int result;

   
  ASSERT (mkdir (BASE "dir", 0700) == 0);
  ASSERT (stat (BASE "dir", &st1) == 0);

   
  result = func (BASE "dir", st1.st_uid, getegid ());
  if (result == -1 && (errno == ENOSYS || errno == EPERM))
    {
      ASSERT (rmdir (BASE "dir") == 0);
      if (print)
        fputs ("skipping test: no support for ownership\n", stderr);
      return 77;
    }
  ASSERT (result == 0);

  ASSERT (close (creat (BASE "dir/file", 0600)) == 0);
  ASSERT (stat (BASE "dir/file", &st1) == 0);
  ASSERT (st1.st_uid != (uid_t) -1);
  ASSERT (st1.st_gid != (gid_t) -1);
   
  if (getgid () != (gid_t) -1)
    ASSERT (st1.st_gid == getegid ());

   
  errno = 0;
  ASSERT (func ("", -1, -1) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func ("no_such", -1, -1) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func ("no_such/", -1, -1) == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (func (BASE "dir/file/", -1, -1) == -1);
  ASSERT (errno == ENOTDIR);

   
  ASSERT (func (BASE "dir/file", -1, st1.st_gid) == 0);
  ASSERT (func (BASE "dir/file", st1.st_uid, -1) == 0);
  ASSERT (func (BASE "dir/file", (uid_t) -1, (gid_t) -1) == 0);
  ASSERT (stat (BASE "dir/file", &st2) == 0);
  ASSERT (st1.st_uid == st2.st_uid);
  ASSERT (st1.st_gid == st2.st_gid);

   
  nap ();
  ASSERT (func (BASE "dir/file", st1.st_uid, st1.st_gid) == 0);
  ASSERT (stat (BASE "dir/file", &st2) == 0);
  ASSERT (st1.st_ctime < st2.st_ctime
          || (st1.st_ctime == st2.st_ctime
              && get_stat_ctime_ns (&st1) < get_stat_ctime_ns (&st2)));

   
  if (symlink ("link", BASE "dir/link2"))
    {
      ASSERT (unlink (BASE "dir/file") == 0);
      ASSERT (rmdir (BASE "dir") == 0);
      if (print)
        fputs ("skipping test: symlinks not supported on this file system\n",
               stderr);
      return 77;
    }
  result = func (BASE "dir/link2", -1, -1);
  if (result == -1 && (errno == ENOSYS || errno == EOPNOTSUPP))
    {
      ASSERT (unlink (BASE "dir/file") == 0);
      ASSERT (unlink (BASE "dir/link2") == 0);
      ASSERT (rmdir (BASE "dir") == 0);
      if (print)
        fputs ("skipping test: symlink ownership not supported\n", stderr);
      return 77;
    }
  ASSERT (result == 0);
  errno = 0;
  ASSERT (func (BASE "dir/link2/", st1.st_uid, st1.st_gid) == -1);
  ASSERT (errno == ENOENT);
  ASSERT (symlink ("file", BASE "dir/link") == 0);
  ASSERT (mkdir (BASE "dir/sub", 0700) == 0);
  ASSERT (symlink ("sub", BASE "dir/link3") == 0);

   
  gids_count = mgetgroups (NULL, st1.st_gid, &gids);
  if (1 < gids_count)
    {
      ASSERT (gids[1] != st1.st_gid);
      if (getgid () != (gid_t) -1)
        ASSERT (gids[1] != (gid_t) -1);
      ASSERT (lstat (BASE "dir/link", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      ASSERT (st1.st_gid == st2.st_gid);
      ASSERT (lstat (BASE "dir/link2", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      ASSERT (st1.st_gid == st2.st_gid);

      errno = 0;
      ASSERT (func (BASE "dir/link2/", -1, gids[1]) == -1);
      ASSERT (errno == ENOTDIR);
      ASSERT (stat (BASE "dir/file", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      ASSERT (st1.st_gid == st2.st_gid);
      ASSERT (lstat (BASE "dir/link", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      ASSERT (st1.st_gid == st2.st_gid);
      ASSERT (lstat (BASE "dir/link2", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      ASSERT (st1.st_gid == st2.st_gid);

      ASSERT (func (BASE "dir/link2", -1, gids[1]) == 0);
      ASSERT (stat (BASE "dir/file", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      ASSERT (st1.st_gid == st2.st_gid);
      ASSERT (lstat (BASE "dir/link", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      ASSERT (st1.st_gid == st2.st_gid);
      ASSERT (lstat (BASE "dir/link2", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      if (getgid () != (gid_t) -1)
        ASSERT (gids[1] == st2.st_gid);

       
      ASSERT (lstat (BASE "dir/link3", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      ASSERT (st1.st_gid == st2.st_gid);
      ASSERT (lstat (BASE "dir/sub", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      ASSERT (st1.st_gid == st2.st_gid);

      ASSERT (func (BASE "dir/link3/", -1, gids[1]) == 0);
      ASSERT (lstat (BASE "dir/link3", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      ASSERT (st1.st_gid == st2.st_gid);
      ASSERT (lstat (BASE "dir/sub", &st2) == 0);
      ASSERT (st1.st_uid == st2.st_uid);
      if (getgid () != (gid_t) -1)
        ASSERT (gids[1] == st2.st_gid);
    }
  else if (!CHOWN_CHANGE_TIME_BUG || HAVE_LCHMOD)
    {
       
      struct stat l1;
      struct stat l2;
      ASSERT (stat (BASE "dir/file", &st1) == 0);
      ASSERT (lstat (BASE "dir/link", &l1) == 0);
      ASSERT (lstat (BASE "dir/link2", &l2) == 0);

      nap ();
      errno = 0;
      ASSERT (func (BASE "dir/link2/", -1, st1.st_gid) == -1);
      ASSERT (errno == ENOTDIR);
      ASSERT (stat (BASE "dir/file", &st2) == 0);
      ASSERT (st1.st_ctime == st2.st_ctime);
      ASSERT (get_stat_ctime_ns (&st1) == get_stat_ctime_ns (&st2));
      ASSERT (lstat (BASE "dir/link", &st2) == 0);
      ASSERT (l1.st_ctime == st2.st_ctime);
      ASSERT (get_stat_ctime_ns (&l1) == get_stat_ctime_ns (&st2));
      ASSERT (lstat (BASE "dir/link2", &st2) == 0);
      ASSERT (l2.st_ctime == st2.st_ctime);
      ASSERT (get_stat_ctime_ns (&l2) == get_stat_ctime_ns (&st2));

      ASSERT (func (BASE "dir/link2", -1, st1.st_gid) == 0);
      ASSERT (stat (BASE "dir/file", &st2) == 0);
      ASSERT (st1.st_ctime == st2.st_ctime);
      ASSERT (get_stat_ctime_ns (&st1) == get_stat_ctime_ns (&st2));
      ASSERT (lstat (BASE "dir/link", &st2) == 0);
      ASSERT (l1.st_ctime == st2.st_ctime);
      ASSERT (get_stat_ctime_ns (&l1) == get_stat_ctime_ns (&st2));
      ASSERT (lstat (BASE "dir/link2", &st2) == 0);
      ASSERT (l2.st_ctime < st2.st_ctime
              || (l2.st_ctime == st2.st_ctime
                  && get_stat_ctime_ns (&l2) < get_stat_ctime_ns (&st2)));

       
      ASSERT (lstat (BASE "dir/sub", &st1) == 0);
      ASSERT (lstat (BASE "dir/link3", &l1) == 0);
      nap ();
      ASSERT (func (BASE "dir/link3/", -1, st1.st_gid) == 0);
      ASSERT (lstat (BASE "dir/link3", &st2) == 0);
      ASSERT (l1.st_ctime == st2.st_ctime);
      ASSERT (get_stat_ctime_ns (&l1) == get_stat_ctime_ns (&st2));
      ASSERT (lstat (BASE "dir/sub", &st2) == 0);
      ASSERT (st1.st_ctime < st2.st_ctime
              || (st1.st_ctime == st2.st_ctime
                  && get_stat_ctime_ns (&st1) < get_stat_ctime_ns (&st2)));
    }

   
  free (gids);
  ASSERT (unlink (BASE "dir/file") == 0);
  ASSERT (unlink (BASE "dir/link") == 0);
  ASSERT (unlink (BASE "dir/link2") == 0);
  ASSERT (unlink (BASE "dir/link3") == 0);
  ASSERT (rmdir (BASE "dir/sub") == 0);
  ASSERT (rmdir (BASE "dir") == 0);
  return 0;
}
