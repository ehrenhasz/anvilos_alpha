 
#include "freadptr.h"

#include <stdlib.h>

#include "stdio-impl.h"

 

const char *
freadptr (FILE *fp, size_t *sizep)
{
  size_t size;

   
#if defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1
   
  if (fp->_IO_write_ptr > fp->_IO_write_base)
    return NULL;
  size = fp->_IO_read_end - fp->_IO_read_ptr;
  if (size == 0)
    return NULL;
  *sizep = size;
  return (const char *) fp->_IO_read_ptr;
#elif defined __sferror || defined __DragonFly__ || defined __ANDROID__
   
  if ((fp_->_flags & __SWR) != 0 || fp_->_r < 0)
    return NULL;
  size = fp_->_r;
  if (size == 0)
    return NULL;
  *sizep = size;
  return (const char *) fp_->_p;
#elif defined __EMX__                
  if ((fp->_flags & _IOWRT) != 0)
    return NULL;
   
  if (fp->_rcount <= 0)
    return NULL;
  if (!(fp->_ungetc_count == 0))
    abort ();
  *sizep = fp->_rcount;
  return fp->_ptr;
#elif defined __minix                
  if ((fp_->_flags & _IOWRITING) != 0)
    return NULL;
  size = fp_->_count;
  if (size == 0)
    return NULL;
  *sizep = size;
  return (const char *) fp_->_ptr;
#elif defined _IOERR                 
  if ((fp_->_flag & _IOWRT) != 0)
    return NULL;
  size = fp_->_cnt;
  if (size == 0)
    return NULL;
  *sizep = size;
  return (const char *) fp_->_ptr;
#elif defined __UCLIBC__             
# ifdef __STDIO_BUFFERS
  if (fp->__modeflags & __FLAG_WRITING)
    return NULL;
  if (fp->__modeflags & __FLAG_UNGOT)
    return NULL;
  size = fp->__bufread - fp->__bufpos;
  if (size == 0)
    return NULL;
  *sizep = size;
  return (const char *) fp->__bufpos;
# else
  return NULL;
# endif
#elif defined __QNX__                
  if ((fp->_Mode & 0x2000  ) != 0)
    return NULL;
   
  size = fp->_Rend - fp->_Next;
  if (size == 0)
    return NULL;
  *sizep = size;
  return (const char *) fp->_Next;
#elif defined __MINT__               
  if (!fp->__mode.__read)
    return NULL;
  size = fp->__get_limit - fp->__bufp;
  if (size == 0)
    return NULL;
  *sizep = size;
  return fp->__bufp;
#elif defined EPLAN9                 
  if (fp->state == 4  )
    return NULL;
  if (fp->rp >= fp->wp)
    return NULL;
  *sizep = fp->wp - fp->rp;
  return fp->rp;
#elif defined SLOW_BUT_NO_HACKS      
   
  return NULL;
#else
 #error "Please port gnulib freadptr.c to your platform! Look at the definition of fflush, fread, getc, getc_unlocked on your system, then report this to bug-gnulib."
#endif
}
