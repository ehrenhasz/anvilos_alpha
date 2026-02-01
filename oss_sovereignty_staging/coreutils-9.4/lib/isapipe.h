 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#ifndef HAVE_FIFO_PIPES
# define HAVE_FIFO_PIPES (-1)
#endif

int isapipe (int fd);
