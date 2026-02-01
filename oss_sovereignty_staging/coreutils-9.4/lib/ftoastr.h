 

#ifndef _GL_FTOASTR_H
#define _GL_FTOASTR_H

#include "intprops.h"
#include <float.h>
#include <stddef.h>

 

int  ftoastr (char *buf, size_t bufsize, int flags, int width,       float x);
int  dtoastr (char *buf, size_t bufsize, int flags, int width,      double x);
int ldtoastr (char *buf, size_t bufsize, int flags, int width, long double x);

 
int  c_dtoastr (char *buf, size_t bufsize, int flags, int width,      double x);
int c_ldtoastr (char *buf, size_t bufsize, int flags, int width, long double x);


 
enum
  {
     
    FTOASTR_LEFT_JUSTIFY = 1,

     
    FTOASTR_ALWAYS_SIGNED = 2,

     
    FTOASTR_SPACE_POSITIVE = 4,

     
    FTOASTR_ZERO_PAD = 8,

     
    FTOASTR_UPPER_E = 16
  };


 

#if FLT_RADIX == 10  
# define  _GL_FLT_PREC_BOUND  FLT_MANT_DIG
# define  _GL_DBL_PREC_BOUND  DBL_MANT_DIG
# define _GL_LDBL_PREC_BOUND LDBL_MANT_DIG
#else

 
# if FLT_RADIX == 2  
#  define _GL_FLOAT_DIG_BITS_BOUND 1
# elif FLT_RADIX <= 16  
#  define _GL_FLOAT_DIG_BITS_BOUND 4
# else  
#  define _GL_FLOAT_DIG_BITS_BOUND ((int) TYPE_WIDTH (int) - 1)
# endif

 
#define _GL_FLOAT_EXPONENT_STRLEN_BOUND(min, max)  \
  (      -100 < (min) && (max) <     100 ? 3       \
   :    -1000 < (min) && (max) <    1000 ? 4       \
   :   -10000 < (min) && (max) <   10000 ? 5       \
   :  -100000 < (min) && (max) <  100000 ? 6       \
   : -1000000 < (min) && (max) < 1000000 ? 7       \
   : INT_STRLEN_BOUND (int)  )

 
#define _GL_FLOAT_STRLEN_BOUND_L(t, pointlen)                          \
  (1 + _GL_##t##_PREC_BOUND + pointlen + 1                             \
   + _GL_FLOAT_EXPONENT_STRLEN_BOUND (t##_MIN_10_EXP, t##_MAX_10_EXP))
#define  FLT_STRLEN_BOUND_L(pointlen) _GL_FLOAT_STRLEN_BOUND_L ( FLT, pointlen)
#define  DBL_STRLEN_BOUND_L(pointlen) _GL_FLOAT_STRLEN_BOUND_L ( DBL, pointlen)
#define LDBL_STRLEN_BOUND_L(pointlen) _GL_FLOAT_STRLEN_BOUND_L (LDBL, pointlen)

 
#define  FLT_STRLEN_BOUND  FLT_STRLEN_BOUND_L (MB_LEN_MAX)
#define  DBL_STRLEN_BOUND  DBL_STRLEN_BOUND_L (MB_LEN_MAX)
#define LDBL_STRLEN_BOUND LDBL_STRLEN_BOUND_L (MB_LEN_MAX)

 
#define  FLT_BUFSIZE_BOUND ( FLT_STRLEN_BOUND + 1)
#define  DBL_BUFSIZE_BOUND ( DBL_STRLEN_BOUND + 1)
#define LDBL_BUFSIZE_BOUND (LDBL_STRLEN_BOUND + 1)

#endif  
