 

#ifndef SAVEWD_H
#define SAVEWD_H 1

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <sys/types.h>

_GL_INLINE_HEADER_BEGIN
#ifndef SAVEWD_INLINE
# define SAVEWD_INLINE _GL_INLINE
#endif

 
struct savewd
{
   
  enum
    {
       
      INITIAL_STATE,

       
      FD_STATE,

       
      FD_POST_CHDIR_STATE,

       
      FORKING_STATE,

       
      ERROR_STATE,

       
      FINAL_STATE
    } state;

   
  union
  {
    int fd;
    int errnum;
    pid_t child;
  } val;
};

 
SAVEWD_INLINE void
savewd_init (struct savewd *wd)
{
  wd->state = INITIAL_STATE;
}


 
enum
  {
     
    SAVEWD_CHDIR_NOFOLLOW = 1,

     
    SAVEWD_CHDIR_SKIP_READABLE = 2
  };

 
int savewd_chdir (struct savewd *wd, char const *dir, int options,
                  int open_result[2]);

 
int savewd_restore (struct savewd *wd, int status);

 
SAVEWD_INLINE int _GL_ATTRIBUTE_PURE
savewd_errno (struct savewd const *wd)
{
  return (wd->state == ERROR_STATE ? wd->val.errnum : 0);
}

 
void savewd_finish (struct savewd *wd);

 
int savewd_process_files (int n_files, char **file,
                          int (*act) (char *, struct savewd *, void *),
                          void *options);

_GL_INLINE_HEADER_END

#endif
