 
#ifndef STAT_SIZE_H
#define STAT_SIZE_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif


 
#if !defined DEV_BSIZE && defined BSIZE
# define DEV_BSIZE BSIZE
#endif
#if !defined DEV_BSIZE && defined BBSIZE  
# define DEV_BSIZE BBSIZE
#endif
#ifndef DEV_BSIZE
# define DEV_BSIZE 4096
#endif



 
#ifndef HAVE_STRUCT_STAT_ST_BLOCKS
# define ST_BLKSIZE(statbuf) DEV_BSIZE
   
# if defined _POSIX_SOURCE || !defined BSIZE
#  define ST_NBLOCKS(statbuf) \
  ((statbuf).st_size / ST_NBLOCKSIZE + ((statbuf).st_size % ST_NBLOCKSIZE != 0))
# else
    
#  define ST_NBLOCKS(statbuf) \
  (S_ISREG ((statbuf).st_mode) || S_ISDIR ((statbuf).st_mode) ? \
   st_blocks ((statbuf).st_size) : 0)
# endif
#else
 
# define ST_BLKSIZE(statbuf) ((0 < (statbuf).st_blksize \
                               && (statbuf).st_blksize <= ((size_t)-1) / 8 + 1) \
                              ? (statbuf).st_blksize : DEV_BSIZE)
# if defined hpux || defined __hpux__ || defined __hpux
   
#  define ST_NBLOCKSIZE 1024
# endif
#endif

#ifndef ST_NBLOCKS
# define ST_NBLOCKS(statbuf) ((statbuf).st_blocks)
#endif

#ifndef ST_NBLOCKSIZE
# ifdef S_BLKSIZE
#  define ST_NBLOCKSIZE S_BLKSIZE
# else
#  define ST_NBLOCKSIZE 512
# endif
#endif

#endif  
