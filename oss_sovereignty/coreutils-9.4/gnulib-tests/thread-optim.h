 

#ifndef _THREAD_OPTIM_H
#define _THREAD_OPTIM_H

 

 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#if HAVE_SYS_SINGLE_THREADED_H  
# include <sys/single_threaded.h>
# define gl_multithreaded()  (!__libc_single_threaded)
#else
# define gl_multithreaded()  1
#endif

#endif  
