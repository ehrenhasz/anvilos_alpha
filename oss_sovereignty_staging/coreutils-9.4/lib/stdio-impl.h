 

 
#if defined _IO_EOF_SEEN
# if !defined _IO_UNBUFFERED
#  define _IO_UNBUFFERED 0x2
# endif
# if !defined _IO_IN_BACKUP
#  define _IO_IN_BACKUP 0x100
# endif
#endif

 

#if defined __NetBSD__                          
 
# include <sys/param.h>
#endif

#include <errno.h>                              

#if defined __sferror || defined __DragonFly__ || defined __ANDROID__
   

# if defined __DragonFly__           
    \
                       } *) fp)
   
#  ifdef __LP64__
#   define _gl_flags_file_t int
#  else
#   define _gl_flags_file_t short
#  endif
#  ifdef __LP64__
#   define _gl_file_offset_t int64_t
#  else
     
#  define fp_ ((struct { unsigned char *_p; \
                         int _r; \
                         int _w; \
                         _gl_flags_file_t _flags; \
                         _gl_flags_file_t _file; \
                         struct { unsigned char *_base; size_t _size; } _bf; \
                         int _lbfsize; \
                         void *_cookie; \
                         void *_close; \
                         void *_read; \
                         void *_seek; \
                         void *_write; \
                         struct { unsigned char *_base; size_t _size; } _ext; \
                         unsigned char *_up; \
                         int _ur; \
                         unsigned char _ubuf[3]; \
                         unsigned char _nbuf[1]; \
                         struct { unsigned char *_base; size_t _size; } _lb; \
                         int _blksize; \
                         _gl_file_offset_t _offset; \
                           \
                       } *) fp)
# else
#  define fp_ fp
# endif

# if (defined __NetBSD__ && __NetBSD_Version__ >= 105270000) || defined __OpenBSD__ || defined __minix  
   
       
    };
#  define fp_ub ((struct __sfileext *) fp->_ext._base)->_ub
# elif defined __ANDROID__                      
  struct __sfileext
    {
      struct { unsigned char *_base; size_t _size; } _ub;  
       
    };
#  define fp_ub ((struct __sfileext *) fp_->_ext._base)->_ub
# else                                          
#  define fp_ub fp_->_ub
# endif

# define HASUB(fp) (fp_ub._base != NULL)

# if defined __ANDROID__  
   

#ifdef __TANDEM                      
# ifndef _IOERR
 
#  define fp_ ((struct { unsigned char *_ptr; \
                         unsigned char *_base; \
                         unsigned char *_end; \
                         long _cnt; \
                         int _file; \
                         unsigned int _flag; \
                       } *) fp)
# elif defined __VMS                 
#  define fp_ ((struct _iobuf *) fp)
# else
#  define fp_ fp
# endif

# if defined _SCO_DS || (defined __SCO_VERSION__ || defined __sysv5__)   
#  define _cnt __cnt
#  define _ptr __ptr
#  define _base __base
#  define _flag __flag
# endif

#elif defined _WIN32 && ! defined __CYGWIN__   

 
# define WINDOWS_OPAQUE_FILE

struct _gl_real_FILE
{
   
  unsigned char *_ptr;
  unsigned char *_base;
  int _cnt;
  int _flag;
  int _file;
  int _charbuf;
  int _bufsiz;
};
# define fp_ ((struct _gl_real_FILE *) fp)

/* These values were determined by a program similar to the one at
   <https:
# define _IOREAD   0x1
# define _IOWRT    0x2
# define _IORW     0x4
# define _IOEOF    0x8
# define _IOERR   0x10

#endif
