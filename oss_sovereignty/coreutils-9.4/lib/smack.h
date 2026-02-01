 

#ifdef HAVE_SMACK
# include <sys/smack.h>
#else
static inline ssize_t
smack_new_label_from_self (char **label)
{
  return -1;
}

static inline int
smack_set_label_for_self (char const *label)
{
  return -1;
}
#endif

static inline bool
is_smack_enabled (void)
{
#ifdef HAVE_SMACK
  return smack_smackfs_path () != nullptr;
#else
  return false;
#endif
}
