 
#include <stdio.h>

 
#include <unistd.h>

#include "stdio-impl.h"

int
fseeko (FILE *fp, off_t offset, int whence)
#undef fseeko
#if !HAVE_FSEEKO
# undef fseek
# define fseeko fseek
#endif
#if _GL_WINDOWS_64_BIT_OFF_T
# undef fseeko
# if HAVE__FSEEKI64 && HAVE_DECL__FSEEKI64  
#  define fseeko _fseeki64
# else  
#  define fseeko fseeko64
# endif
#endif
{
#if LSEEK_PIPE_BROKEN
   
  if (lseek (fileno (fp), 0, SEEK_CUR) == -1)
    return EOF;
#endif

   
#if defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1
   
  if (fp->_IO_read_end == fp->_IO_read_ptr
      && fp->_IO_write_ptr == fp->_IO_write_base
      && fp->_IO_save_base == NULL)
#elif defined __sferror || defined __DragonFly__ || defined __ANDROID__
   
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
  if (fp_->_p == fp_->_bf._base
      && fp_->_r == 0
      && fp_->_w == ((fp_->_flags & (__SLBF | __SNBF | __SRD)) == 0  
                     ? fp_->_bf._size
                     : 0)
      && fp_ub._base == NULL)
#elif defined __EMX__                
  if (fp->_ptr == fp->_buffer
      && fp->_rcount == 0
      && fp->_wcount == 0
      && fp->_ungetc_count == 0)
#elif defined __minix                
  if (fp_->_ptr == fp_->_buf
      && (fp_->_ptr == NULL || fp_->_count == 0))
#elif defined _IOERR                 
  if (fp_->_ptr == fp_->_base
      && (fp_->_ptr == NULL || fp_->_cnt == 0))
#elif defined __UCLIBC__             
  if (((fp->__modeflags & __FLAG_WRITING) == 0
       || fp->__bufpos == fp->__bufstart)
      && ((fp->__modeflags & (__FLAG_READONLY | __FLAG_READING)) == 0
          || fp->__bufpos == fp->__bufread))
#elif defined __QNX__                
  if ((fp->_Mode & 0x2000   ? fp->_Next == fp->_Buf : fp->_Next == fp->_Rend)
      && fp->_Rback == fp->_Back + sizeof (fp->_Back)
      && fp->_Rsave == NULL)
#elif defined __MINT__               
  if (fp->__bufp == fp->__buffer
      && fp->__get_limit == fp->__bufp
      && fp->__put_limit == fp->__bufp
      && !fp->__pushed_back)
#elif defined EPLAN9                 
  if (fp->rp == fp->buf
      && fp->wp == fp->buf)
#elif FUNC_FFLUSH_STDIN < 0 && 200809 <= _POSIX_VERSION
   
  if (0)
#else
  #error "Please port gnulib fseeko.c to your platform! Look at the code in fseeko.c, then report this to bug-gnulib."
#endif
    {
       
      off_t pos = lseek (fileno (fp), offset, whence);
      if (pos == -1)
        {
#if defined __sferror || defined __DragonFly__ || defined __ANDROID__
           
          fp_->_flags &= ~__SOFF;
#endif
          return -1;
        }

#if defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1
       
      fp->_flags &= ~_IO_EOF_SEEN;
      fp->_offset = pos;
#elif defined __sferror || defined __DragonFly__ || defined __ANDROID__
       
# if defined __CYGWIN__ || (defined __NetBSD__ && __NetBSD_Version__ >= 600000000) || defined __minix
       
      fp_->_offset = pos;
# else
       
      {
         
        union
          {
            fpos_t f;
            off_t o;
          } u;
        u.o = pos;
        fp_->_offset = u.f;
      }
# endif
      fp_->_flags |= __SOFF;
      fp_->_flags &= ~__SEOF;
#elif defined __EMX__                
      fp->_flags &= ~_IOEOF;
#elif defined _IOERR                 
      fp_->_flag &= ~_IOEOF;
#elif defined __MINT__               
      fp->__offset = pos;
      fp->__eof = 0;
#endif
      return 0;
    }
  return fseeko (fp, offset, whence);
}
