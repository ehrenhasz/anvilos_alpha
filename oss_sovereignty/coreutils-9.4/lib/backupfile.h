 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <fcntl.h>

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


 
enum backup_type
{
   
  no_backups,

   
  simple_backups,

   
  numbered_existing_backups,

   
  numbered_backups
};

#define VALID_BACKUP_TYPE(Type) \
  ((unsigned int) (Type) <= numbered_backups)

extern char const *simple_backup_suffix;

void set_simple_backup_suffix (char const *);
char *backup_file_rename (int, char const *, enum backup_type)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
char *find_backup_file_name (int, char const *, enum backup_type)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;
enum backup_type get_version (char const *context, char const *arg);
enum backup_type xget_version (char const *context, char const *arg);


#ifdef __cplusplus
}
#endif

#endif  
