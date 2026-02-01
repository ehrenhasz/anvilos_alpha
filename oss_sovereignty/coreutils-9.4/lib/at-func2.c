 

#include <config.h>

#include "openat-priv.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "filename.h"  
#include "filenamecat.h"
#include "openat.h"
#include "same-inode.h"
#include "save-cwd.h"

 
int
at_func2 (int fd1, char const *file1,
          int fd2, char const *file2,
          int (*func) (char const *file1, char const *file2))
{
  struct saved_cwd saved_cwd;
  int saved_errno;
  int err;
  char *file1_alt;
  char *file2_alt;
  struct stat st1;
  struct stat st2;

   

  if ((fd1 == AT_FDCWD || IS_ABSOLUTE_FILE_NAME (file1))
      && (fd2 == AT_FDCWD || IS_ABSOLUTE_FILE_NAME (file2)))
    return func (file1, file2);  

   
  {
    char proc_buf1[OPENAT_BUFFER_SIZE];
    char *proc_file1 = ((fd1 == AT_FDCWD || IS_ABSOLUTE_FILE_NAME (file1))
                        ? (char *) file1
                        : openat_proc_name (proc_buf1, fd1, file1));
    if (proc_file1)
      {
        char proc_buf2[OPENAT_BUFFER_SIZE];
        char *proc_file2 = ((fd2 == AT_FDCWD || IS_ABSOLUTE_FILE_NAME (file2))
                            ? (char *) file2
                            : openat_proc_name (proc_buf2, fd2, file2));
        if (proc_file2)
          {
            int proc_result = func (proc_file1, proc_file2);
            int proc_errno = errno;
            if (proc_file1 != proc_buf1 && proc_file1 != file1)
              free (proc_file1);
            if (proc_file2 != proc_buf2 && proc_file2 != file2)
              free (proc_file2);
             
            if (0 <= proc_result)
              return proc_result;
            if (! EXPECTED_ERRNO (proc_errno))
              {
                errno = proc_errno;
                return proc_result;
              }
          }
        else if (proc_file1 != proc_buf1 && proc_file1 != file1)
          free (proc_file1);
      }
  }

   
  if (IS_ABSOLUTE_FILE_NAME (file1))
    fd1 = AT_FDCWD;  
  else if (IS_ABSOLUTE_FILE_NAME (file2))
    fd2 = AT_FDCWD;  

   

  if (fd1 == AT_FDCWD)  
    {
      if (stat (".", &st1) == -1 || fstat (fd2, &st2) == -1)
        return -1;
      if (!S_ISDIR (st2.st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
      if (SAME_INODE (st1, st2))  
        return func (file1, file2);
    }
  else if (fd2 == AT_FDCWD)  
    {
      if (stat (".", &st2) == -1 || fstat (fd1, &st1) == -1)
        return -1;
      if (!S_ISDIR (st1.st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
      if (SAME_INODE (st1, st2))  
        return func (file1, file2);
    }
  else if (fd1 != fd2)  
    {
      if (fstat (fd1, &st1) == -1 || fstat (fd2, &st2) == -1)
        return -1;
      if (!S_ISDIR (st1.st_mode) || !S_ISDIR (st2.st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
      if (SAME_INODE (st1, st2))  
        {
          fd2 = fd1;
          if (stat (".", &st1) == 0 && SAME_INODE (st1, st2))
            return func (file1, file2);  
        }
    }
  else  
    {
      if (fstat (fd1, &st1) == -1)
        return -1;
      if (!S_ISDIR (st1.st_mode))
        {
          errno = ENOTDIR;
          return -1;
        }
      if (stat (".", &st2) == 0 && SAME_INODE (st1, st2))
        return func (file1, file2);  
    }

   
  if (file1[0] == '\0' || file2[0] == '\0')
    {
      errno = ENOENT;
      return -1;
    }

   

  if (save_cwd (&saved_cwd) != 0)
    openat_save_fail (errno);

  if (fd1 != AT_FDCWD && fd2 != AT_FDCWD && fd1 != fd2)  
    {
      if (fchdir (fd1) != 0)
        {
          saved_errno = errno;
          free_cwd (&saved_cwd);
          errno = saved_errno;
          return -1;
        }
      fd1 = AT_FDCWD;  
    }

   

  file1_alt = (char *) file1;
  file2_alt = (char *) file2;

  if (fd1 == AT_FDCWD && !IS_ABSOLUTE_FILE_NAME (file1))  
    {
       
      char *cwd = getcwd (NULL, 0);
      if (!cwd)
        {
          saved_errno = errno;
          free_cwd (&saved_cwd);
          errno = saved_errno;
          return -1;
        }
      file1_alt = mfile_name_concat (cwd, file1, NULL);
      if (!file1_alt)
        {
          saved_errno = errno;
          free (cwd);
          free_cwd (&saved_cwd);
          errno = saved_errno;
          return -1;
        }
      free (cwd);  
    }
  else if (fd2 == AT_FDCWD && !IS_ABSOLUTE_FILE_NAME (file2))  
    {
      char *cwd = getcwd (NULL, 0);
      if (!cwd)
        {
          saved_errno = errno;
          free_cwd (&saved_cwd);
          errno = saved_errno;
          return -1;
        }
      file2_alt = mfile_name_concat (cwd, file2, NULL);
      if (!file2_alt)
        {
          saved_errno = errno;
          free (cwd);
          free_cwd (&saved_cwd);
          errno = saved_errno;
          return -1;
        }
      free (cwd);  
    }

   
  if (fchdir (fd1 == AT_FDCWD ? fd2 : fd1) != 0)
    {
      saved_errno = errno;
      free_cwd (&saved_cwd);
      if (file1 != file1_alt)
        free (file1_alt);
      else if (file2 != file2_alt)
        free (file2_alt);
      errno = saved_errno;
      return -1;
    }

   

  err = func (file1_alt, file2_alt);
  saved_errno = (err < 0 ? errno : 0);

  if (file1 != file1_alt)
    free (file1_alt);
  else if (file2 != file2_alt)
    free (file2_alt);

  if (restore_cwd (&saved_cwd) != 0)
    openat_restore_fail (errno);

  free_cwd (&saved_cwd);

  if (saved_errno)
    errno = saved_errno;
  return err;
}
#undef CALL_FUNC
#undef FUNC_RESULT
