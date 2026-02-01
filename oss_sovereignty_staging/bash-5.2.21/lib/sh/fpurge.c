 

 

#include <config.h>

#include "stdc.h"

#include <stdio.h>

 
#define NEED_FPURGE_DECL
#if HAVE_FPURGE
#  define fpurge _bash_fpurge
#endif
extern int fpurge PARAMS((FILE *stream));

#if HAVE___FPURGE                    
# include <stdio_ext.h>
#endif
#include <stdlib.h>

 

 

 

#if defined __NetBSD__                          
 
# include <sys/param.h>
#endif

#if defined __sferror || defined __DragonFly__  

# if defined __DragonFly__           
    \
                       } *) fp)
   
   
       
    };
#  define fp_ub ((struct __sfileext *) fp->_ext._base)->_ub
# else                                          
#  define fp_ub fp_->_ub
# endif

# define HASUB(fp) (fp_ub._base != NULL)

#endif

 

#if defined _IOERR

# if defined __sun && defined _LP64  
#  define fp_ ((struct { unsigned char *_ptr; \
                         unsigned char *_base; \
                         unsigned char *_end; \
                         long _cnt; \
                         int _file; \
                         unsigned int _flag; \
                       } *) fp)
# else
#  define fp_ fp
# endif

# if defined _SCO_DS                 
#  define _cnt __cnt
#  define _ptr __ptr
#  define _base __base
#  define _flag __flag
# endif

#endif

int
fpurge (FILE *fp)
{
#if HAVE___FPURGE                    

  __fpurge (fp);
   
  return 0;

#elif HAVE_FPURGE                    

   
# undef fpurge
# if !HAVE_DECL_FPURGE
  extern int fpurge (FILE *);
# endif
  int result = fpurge (fp);
# if defined __sferror || defined __DragonFly__  
  if (result == 0)
     
    if ((fp_->_flags & __SRD) != 0)
      fp_->_w = 0;
# endif
  return result;

#else

   
# if defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1  
  fp->_IO_read_end = fp->_IO_read_ptr;
  fp->_IO_write_ptr = fp->_IO_write_base;
   
  if (fp->_IO_save_base != NULL)
    {
      free (fp->_IO_save_base);
      fp->_IO_save_base = NULL;
    }
  return 0;
# elif defined __sferror || defined __DragonFly__  
  fp_->_p = fp_->_bf._base;
  fp_->_r = 0;
  fp_->_w = ((fp_->_flags & (__SLBF | __SNBF | __SRD)) == 0  
	     ? fp_->_bf._size
	     : 0);
   
  if (fp_ub._base != NULL)
    {
      if (fp_ub._base != fp_->_ubuf)
	free (fp_ub._base);
      fp_ub._base = NULL;
    }
  return 0;
# elif defined __EMX__               
  fp->_ptr = fp->_buffer;
  fp->_rcount = 0;
  fp->_wcount = 0;
  fp->_ungetc_count = 0;
  return 0;
# elif defined _IOERR || defined __TANDEM     
  fp->_ptr = fp->_base;
  if (fp->_ptr != NULL)
    fp->_cnt = 0;
  return 0;
# elif defined __UCLIBC__            
#  ifdef __STDIO_BUFFERS
  if (fp->__modeflags & __FLAG_WRITING)
    fp->__bufpos = fp->__bufstart;
  else if (fp->__modeflags & (__FLAG_READONLY | __FLAG_READING))
    fp->__bufpos = fp->__bufread;
#  endif
  return 0;
# elif defined __QNX__               
  fp->_Rback = fp->_Back + sizeof (fp->_Back);
  fp->_Rsave = NULL;
  if (fp->_Mode & 0x2000  )
     
    fp->_Next = fp->_Buf;
  else
     
    fp->_Rend = fp->_Next;
  return 0;
# elif defined __MINT__              
  if (fp->__pushed_back)
    {
      fp->__bufp = fp->__pushback_bufp;
      fp->__pushed_back = 0;
    }
   
  if (fp->__target != -1)
    fp->__target += fp->__bufp - fp->__buffer;
  fp->__bufp = fp->__buffer;
   
  fp->__get_limit = fp->__bufp;
   
  fp->__put_limit = fp->__buffer;
  return 0;
# else
# warning "Please port gnulib fpurge.c to your platform! Look at the definitions of fflush, setvbuf and ungetc on your system, then report this to bug-gnulib."
  return 0;
# endif

#endif
}
