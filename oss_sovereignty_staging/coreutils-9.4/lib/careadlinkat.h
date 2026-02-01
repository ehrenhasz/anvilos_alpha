 

#ifndef _GL_CAREADLINKAT_H
#define _GL_CAREADLINKAT_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <fcntl.h>
#include <unistd.h>

struct allocator;

 

char *careadlinkat (int fd, char const *filename,
                    char *restrict buffer, size_t buffer_size,
                    struct allocator const *alloc,
                    ssize_t (*preadlinkat) (int, char const *,
                                            char *, size_t));

 
#if HAVE_READLINKAT
 
#else
 
# ifndef AT_FDCWD
#  define AT_FDCWD (-3041965)
# endif
#endif

#endif  
