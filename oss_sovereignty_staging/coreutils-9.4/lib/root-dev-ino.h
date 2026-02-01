 

# define ROOT_DEV_INO_CHECK(Root_dev_ino, Dir_statbuf) \
    (Root_dev_ino && SAME_INODE (*Dir_statbuf, *Root_dev_ino))

# define ROOT_DEV_INO_WARN(Dirname)					\
  do									\
    {									\
      if (STREQ (Dirname, "/"))						\
        error (0, 0, _("it is dangerous to operate recursively on %s"),	\
               quoteaf (Dirname));					\
      else								\
        error (0, 0,							\
               _("it is dangerous to operate recursively on %s (same as %s)"), \
               quoteaf_n (0, Dirname), quoteaf_n (1, "/"));		\
      error (0, 0, _("use --no-preserve-root to override this failsafe")); \
    }									\
  while (0)

#endif
