 
  V_high,

   
  V_changes_only,

   
  V_off
};

struct Chown_option
{
   
  enum Verbosity verbosity;

   
  bool recurse;

   
  struct dev_ino *root_dev_ino;

   
  bool affect_symlink_referent;

   
  bool force_silent;

   
  char *user_name;

   
  char *group_name;
};

void
chopt_init (struct Chown_option *);

void
chopt_free (struct Chown_option *);

char *
gid_to_name (gid_t)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;

char *
uid_to_name (uid_t)
  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE
  _GL_ATTRIBUTE_RETURNS_NONNULL;

bool
chown_files (char **files, int bit_flags,
             uid_t uid, gid_t gid,
             uid_t required_uid, gid_t required_gid,
             struct Chown_option const *chopt)
  _GL_ATTRIBUTE_NONNULL ();

#endif  
