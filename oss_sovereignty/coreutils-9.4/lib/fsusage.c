 
# include <sys/statvfs.h>
#else
 
# include <fcntl.h>
# include <unistd.h>
# include <sys/stat.h>
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#if HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#if HAVE_SYS_VFS_H
# include <sys/vfs.h>
#endif
# if HAVE_SYS_FS_S5PARAM_H       
#  include <sys/fs/s5param.h>
# endif
# if HAVE_SYS_STATFS_H
#  include <sys/statfs.h>
# endif
#endif

 
#define PROPAGATE_ALL_ONES(x) \
  ((sizeof (x) < sizeof (uintmax_t) \
    && (~ (x) == (sizeof (x) < sizeof (int) \
                  ? - (1 << (sizeof (x) * CHAR_BIT)) \
                  : 0))) \
   ? UINTMAX_MAX : (uintmax_t) (x))

 
#define EXTRACT_TOP_BIT(x) ((x) \
                            & ((uintmax_t) 1 << (sizeof (x) * CHAR_BIT - 1)))

 
#define PROPAGATE_TOP_BIT(x) ((x) | ~ (EXTRACT_TOP_BIT (x) - 1))

#ifdef STAT_STATVFS
 
# if ! (__linux__ && (__GLIBC__ || __UCLIBC__))
 
#  undef STAT_STATFS2_FRSIZE
static int statvfs_works (void) { return 1; }
# else
#  include <string.h>  
#  include <sys/utsname.h>
#  include <sys/statfs.h>
#  define STAT_STATFS2_BSIZE 1

static int
statvfs_works (void)
{
  static int statvfs_works_cache = -1;
  struct utsname name;
  if (statvfs_works_cache < 0)
    statvfs_works_cache = (uname (&name) == 0
                           && 0 <= strverscmp (name.release, "2.6.36"));
  return statvfs_works_cache;
}
# endif
#endif


 
int
get_fs_usage (char const *file, char const *disk, struct fs_usage *fsp)
{
#ifdef STAT_STATVFS      

  if (statvfs_works ())
    {
      struct statvfs vfsd;

      if (statvfs (file, &vfsd) < 0)
        return -1;

       
      fsp->fsu_blocksize = (vfsd.f_frsize
                            ? PROPAGATE_ALL_ONES (vfsd.f_frsize)
                            : PROPAGATE_ALL_ONES (vfsd.f_bsize));

      fsp->fsu_blocks = PROPAGATE_ALL_ONES (vfsd.f_blocks);
      fsp->fsu_bfree = PROPAGATE_ALL_ONES (vfsd.f_bfree);
      fsp->fsu_bavail = PROPAGATE_TOP_BIT (vfsd.f_bavail);
      fsp->fsu_bavail_top_bit_set = EXTRACT_TOP_BIT (vfsd.f_bavail) != 0;
      fsp->fsu_files = PROPAGATE_ALL_ONES (vfsd.f_files);
      fsp->fsu_ffree = PROPAGATE_ALL_ONES (vfsd.f_ffree);
      return 0;
    }

#endif

#if defined STAT_STATVFS64             

  struct statvfs64 fsd;

  if (statvfs64 (file, &fsd) < 0)
    return -1;

   
  fsp->fsu_blocksize = (fsd.f_frsize
                        ? PROPAGATE_ALL_ONES (fsd.f_frsize)
                        : PROPAGATE_ALL_ONES (fsd.f_bsize));

#elif defined STAT_STATFS3_OSF1          

  struct statfs fsd;

  if (statfs (file, &fsd, sizeof (struct statfs)) != 0)
    return -1;

  fsp->fsu_blocksize = PROPAGATE_ALL_ONES (fsd.f_fsize);

#elif defined STAT_STATFS2_FRSIZE         

  struct statfs fsd;

  if (statfs (file, &fsd) < 0)
    return -1;

  fsp->fsu_blocksize = PROPAGATE_ALL_ONES (fsd.f_frsize);

#elif defined STAT_STATFS2_BSIZE         

  struct statfs fsd;

  if (statfs (file, &fsd) < 0)
    return -1;

  fsp->fsu_blocksize = PROPAGATE_ALL_ONES (fsd.f_bsize);

# ifdef STATFS_TRUNCATES_BLOCK_COUNTS

   
  if (fsd.f_blocks == 0x7fffffff / fsd.f_bsize && fsd.f_spare[0] > 0)
    {
      fsd.f_blocks = fsd.f_spare[0];
      fsd.f_bfree = fsd.f_spare[1];
      fsd.f_bavail = fsd.f_spare[2];
    }
# endif  

#elif defined STAT_STATFS2_FSIZE         

  struct statfs fsd;

  if (statfs (file, &fsd) < 0)
    return -1;

  fsp->fsu_blocksize = PROPAGATE_ALL_ONES (fsd.f_fsize);

#elif defined STAT_STATFS4               

  struct statfs fsd;

  if (statfs (file, &fsd, sizeof fsd, 0) < 0)
    return -1;

   
   fsp->fsu_blocksize = 512;

#endif

#if (defined STAT_STATVFS64 || defined STAT_STATFS3_OSF1                \
     || defined STAT_STATFS2_FRSIZE || defined STAT_STATFS2_BSIZE       \
     || defined STAT_STATFS2_FSIZE || defined STAT_STATFS4)

  fsp->fsu_blocks = PROPAGATE_ALL_ONES (fsd.f_blocks);
  fsp->fsu_bfree = PROPAGATE_ALL_ONES (fsd.f_bfree);
  fsp->fsu_bavail = PROPAGATE_TOP_BIT (fsd.f_bavail);
  fsp->fsu_bavail_top_bit_set = EXTRACT_TOP_BIT (fsd.f_bavail) != 0;
  fsp->fsu_files = PROPAGATE_ALL_ONES (fsd.f_files);
  fsp->fsu_ffree = PROPAGATE_ALL_ONES (fsd.f_ffree);

#endif

  (void) disk;   
  return 0;
}
