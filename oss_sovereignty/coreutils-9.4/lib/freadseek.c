 
#include "freadseek.h"

#include <stdlib.h>
#include <unistd.h>

#include "freadahead.h"
#include "freadptr.h"

#include "stdio-impl.h"

 
static void
freadptrinc (FILE *fp, size_t increment)
{
   
#if HAVE___FREADPTRINC               
  __freadptrinc (fp, increment);
#elif defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1
   
  fp->_IO_read_ptr += increment;
#elif defined __sferror || defined __DragonFly__ || defined __ANDROID__
   
  fp_->_p += increment;
  fp_->_r -= increment;
#elif defined __EMX__                
  fp->_ptr += increment;
  fp->_rcount -= increment;
#elif defined __minix                
  fp_->_ptr += increment;
  fp_->_count -= increment;
#elif defined _IOERR                 
  fp_->_ptr += increment;
  fp_->_cnt -= increment;
#elif defined __UCLIBC__             
# ifdef __STDIO_BUFFERS
  fp->__bufpos += increment;
# else
  abort ();
# endif
#elif defined __QNX__                
  fp->_Next += increment;
#elif defined __MINT__               
  fp->__bufp += increment;
#elif defined EPLAN9                 
  fp->rp += increment;
#elif defined SLOW_BUT_NO_HACKS      
#else
 #error "Please port gnulib freadseek.c to your platform! Look at the definition of getc, getc_unlocked on your system, then report this to bug-gnulib."
#endif
}

int
freadseek (FILE *fp, size_t offset)
{
  size_t total_buffered;
  int fd;

  if (offset == 0)
    return 0;

   
  total_buffered = freadahead (fp);
   
  while (total_buffered > 0)
    {
      size_t buffered;

      if (freadptr (fp, &buffered) != NULL && buffered > 0)
        {
          size_t increment = (buffered < offset ? buffered : offset);

          freadptrinc (fp, increment);
          offset -= increment;
          if (offset == 0)
            return 0;
          total_buffered -= increment;
          if (total_buffered == 0)
            break;
        }
       
      if (fgetc (fp) == EOF)
        goto eof;
      offset--;
      if (offset == 0)
        return 0;
      total_buffered--;
    }

   
  fd = fileno (fp);
  if (fd >= 0 && lseek (fd, 0, SEEK_CUR) >= 0)
    {
       
      return fseeko (fp, offset, SEEK_CUR);
    }
  else
    {
       
      char buf[4096];

      do
        {
          size_t count = (sizeof (buf) < offset ? sizeof (buf) : offset);
          if (fread (buf, 1, count, fp) < count)
            goto eof;
          offset -= count;
        }
      while (offset > 0);

      return 0;
   }

 eof:
   
  if (ferror (fp))
    return EOF;
  else
     
    return 0;
}
