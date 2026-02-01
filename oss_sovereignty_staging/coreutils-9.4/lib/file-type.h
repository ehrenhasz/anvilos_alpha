 

#ifndef FILE_TYPE_H
# define FILE_TYPE_H 1

 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <sys/types.h>
# include <sys/stat.h>

char const *c_file_type (struct stat const *) _GL_ATTRIBUTE_PURE;
char const *file_type (struct stat const *) _GL_ATTRIBUTE_PURE;

#endif  
