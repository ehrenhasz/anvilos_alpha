 

#include <config.h>

 
#include "fpending.h"

#include "stdio-impl.h"

 

 
size_t
__fpending (FILE *fp)
{
   
#if defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1
   
  return fp->_IO_write_ptr - fp->_IO_write_base;
#elif defined __sferror || defined __DragonFly__ || defined __ANDROID__
   
  return fp_->_p - fp_->_bf._base;
#elif defined __EMX__                 
  return fp->_ptr - fp->_buffer;
#elif defined __minix                 
  return fp_->_ptr - fp_->_buf;
#elif defined _IOERR                  
  return (fp_->_ptr ? fp_->_ptr - fp_->_base : 0);
#elif defined __UCLIBC__              
  return (fp->__modeflags & __FLAG_WRITING ? fp->__bufpos - fp->__bufstart : 0);
#elif defined __QNX__                 
  return (fp->_Mode & 0x2000   ? fp->_Next - fp->_Buf : 0);
#elif defined __MINT__                
  return fp->__bufp - fp->__buffer;
#elif defined EPLAN9                  
  return fp->wp - fp->buf;
#else
# error "Please port gnulib fpending.c to your platform!"
  return 1;
#endif
}
