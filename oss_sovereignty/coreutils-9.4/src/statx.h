 
static inline struct timespec
statx_timestamp_to_timespec (struct statx_timestamp tsx)
{
  struct timespec ts;

  ts.tv_sec = tsx.tv_sec;
  ts.tv_nsec = tsx.tv_nsec;
  return ts;
}

static inline void
statx_to_stat (struct statx *stx, struct stat *stat)
{
  stat->st_dev = makedev (stx->stx_dev_major, stx->stx_dev_minor);
  stat->st_ino = stx->stx_ino;
  stat->st_mode = stx->stx_mode;
  stat->st_nlink = stx->stx_nlink;
  stat->st_uid = stx->stx_uid;
  stat->st_gid = stx->stx_gid;
  stat->st_rdev = makedev (stx->stx_rdev_major, stx->stx_rdev_minor);
  stat->st_size = stx->stx_size;
  stat->st_blksize = stx->stx_blksize;
 
#  define SC_ST_BLOCKS st_blocks
  stat->SC_ST_BLOCKS = stx->stx_blocks;
  stat->st_atim = statx_timestamp_to_timespec (stx->stx_atime);
  stat->st_mtim = statx_timestamp_to_timespec (stx->stx_mtime);
  stat->st_ctim = statx_timestamp_to_timespec (stx->stx_ctime);
}
# endif  
#endif  
