 

#ifndef GL_TEMPNAME_H
# define GL_TEMPNAME_H

# include <stdio.h>

# ifdef __GT_FILE
#  define GT_FILE     __GT_FILE
#  define GT_DIR      __GT_DIR
#  define GT_NOCREATE __GT_NOCREATE
# else
#  define GT_FILE     0
#  define GT_DIR      1
#  define GT_NOCREATE 2
# endif

#ifdef __cplusplus
extern "C" {
#endif

 
extern int gen_tempname (char *tmpl, int suffixlen, int flags, int kind);
 
extern int gen_tempname_len (char *tmpl, int suffixlen, int flags, int kind,
                             size_t x_suffix_len);

 
extern int try_tempname (char *tmpl, int suffixlen, void *args,
                         int (*tryfunc) (char *, void *));
 
extern int try_tempname_len (char *tmpl, int suffixlen, void *args,
                             int (*tryfunc) (char *, void *),
                             size_t x_suffix_len);

#ifdef __cplusplus
}
#endif

#endif  
