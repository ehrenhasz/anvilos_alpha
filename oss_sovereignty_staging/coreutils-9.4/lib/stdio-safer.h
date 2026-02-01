 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdio.h>

#if GNULIB_FOPEN_SAFER
FILE *fopen_safer (char const *, char const *)
  _GL_ARG_NONNULL ((1, 2)) _GL_ATTRIBUTE_DEALLOC (fclose, 1);
#endif

#if GNULIB_FREOPEN_SAFER
FILE *freopen_safer (char const *, char const *, FILE *)
  _GL_ARG_NONNULL ((2, 3));
#endif

#if GNULIB_POPEN_SAFER
FILE *popen_safer (char const *, char const *)
  _GL_ARG_NONNULL ((1, 2)) _GL_ATTRIBUTE_DEALLOC (pclose, 1);
#endif

#if GNULIB_TMPFILE_SAFER
FILE *tmpfile_safer (void)
  _GL_ATTRIBUTE_DEALLOC (fclose, 1);
#endif
