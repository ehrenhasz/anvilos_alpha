 

 

#ifndef _BSD_BASE64_H
#define _BSD_BASE64_H

#include "includes.h"

#ifndef HAVE___B64_NTOP
# ifndef HAVE_B64_NTOP
int b64_ntop(u_char const *src, size_t srclength, char *target,
    size_t targsize);
# endif  
# define __b64_ntop(a,b,c,d) b64_ntop(a,b,c,d)
#endif  

#ifndef HAVE___B64_PTON
# ifndef HAVE_B64_PTON
int b64_pton(char const *src, u_char *target, size_t targsize);
# endif  
# define __b64_pton(a,b,c) b64_pton(a,b,c)
#endif  

#endif  
