 
#include "fseterr.h"

#include <errno.h>

#include "stdio-impl.h"

 

void
fseterr (FILE *fp)
{
   
#if defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1
   
  fp->_flags |= _IO_ERR_SEEN;
#elif defined __sferror || defined __DragonFly__ || defined __ANDROID__
   
  fp_->_flags |= __SERR;
#elif defined __EMX__                
  fp->_flags |= _IOERR;
#elif defined __minix                
  fp->_flags |= _IOERR;
#elif defined _IOERR                 
  fp_->_flag |= _IOERR;
#elif defined __UCLIBC__             
  fp->__modeflags |= __FLAG_ERROR;
#elif defined __QNX__                
  fp->_Mode |= 0x200  ;
#elif defined __MINT__               
  fp->__error = 1;
#elif defined EPLAN9                 
  if (fp->state != 0  )
    fp->state = 5  ;
#elif 0                              
   
  int saved_errno;
  int fd;
  int fd2;

  saved_errno = errno;
  fflush (fp);
  fd = fileno (fp);
  fd2 = dup (fd);
  if (fd2 >= 0)
    {
      close (fd);
      fputc ('\0', fp);  
      fflush (fp);       
      if (dup2 (fd2, fd) < 0)
         
        abort ();
      close (fd2);
    }
  errno = saved_errno;
#else
 #error "Please port gnulib fseterr.c to your platform! Look at the definitions of ferror and clearerr on your system, then report this to bug-gnulib."
#endif
}
