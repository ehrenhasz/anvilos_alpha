 
#include <unistd.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#if !HAVE_READLINK

 

ssize_t
readlink (char const *file, _GL_UNUSED char *buf,
          _GL_UNUSED size_t bufsize)
{
  struct stat statbuf;

   
  if (stat (file, &statbuf) >= 0)
    errno = EINVAL;
  return -1;
}

#else  

# undef readlink

 

ssize_t
rpl_readlink (char const *file, char *buf, size_t bufsize)
{
# if READLINK_TRAILING_SLASH_BUG
  size_t file_len = strlen (file);
  if (file_len && file[file_len - 1] == '/')
    {
       
      struct stat st;
      if (stat (file, &st) == 0 || errno == EOVERFLOW)
        errno = EINVAL;
      return -1;
    }
# endif  

  ssize_t r = readlink (file, buf, bufsize);

# if READLINK_TRUNCATE_BUG
  if (r < 0 && errno == ERANGE)
    {
       
      char stackbuf[4032];
      r = readlink (file, stackbuf, sizeof stackbuf);
      if (r < 0)
        {
          if (errno == ERANGE)
            {
               
              r = bufsize;
              memset (buf, 0, r);
            }
        }
      else
        {
          if (bufsize < r)
            r = bufsize;
          memcpy (buf, stackbuf, r);
        }
    }
# endif

  return r;
}

#endif  
