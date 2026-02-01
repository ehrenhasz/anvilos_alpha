 

 

#ifndef _GL_IGNORE_VALUE_H
#define _GL_IGNORE_VALUE_H

 
#if (3 < __GNUC__ + (4 <= __GNUC_MINOR__)) && !defined __clang__
# define ignore_value(x) \
    (__extension__ ({ __typeof__ (x) __x = (x); (void) __x; }))
#else
# define ignore_value(x) ((void) (x))
#endif

#endif
