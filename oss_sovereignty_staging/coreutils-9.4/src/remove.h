 
  RMI_ALWAYS = 3,
  RMI_SOMETIMES,
  RMI_NEVER
};

struct rm_options
{
   
  bool ignore_missing_files;

   
  enum rm_interactive interactive;

   
   
  bool one_file_system;

   
  bool recursive;

   
  bool remove_empty_directories;

   
  struct dev_ino *root_dev_ino;

   
  bool preserve_all_root;

   
  bool stdin_tty;

   
  bool verbose;

   
  bool require_restore_cwd;
};

enum RM_status
{
   
  RM_OK = 2,
  RM_USER_ACCEPTED,
  RM_USER_DECLINED,
  RM_ERROR,
  RM_NONEMPTY_DIR
};

# define VALID_STATUS(S) \
  ((S) == RM_OK || (S) == RM_USER_ACCEPTED || (S) == RM_USER_DECLINED \
   || (S) == RM_ERROR)

# define UPDATE_STATUS(S, New_value)				\
  do								\
    {								\
      if ((New_value) == RM_ERROR				\
          || ((New_value) == RM_USER_DECLINED && (S) == RM_OK))	\
        (S) = (New_value);					\
    }								\
  while (0)

extern enum RM_status rm (char *const *file, struct rm_options const *x);

#endif
