 

# undef DIR

struct gl_directory
{
   
  int fd_to_close;
   
  DIR *real_dirp;
};

 
# define DIR struct gl_directory

#else                                    

# define WIN32_LEAN_AND_MEAN
# include <windows.h>

 
# undef WIN32_FIND_DATA
# define WIN32_FIND_DATA WIN32_FIND_DATAA

struct gl_directory
{
   
  int fd_to_close;
   
  int status;
   
  HANDLE current;
   
  WIN32_FIND_DATA entry;
   
  char dir_name_mask[1];
};

#endif

#endif  
