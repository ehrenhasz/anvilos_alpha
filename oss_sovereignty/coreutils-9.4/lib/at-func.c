 

#include "filename.h"  

#ifdef GNULIB_SUPPORT_ONLY_AT_FDCWD
# include <errno.h>
# ifndef ENOTSUP
#  define ENOTSUP EINVAL
# endif
#else
# include "openat.h"
# include "openat-priv.h"
# include "save-cwd.h"
#endif

#ifdef AT_FUNC_USE_F1_COND
# define CALL_FUNC(F)                           \
  (flag == AT_FUNC_USE_F1_COND                  \
    ? AT_FUNC_F1 (F AT_FUNC_POST_FILE_ARGS)     \
    : AT_FUNC_F2 (F AT_FUNC_POST_FILE_ARGS))
# define VALIDATE_FLAG(F)                       \
  if (flag & ~AT_FUNC_USE_F1_COND)              \
    {                                           \
      errno = EINVAL;                           \
      return FUNC_FAIL;                         \
    }
#else
# define CALL_FUNC(F) (AT_FUNC_F1 (F AT_FUNC_POST_FILE_ARGS))
# define VALIDATE_FLAG(F)  
#endif

#ifdef AT_FUNC_RESULT
# define FUNC_RESULT AT_FUNC_RESULT
#else
# define FUNC_RESULT int
#endif

#ifdef AT_FUNC_FAIL
# define FUNC_FAIL AT_FUNC_FAIL
#else
# define FUNC_FAIL -1
#endif

 
FUNC_RESULT
AT_FUNC_NAME (int fd, char const *file AT_FUNC_POST_FILE_PARAM_DECLS)
{
  VALIDATE_FLAG (flag);

  if (fd == AT_FDCWD || IS_ABSOLUTE_FILE_NAME (file))
    return CALL_FUNC (file);

#ifdef GNULIB_SUPPORT_ONLY_AT_FDCWD
  errno = ENOTSUP;
  return FUNC_FAIL;
#else
  {
   
  struct saved_cwd saved_cwd;
  int saved_errno;
  FUNC_RESULT err;

  {
    char proc_buf[OPENAT_BUFFER_SIZE];
    char *proc_file = openat_proc_name (proc_buf, fd, file);
    if (proc_file)
      {
        FUNC_RESULT proc_result = CALL_FUNC (proc_file);
        int proc_errno = errno;
        if (proc_file != proc_buf)
          free (proc_file);
         
        if (FUNC_FAIL != proc_result)
          return proc_result;
        if (! EXPECTED_ERRNO (proc_errno))
          {
            errno = proc_errno;
            return proc_result;
          }
      }
  }

  if (save_cwd (&saved_cwd) != 0)
    openat_save_fail (errno);
  if (0 <= fd && fd == saved_cwd.desc)
    {
       
      free_cwd (&saved_cwd);
      errno = EBADF;
      return FUNC_FAIL;
    }

  if (fchdir (fd) != 0)
    {
      saved_errno = errno;
      free_cwd (&saved_cwd);
      errno = saved_errno;
      return FUNC_FAIL;
    }

  err = CALL_FUNC (file);
  saved_errno = (err == FUNC_FAIL ? errno : 0);

  if (restore_cwd (&saved_cwd) != 0)
    openat_restore_fail (errno);

  free_cwd (&saved_cwd);

  if (saved_errno)
    errno = saved_errno;
  return err;
  }
#endif
}
#undef CALL_FUNC
#undef FUNC_RESULT
#undef FUNC_FAIL
