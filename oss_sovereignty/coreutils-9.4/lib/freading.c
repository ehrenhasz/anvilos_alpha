 
#include "freading.h"

#include "stdio-impl.h"

 
# if defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1
   
  return ((fp->_flags & _IO_NO_WRITES) != 0
          || ((fp->_flags & (_IO_NO_READS | _IO_CURRENTLY_PUTTING)) == 0
              && fp->_IO_read_base != NULL));
# elif defined __sferror || defined __DragonFly__ || defined __ANDROID__
   
  return (fp_->_flags & __SRD) != 0;
# elif defined __EMX__                
  return (fp->_flags & _IOREAD) != 0;
# elif defined __minix                
  return (fp->_flags & _IOREADING) != 0;
# elif defined _IOERR                 
#  if defined __sun                   
  return (fp_->_flag & _IOREAD) != 0 && (fp_->_flag & _IOWRT) == 0;
#  else
  return (fp_->_flag & _IOREAD) != 0;
#  endif
# elif defined __UCLIBC__             
  return (fp->__modeflags & (__FLAG_READONLY | __FLAG_READING)) != 0;
# elif defined __QNX__                
  return ((fp->_Mode & 0x2  ) == 0
          || (fp->_Mode & 0x1000  ) != 0);
# elif defined __MINT__               
  if (!fp->__mode.__write)
    return 1;
  if (!fp->__mode.__read)
    return 0;
#  ifdef _IO_CURRENTLY_GETTING  
  return (fp->__flags & _IO_CURRENTLY_GETTING) != 0;
#  else
  return (fp->__buffer < fp->__get_limit  );
#  endif
# elif defined EPLAN9                 
  if (fp->state == 0   || fp->state == 4  )
    return 0;
  return (fp->state == 3   && (fp->bufl == 0 || fp->rp < fp->wp));
# else
#  error "Please port gnulib freading.c to your platform!"
# endif
}

#endif
