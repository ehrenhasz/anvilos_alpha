 

#include <config.h>

 
#include <stdio.h>

#include <errno.h>
#include <unistd.h>

#include "freading.h"

#include "stdio-impl.h"

#undef fflush


#if defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1
 

 
static void
clear_ungetc_buffer_preserving_position (FILE *fp)
{
  if (fp->_flags & _IO_IN_BACKUP)
     
    fseeko (fp, 0, SEEK_CUR);
}

#else

 
static void
clear_ungetc_buffer (FILE *fp)
{
# if defined __sferror || defined __DragonFly__ || defined __ANDROID__
   
  if (HASUB (fp))
    {
      fp_->_p += fp_->_r;
      fp_->_r = 0;
    }
# elif defined __EMX__               
  if (fp->_ungetc_count > 0)
    {
      fp->_ungetc_count = 0;
      fp->_rcount = - fp->_rcount;
    }
# elif defined _IOERR                
   
# else                               
  fseeko (fp, 0, SEEK_CUR);
# endif
}

#endif

#if ! (defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1)
 

# if (defined __sferror || defined __DragonFly__ || defined __ANDROID__) && defined __SNPT
 

static int
disable_seek_optimization (FILE *fp)
{
  int saved_flags = fp_->_flags & (__SOPT | __SNPT);
  fp_->_flags = (fp_->_flags & ~__SOPT) | __SNPT;
  return saved_flags;
}

static void
restore_seek_optimization (FILE *fp, int saved_flags)
{
  fp_->_flags = (fp_->_flags & ~(__SOPT | __SNPT)) | saved_flags;
}

# else

static void
update_fpos_cache (_GL_ATTRIBUTE_MAYBE_UNUSED FILE *fp,
                   _GL_ATTRIBUTE_MAYBE_UNUSED off_t pos)
{
#  if defined __sferror || defined __DragonFly__ || defined __ANDROID__
   
#   if defined __CYGWIN__ || defined __ANDROID__
   
  fp_->_offset = pos;
#   else
   
   
  union
    {
      fpos_t f;
      off_t o;
    } u;
  u.o = pos;
  fp_->_offset = u.f;
#   endif
  fp_->_flags |= __SOFF;
#  endif
}
# endif
#endif

 
int
rpl_fflush (FILE *stream)
{
   
  if (stream == NULL || ! freading (stream))
    return fflush (stream);

#if defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1
   

  clear_ungetc_buffer_preserving_position (stream);

  return fflush (stream);

#else
  {
     

     
    off_t pos = ftello (stream);
    if (pos == -1)
      {
        errno = EBADF;
        return EOF;
      }

     
    clear_ungetc_buffer (stream);

     
    {
      int result = fpurge (stream);
      if (result != 0)
        return result;
    }

# if (defined __sferror || defined __DragonFly__ || defined __ANDROID__) && defined __SNPT
     

    {
       
      int saved_flags = disable_seek_optimization (stream);
      int result = fseeko (stream, pos, SEEK_SET);

      restore_seek_optimization (stream, saved_flags);
      return result;
    }

# else

    pos = lseek (fileno (stream), pos, SEEK_SET);
    if (pos == -1)
      return EOF;
     
    update_fpos_cache (stream, pos);

    return 0;

# endif
  }
#endif
}
