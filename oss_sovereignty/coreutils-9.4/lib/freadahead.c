 
#include "freadahead.h"

#include <stdlib.h>
#include "stdio-impl.h"

#if defined __DragonFly__
 
extern size_t __sreadahead (FILE *);
#endif

 

size_t
freadahead (FILE *fp)
{
#if defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1
   
  if (fp->_IO_write_ptr > fp->_IO_write_base)
    return 0;
  return (fp->_IO_read_end - fp->_IO_read_ptr)
         + (fp->_flags & _IO_IN_BACKUP ? fp->_IO_save_end - fp->_IO_save_base :
            0);
#elif defined __sferror || defined __DragonFly__ || defined __ANDROID__
   
  if ((fp_->_flags & __SWR) != 0 || fp_->_r < 0)
    return 0;
# if defined __DragonFly__
  return __sreadahead (fp);
# else
  return fp_->_r
         + (HASUB (fp) ? fp_->_ur : 0);
# endif
#elif defined __EMX__                
  if ((fp->_flags & _IOWRT) != 0)
    return 0;
   
   
  return (fp->_rcount > 0 ? fp->_rcount : fp->_ungetc_count - fp->_rcount);
#elif defined __minix                
  if ((fp_->_flags & _IOWRITING) != 0)
    return 0;
  return fp_->_count;
#elif defined _IOERR                 
  if ((fp_->_flag & _IOWRT) != 0)
    return 0;
  return fp_->_cnt;
#elif defined __UCLIBC__             
# ifdef __STDIO_BUFFERS
  if (fp->__modeflags & __FLAG_WRITING)
    return 0;
  return (fp->__bufread - fp->__bufpos)
         + (fp->__modeflags & __FLAG_UNGOT ? 1 : 0);
# else
  return 0;
# endif
#elif defined __QNX__                
  if ((fp->_Mode & 0x2000  ) != 0)
    return 0;
   
  return ((fp->_Rsave ? fp->_Rsave : fp->_Rend) - fp->_Next)
         + (fp->_Mode & 0x4000  
            ? (fp->_Back + sizeof (fp->_Back)) - fp->_Rback
            : 0);
#elif defined __MINT__               
  if (!fp->__mode.__read)
    return 0;
  return (fp->__pushed_back
          ? fp->__get_limit - fp->__pushback_bufp + 1
          : fp->__get_limit - fp->__bufp);
#elif defined EPLAN9                 
  if (fp->state == 4   || fp->rp >= fp->wp)
    return 0;
  return fp->wp - fp->rp;
#elif defined SLOW_BUT_NO_HACKS      
  abort ();
  return 0;
#else
 #error "Please port gnulib freadahead.c to your platform! Look at the definition of fflush, fread, ungetc on your system, then report this to bug-gnulib."
#endif
}
