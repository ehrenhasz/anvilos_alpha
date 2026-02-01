 

 

#include <config.h>

 
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#if !HAVE_CHOWN

 
int
chown (_GL_UNUSED const char *file, _GL_UNUSED uid_t uid,
       _GL_UNUSED gid_t gid)
{
  errno = ENOSYS;
  return -1;
}

#else  

 
# undef chown

 

int
rpl_chown (const char *file, uid_t uid, gid_t gid)
{
  struct stat st;
  bool stat_valid = false;
  int result;

# if CHOWN_CHANGE_TIME_BUG
  if (gid != (gid_t) -1 || uid != (uid_t) -1)
    {
      if (stat (file, &st))
        return -1;
      stat_valid = true;
    }
# endif

# if CHOWN_FAILS_TO_HONOR_ID_OF_NEGATIVE_ONE
  if (gid == (gid_t) -1 || uid == (uid_t) -1)
    {
       
      if (!stat_valid && stat (file, &st))
        return -1;
      if (gid == (gid_t) -1)
        gid = st.st_gid;
      if (uid == (uid_t) -1)
        uid = st.st_uid;
    }
# endif

# if CHOWN_MODIFIES_SYMLINK
  {
     
    int open_flags = O_NONBLOCK | O_NOCTTY | O_CLOEXEC;
    int fd = open (file, O_RDONLY | open_flags);
    if (0 <= fd
        || (errno == EACCES
            && 0 <= (fd = open (file, O_WRONLY | open_flags))))
      {
        int saved_errno;
        bool fchown_socket_failure;

        result = fchown (fd, uid, gid);
        saved_errno = errno;

         
        fchown_socket_failure =
          (result != 0 && saved_errno == EINVAL
           && fstat (fd, &st) == 0
           && (S_ISFIFO (st.st_mode) || S_ISSOCK (st.st_mode)));

        close (fd);

        if (! fchown_socket_failure)
          {
            errno = saved_errno;
            return result;
          }
      }
    else if (errno != EACCES)
      return -1;
  }
# endif

# if CHOWN_TRAILING_SLASH_BUG
  if (!stat_valid)
    {
      size_t len = strlen (file);
      if (len && file[len - 1] == '/' && stat (file, &st))
        return -1;
    }
# endif

  result = chown (file, uid, gid);

# if CHOWN_CHANGE_TIME_BUG
  if (result == 0 && stat_valid
      && (uid == st.st_uid || uid == (uid_t) -1)
      && (gid == st.st_gid || gid == (gid_t) -1))
    {
       
      result = chmod (file, st.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO
                                          | S_ISUID | S_ISGID | S_ISVTX));
    }
# endif

  return result;
}

#endif  
