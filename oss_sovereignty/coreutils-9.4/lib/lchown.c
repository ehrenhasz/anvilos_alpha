 

#include <config.h>

#include <unistd.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#if !HAVE_LCHOWN

 
# if CHOWN_MODIFIES_SYMLINK
#  undef chown
# endif

 

int
lchown (const char *file, uid_t uid, gid_t gid)
{
# if HAVE_CHOWN
#  if ! CHOWN_MODIFIES_SYMLINK
  char readlink_buf[1];

  if (0 <= readlink (file, readlink_buf, sizeof readlink_buf))
    {
      errno = EOPNOTSUPP;
      return -1;
    }
#  endif

  return chown (file, uid, gid);

# else  
  errno = ENOSYS;
  return -1;
# endif
}

#else  

# undef lchown

 
int
rpl_lchown (const char *file, uid_t uid, gid_t gid)
{
  bool stat_valid = false;
  int result;

# if CHOWN_CHANGE_TIME_BUG
  struct stat st;

  if (gid != (gid_t) -1 || uid != (uid_t) -1)
    {
      if (lstat (file, &st))
        return -1;
      stat_valid = true;
      if (!S_ISLNK (st.st_mode))
        return chown (file, uid, gid);
    }
# endif

# if CHOWN_TRAILING_SLASH_BUG
  if (!stat_valid)
    {
      size_t len = strlen (file);
      if (len && file[len - 1] == '/')
        return chown (file, uid, gid);
    }
# endif

  result = lchown (file, uid, gid);

# if CHOWN_CHANGE_TIME_BUG && HAVE_LCHMOD
  if (result == 0 && stat_valid
      && (uid == st.st_uid || uid == (uid_t) -1)
      && (gid == st.st_gid || gid == (gid_t) -1))
    {
       
      result = lchmod (file, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO
                                           | S_ISUID | S_ISGID | S_ISVTX));
    }
# endif

  return result;
}

#endif  
