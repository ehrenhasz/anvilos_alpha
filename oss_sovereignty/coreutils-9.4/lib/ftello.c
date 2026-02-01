 
#include <stdio.h>

#include <errno.h>
#include "intprops.h"

 
#include <unistd.h>

#include "stdio-impl.h"

off_t
ftello (FILE *fp)
#undef ftello
#if !HAVE_FTELLO
# undef ftell
# define ftello ftell
#endif
#if _GL_WINDOWS_64_BIT_OFF_T
# undef ftello
# if HAVE__FTELLI64  
#  define ftello _ftelli64
# else  
#  define ftello ftello64
# endif
#endif
{
#if FTELLO_BROKEN_AFTER_UNGETC  
   

   
  if (fp->_file < 0 || fp->_seek == NULL)
    {
      errno = ESPIPE;
      return -1;
    }

   
  off_t pos;

  if (fp->_flags & __SOFF)
    pos = fp->_offset;
  else
    {
      pos = fp->_seek (fp->_cookie, 0, SEEK_CUR);
      if (pos < 0)
        return -1;
      if (fp->_flags & __SOPT)
        {
          fp->_offset = pos;
          fp->_flags |= __SOFF;
        }
    }

  if (fp->_flags & __SRD)
    {
       
      if (fp->_ub._base != NULL)
         
        pos = pos - fp->_ur - fp->_r;
      else
         
        pos = pos - fp->_r;
      if (pos < 0)
        {
          errno = EIO;
          return -1;
        }
    }
  else if ((fp->_flags & __SWR) && fp->_p != NULL)
    {
       
      off_t buffered = fp->_p - fp->_bf._base;

       
      off_t sum;
      if (! INT_ADD_OK (pos, buffered, &sum))
        {
          errno = EOVERFLOW;
          return -1;
        }
      pos = sum;
    }

  return pos;

#else

# if LSEEK_PIPE_BROKEN
   
  if (lseek (fileno (fp), 0, SEEK_CUR) == -1)
    return -1;
# endif

# if FTELLO_BROKEN_AFTER_SWITCHING_FROM_READ_TO_WRITE  
   
  if (fp_->_flag & _IOWRT)
    {
      off_t pos;

       
      ftello (fp);

       
      pos = lseek (fileno (fp), (off_t) 0, SEEK_CUR);
      if (pos >= 0)
        {
          if ((fp_->_flag & _IONBF) == 0 && fp_->_base != NULL)
            pos += fp_->_ptr - fp_->_base;
        }
      return pos;
    }
# endif

# if defined __SL64 && defined __SCLE  
  if ((fp->_flags & __SL64) == 0)
    {
       
      FILE *tmp = fopen ("/dev/null", "r");
      if (!tmp)
        return -1;
      fp->_flags |= __SL64;
      fp->_seek64 = tmp->_seek64;
      fclose (tmp);
    }
# endif

  return ftello (fp);

#endif
}
