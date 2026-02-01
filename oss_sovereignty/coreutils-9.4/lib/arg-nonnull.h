 
#ifndef _GL_ARG_NONNULL
# if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) || defined __clang__
#  define _GL_ARG_NONNULL(params) __attribute__ ((__nonnull__ params))
# else
#  define _GL_ARG_NONNULL(params)
# endif
#endif
