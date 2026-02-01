 

#if UNLINK_CANNOT_UNLINK_DIR
# define cannot_unlink_dir() true
#else
bool cannot_unlink_dir (void);
#endif
