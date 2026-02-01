 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <sys/types.h>

 
struct mount_entry
{
  char *me_devname;              
  char *me_mountdir;             
  char *me_mntroot;              
                                 
  char *me_type;                 
  dev_t me_dev;                  
  unsigned int me_dummy : 1;     
  unsigned int me_remote : 1;    
  unsigned int me_type_malloced : 1;  
  struct mount_entry *me_next;
};

struct mount_entry *read_file_system_list (bool need_fs_type)
  _GL_ATTRIBUTE_MALLOC;
void free_mount_entry (struct mount_entry *entry);

#endif
