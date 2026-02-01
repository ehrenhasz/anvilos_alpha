 

#ifndef _FTS_H
# define _FTS_H 1

 
# if !_LIBC && !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# ifdef _LIBC
#  include <features.h>
#  if __STDC_VERSION__ < 199901L
#   define __FLEXIBLE_ARRAY_MEMBER 1
#  else
#   define __FLEXIBLE_ARRAY_MEMBER
#  endif
# else
#  define __FLEXIBLE_ARRAY_MEMBER FLEXIBLE_ARRAY_MEMBER
#  undef __THROW
#  define __THROW
#  undef __BEGIN_DECLS
#  undef __END_DECLS
#  ifdef __cplusplus
#   define __BEGIN_DECLS extern "C" {
#   define __END_DECLS }
#  else
#   define __BEGIN_DECLS
#   define __END_DECLS
#  endif
# endif

# include <stddef.h>
# include <sys/types.h>
# include <dirent.h>
# include <sys/stat.h>
# include "i-ring.h"

typedef struct {
        struct _ftsent *fts_cur;         
        struct _ftsent *fts_child;       
        struct _ftsent **fts_array;      
        dev_t fts_dev;                   
        char *fts_path;                  
        int fts_rfd;                     
        int fts_cwd_fd;                  
        size_t fts_pathlen;              
        size_t fts_nitems;               
        int (*fts_compar) (struct _ftsent const **, struct _ftsent const **);
                                         

# define FTS_COMFOLLOW  0x0001           
# define FTS_LOGICAL    0x0002           
# define FTS_NOCHDIR    0x0004           
# define FTS_NOSTAT     0x0008           
# define FTS_PHYSICAL   0x0010           
# define FTS_SEEDOT     0x0020           
# define FTS_XDEV       0x0040           
# define FTS_WHITEOUT   0x0080           

   
# define FTS_TIGHT_CYCLE_CHECK  0x0100

   
# define FTS_CWDFD              0x0200

   
# define FTS_DEFER_STAT         0x0400

   
# define FTS_VERBATIM   0x0800

# define FTS_OPTIONMASK 0x0fff           

# define FTS_NAMEONLY   0x1000           
# define FTS_STOP       0x2000           
        int fts_options;                 

         
        struct hash_table *fts_leaf_optimization_works_ht;

        union {
                 
                struct hash_table *ht;

                 
                 
                struct cycle_check_state *state;
        } fts_cycle;

         
        I_ring fts_fd_ring;
} FTS;

typedef struct _ftsent {
        struct _ftsent *fts_cycle;       
        struct _ftsent *fts_parent;      
        struct _ftsent *fts_link;        
        DIR *fts_dirp;                   
        long fts_number;                 
        void *fts_pointer;               
        char *fts_accpath;               
        char *fts_path;                  
        int fts_errno;                   
        int fts_symfd;                   
        size_t fts_pathlen;              

        FTS *fts_fts;                    

# define FTS_ROOTPARENTLEVEL    (-1)
# define FTS_ROOTLEVEL           0
        ptrdiff_t fts_level;             

        size_t fts_namelen;              

# define FTS_D           1               
# define FTS_DC          2               
# define FTS_DEFAULT     3               
# define FTS_DNR         4               
# define FTS_DOT         5               
# define FTS_DP          6               
# define FTS_ERR         7               
# define FTS_F           8               
# define FTS_INIT        9               
# define FTS_NS         10               
# define FTS_NSOK       11               
# define FTS_SL         12               
# define FTS_SLNONE     13               
# define FTS_W          14               
        unsigned short int fts_info;     

# define FTS_DONTCHDIR   0x01            
# define FTS_SYMFOLLOW   0x02            
        unsigned short int fts_flags;    

# define FTS_AGAIN       1               
# define FTS_FOLLOW      2               
# define FTS_NOINSTR     3               
# define FTS_SKIP        4               
        unsigned short int fts_instr;    

        struct stat fts_statp[1];        
        char fts_name[__FLEXIBLE_ARRAY_MEMBER];  
} FTSENT;

__BEGIN_DECLS

 _GL_ATTRIBUTE_NODISCARD
FTSENT  *fts_children (FTS *, int) __THROW;

_GL_ATTRIBUTE_NODISCARD
int      fts_close (FTS *) __THROW;

_GL_ATTRIBUTE_NODISCARD
FTS     *fts_open (char * const *, int,
                   int (*)(const FTSENT **, const FTSENT **))
  _GL_ATTRIBUTE_DEALLOC (fts_close, 1) __THROW;

_GL_ATTRIBUTE_NODISCARD
FTSENT  *fts_read (FTS *) __THROW;

int      fts_set (FTS *, FTSENT *, int) __THROW;

#if GNULIB_FTS_DEBUG
void     fts_cross_check (FTS const *);
#endif
__END_DECLS

#endif  
