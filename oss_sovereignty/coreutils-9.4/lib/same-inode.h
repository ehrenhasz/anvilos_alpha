 
#  if _GL_WINDOWS_STAT_INODES
     
#   define SAME_INODE(a, b) \
     (!((a).st_ino == 0 && (a).st_dev == 0) \
      && (a).st_ino == (b).st_ino && (a).st_dev == (b).st_dev)
#  else
     
#   define SAME_INODE(a, b) 0
#  endif
# else
#  define SAME_INODE(a, b)    \
    ((a).st_ino == (b).st_ino \
     && (a).st_dev == (b).st_dev)
# endif

#endif
