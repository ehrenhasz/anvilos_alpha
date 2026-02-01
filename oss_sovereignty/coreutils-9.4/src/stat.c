 

#include <config.h>

 
#if ((STAT_STATVFS || STAT_STATVFS64)                                       \
     && (HAVE_STRUCT_STATVFS_F_BASETYPE || HAVE_STRUCT_STATVFS_F_FSTYPENAME \
         || (! HAVE_STRUCT_STATFS_F_FSTYPENAME && HAVE_STRUCT_STATVFS_F_TYPE)))
# define USE_STATVFS 1
#else
# define USE_STATVFS 0
#endif

#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#if USE_STATVFS
# include <sys/statvfs.h>
#elif HAVE_SYS_VFS_H
# include <sys/vfs.h>
#elif HAVE_SYS_MOUNT_H && HAVE_SYS_PARAM_H
 
 
# include <sys/param.h>
# include <sys/mount.h>
# if HAVE_NFS_NFS_CLNT_H && HAVE_NFS_VFS_H
 
#  include <netinet/in.h>
#  include <nfs/nfs_clnt.h>
#  include <nfs/vfs.h>
# endif
#elif HAVE_OS_H  
# include <fs_info.h>
#endif
#include <selinux/selinux.h>

#include "system.h"

#include "areadlink.h"
#include "argmatch.h"
#include "file-type.h"
#include "filemode.h"
#include "fs.h"
#include "getopt.h"
#include "mountlist.h"
#include "quote.h"
#include "stat-size.h"
#include "stat-time.h"
#include "strftime.h"
#include "find-mount-point.h"
#include "xvasprintf.h"
#include "statx.h"

#if HAVE_STATX && defined STATX_INO
# define USE_STATX 1
#else
# define USE_STATX 0
#endif

#if USE_STATVFS
# define STRUCT_STATXFS_F_FSID_IS_INTEGER STRUCT_STATVFS_F_FSID_IS_INTEGER
# define HAVE_STRUCT_STATXFS_F_TYPE HAVE_STRUCT_STATVFS_F_TYPE
# if HAVE_STRUCT_STATVFS_F_NAMEMAX
#  define SB_F_NAMEMAX(S) ((S)->f_namemax)
# endif
# if ! STAT_STATVFS && STAT_STATVFS64
#  define STRUCT_STATVFS struct statvfs64
#  define STATFS statvfs64
# else
#  define STRUCT_STATVFS struct statvfs
#  define STATFS statvfs
# endif
# define STATFS_FRSIZE(S) ((S)->f_frsize)
#else
# define HAVE_STRUCT_STATXFS_F_TYPE HAVE_STRUCT_STATFS_F_TYPE
# if HAVE_STRUCT_STATFS_F_NAMELEN
#  define SB_F_NAMEMAX(S) ((S)->f_namelen)
# elif HAVE_STRUCT_STATFS_F_NAMEMAX
#  define SB_F_NAMEMAX(S) ((S)->f_namemax)
# endif
# define STATFS statfs
# if HAVE_OS_H  
 
NODISCARD
static int
statfs (char const *filename, struct fs_info *buf)
{
  dev_t device = dev_for_path (filename);
  if (device < 0)
    {
      errno = (device == B_ENTRY_NOT_FOUND ? ENOENT
               : device == B_BAD_VALUE ? EINVAL
               : device == B_NAME_TOO_LONG ? ENAMETOOLONG
               : device == B_NO_MEMORY ? ENOMEM
               : device == B_FILE_ERROR ? EIO
               : 0);
      return -1;
    }
   
  return fs_stat_dev (device, buf);
}
#  define f_fsid dev
#  define f_blocks total_blocks
#  define f_bfree free_blocks
#  define f_bavail free_blocks
#  define f_bsize io_size
#  define f_files total_nodes
#  define f_ffree free_nodes
#  define STRUCT_STATVFS struct fs_info
#  define STRUCT_STATXFS_F_FSID_IS_INTEGER true
#  define STATFS_FRSIZE(S) ((S)->block_size)
# else
#  define STRUCT_STATVFS struct statfs
#  define STRUCT_STATXFS_F_FSID_IS_INTEGER STRUCT_STATFS_F_FSID_IS_INTEGER
#  if HAVE_STRUCT_STATFS_F_FRSIZE
#   define STATFS_FRSIZE(S) ((S)->f_frsize)
#  else
#   define STATFS_FRSIZE(S) 0
#  endif
# endif
#endif

#ifdef SB_F_NAMEMAX
# define OUT_NAMEMAX out_uint
#else
 
# define SB_F_NAMEMAX(S) "?"
# define OUT_NAMEMAX out_string
#endif

#if HAVE_STRUCT_STATVFS_F_BASETYPE
# define STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME f_basetype
#else
# if HAVE_STRUCT_STATVFS_F_FSTYPENAME || HAVE_STRUCT_STATFS_F_FSTYPENAME
#  define STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME f_fstypename
# elif HAVE_OS_H  
#  define STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME fsh_name
# endif
#endif

#if HAVE_GETATTRAT
# include <attr.h>
# include <sys/nvpair.h>
#endif

 
#define isodigit(c) ('0' <= (c) && (c) <= '7')
#define octtobin(c) ((c) - '0')
#define hextobin(c) ((c) >= 'a' && (c) <= 'f' ? (c) - 'a' + 10 : \
                     (c) >= 'A' && (c) <= 'F' ? (c) - 'A' + 10 : (c) - '0')

static char const digits[] = "0123456789";

 
static char const printf_flags[] = "'-+ #0I";

 
static char const fmt_terse_fs[] = "%n %i %l %t %s %S %b %f %a %c %d\n";
static char const fmt_terse_regular[] = "%n %s %b %f %u %g %D %i %h %t %T"
                                        " %X %Y %Z %W %o\n";
static char const fmt_terse_selinux[] = "%n %s %b %f %u %g %D %i %h %t %T"
                                        " %X %Y %Z %W %o %C\n";

#define PROGRAM_NAME "stat"

#define AUTHORS proper_name ("Michael Meskes")

enum
{
  PRINTF_OPTION = CHAR_MAX + 1
};

enum cached_mode
{
  cached_default,
  cached_never,
  cached_always
};

static char const *const cached_args[] =
{
  "default", "never", "always", nullptr
};

static enum cached_mode const cached_modes[] =
{
  cached_default, cached_never, cached_always
};

static struct option const long_options[] =
{
  {"dereference", no_argument, nullptr, 'L'},
  {"file-system", no_argument, nullptr, 'f'},
  {"format", required_argument, nullptr, 'c'},
  {"printf", required_argument, nullptr, PRINTF_OPTION},
  {"terse", no_argument, nullptr, 't'},
  {"cached", required_argument, nullptr, 0},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {nullptr, 0, nullptr, 0}
};

 
static bool follow_links;

 
static bool interpret_backslash_escapes;

 
static char const *trailing_delim = "";

 
static char const *decimal_point;
static size_t decimal_point_len;

static bool
print_stat (char *pformat, size_t prefix_len, char mod, char m,
            int fd, char const *filename, void const *data);

 
NODISCARD
static char const *
human_fstype (STRUCT_STATVFS const *statfsbuf)
{
#ifdef STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME
  return statfsbuf->STATXFS_FILE_SYSTEM_TYPE_MEMBER_NAME;
#else
  switch (statfsbuf->f_type)
    {
# if defined __linux__ || defined __ANDROID__

       

       

       

    case S_MAGIC_AAFS:  
      return "aafs";
    case S_MAGIC_ACFS:  
      return "acfs";
    case S_MAGIC_ADFS:  
      return "adfs";
    case S_MAGIC_AFFS:  
      return "affs";
    case S_MAGIC_AFS:  
      return "afs";
    case S_MAGIC_ANON_INODE_FS:  
      return "anon-inode FS";
    case S_MAGIC_AUFS:  
       
      return "aufs";
    case S_MAGIC_AUTOFS:  
      return "autofs";
    case S_MAGIC_BALLOON_KVM:  
      return "balloon-kvm-fs";
    case S_MAGIC_BEFS:  
      return "befs";
    case S_MAGIC_BDEVFS:  
      return "bdevfs";
    case S_MAGIC_BFS:  
      return "bfs";
    case S_MAGIC_BINDERFS:  
      return "binderfs";
    case S_MAGIC_BPF_FS:  
      return "bpf_fs";
    case S_MAGIC_BINFMTFS:  
      return "binfmt_misc";
    case S_MAGIC_BTRFS:  
      return "btrfs";
    case S_MAGIC_BTRFS_TEST:  
      return "btrfs_test";
    case S_MAGIC_CEPH:  
      return "ceph";
    case S_MAGIC_CGROUP:  
      return "cgroupfs";
    case S_MAGIC_CGROUP2:  
      return "cgroup2fs";
    case S_MAGIC_CIFS:  
      return "cifs";
    case S_MAGIC_CODA:  
      return "coda";
    case S_MAGIC_COH:  
      return "coh";
    case S_MAGIC_CONFIGFS:  
      return "configfs";
    case S_MAGIC_CRAMFS:  
      return "cramfs";
    case S_MAGIC_CRAMFS_WEND:  
      return "cramfs-wend";
    case S_MAGIC_DAXFS:  
      return "daxfs";
    case S_MAGIC_DEBUGFS:  
      return "debugfs";
    case S_MAGIC_DEVFS:  
      return "devfs";
    case S_MAGIC_DEVMEM:  
      return "devmem";
    case S_MAGIC_DEVPTS:  
      return "devpts";
    case S_MAGIC_DMA_BUF:  
      return "dma-buf-fs";
    case S_MAGIC_ECRYPTFS:  
      return "ecryptfs";
    case S_MAGIC_EFIVARFS:  
      return "efivarfs";
    case S_MAGIC_EFS:  
      return "efs";
    case S_MAGIC_EROFS_V1:  
      return "erofs";
    case S_MAGIC_EXFAT:  
      return "exfat";
    case S_MAGIC_EXFS:  
      return "exfs";
    case S_MAGIC_EXOFS:  
      return "exofs";
    case S_MAGIC_EXT:  
      return "ext";
    case S_MAGIC_EXT2:  
      return "ext2/ext3";
    case S_MAGIC_EXT2_OLD:  
      return "ext2";
    case S_MAGIC_F2FS:  
      return "f2fs";
    case S_MAGIC_FAT:  
      return "fat";
    case S_MAGIC_FHGFS:  
      return "fhgfs";
    case S_MAGIC_FUSEBLK:  
      return "fuseblk";
    case S_MAGIC_FUSECTL:  
      return "fusectl";
    case S_MAGIC_FUTEXFS:  
      return "futexfs";
    case S_MAGIC_GFS:  
      return "gfs/gfs2";
    case S_MAGIC_GPFS:  
      return "gpfs";
    case S_MAGIC_HFS:  
      return "hfs";
    case S_MAGIC_HFS_PLUS:  
      return "hfs+";
    case S_MAGIC_HFS_X:  
      return "hfsx";
    case S_MAGIC_HOSTFS:  
      return "hostfs";
    case S_MAGIC_HPFS:  
      return "hpfs";
    case S_MAGIC_HUGETLBFS:  
      return "hugetlbfs";
    case S_MAGIC_MTD_INODE_FS:  
      return "inodefs";
    case S_MAGIC_IBRIX:  
      return "ibrix";
    case S_MAGIC_INOTIFYFS:  
      return "inotifyfs";
    case S_MAGIC_ISOFS:  
      return "isofs";
    case S_MAGIC_ISOFS_R_WIN:  
      return "isofs";
    case S_MAGIC_ISOFS_WIN:  
      return "isofs";
    case S_MAGIC_JFFS:  
      return "jffs";
    case S_MAGIC_JFFS2:  
      return "jffs2";
    case S_MAGIC_JFS:  
      return "jfs";
    case S_MAGIC_KAFS:  
      return "k-afs";
    case S_MAGIC_LOGFS:  
      return "logfs";
    case S_MAGIC_LUSTRE:  
      return "lustre";
    case S_MAGIC_M1FS:  
      return "m1fs";
    case S_MAGIC_MINIX:  
      return "minix";
    case S_MAGIC_MINIX_30:  
      return "minix (30 char.)";
    case S_MAGIC_MINIX_V2:  
      return "minix v2";
    case S_MAGIC_MINIX_V2_30:  
      return "minix v2 (30 char.)";
    case S_MAGIC_MINIX_V3:  
      return "minix3";
    case S_MAGIC_MQUEUE:  
      return "mqueue";
    case S_MAGIC_MSDOS:  
      return "msdos";
    case S_MAGIC_NCP:  
      return "novell";
    case S_MAGIC_NFS:  
      return "nfs";
    case S_MAGIC_NFSD:  
      return "nfsd";
    case S_MAGIC_NILFS:  
      return "nilfs";
    case S_MAGIC_NSFS:  
      return "nsfs";
    case S_MAGIC_NTFS:  
      return "ntfs";
    case S_MAGIC_OPENPROM:  
      return "openprom";
    case S_MAGIC_OCFS2:  
      return "ocfs2";
    case S_MAGIC_OVERLAYFS:  
       
      return "overlayfs";
    case S_MAGIC_PANFS:  
      return "panfs";
    case S_MAGIC_PIPEFS:  
       
      return "pipefs";
    case S_MAGIC_PPC_CMM:  
      return "ppc-cmm-fs";
    case S_MAGIC_PRL_FS:  
      return "prl_fs";
    case S_MAGIC_PROC:  
      return "proc";
    case S_MAGIC_PSTOREFS:  
      return "pstorefs";
    case S_MAGIC_QNX4:  
      return "qnx4";
    case S_MAGIC_QNX6:  
      return "qnx6";
    case S_MAGIC_RAMFS:  
      return "ramfs";
    case S_MAGIC_RDTGROUP:  
      return "rdt";
    case S_MAGIC_REISERFS:  
      return "reiserfs";
    case S_MAGIC_ROMFS:  
      return "romfs";
    case S_MAGIC_RPC_PIPEFS:  
      return "rpc_pipefs";
    case S_MAGIC_SDCARDFS:  
      return "sdcardfs";
    case S_MAGIC_SECRETMEM:  
      return "secretmem";
    case S_MAGIC_SECURITYFS:  
      return "securityfs";
    case S_MAGIC_SELINUX:  
      return "selinux";
    case S_MAGIC_SMACK:  
      return "smackfs";
    case S_MAGIC_SMB:  
      return "smb";
    case S_MAGIC_SMB2:  
      return "smb2";
    case S_MAGIC_SNFS:  
      return "snfs";
    case S_MAGIC_SOCKFS:  
      return "sockfs";
    case S_MAGIC_SQUASHFS:  
      return "squashfs";
    case S_MAGIC_SYSFS:  
      return "sysfs";
    case S_MAGIC_SYSV2:  
      return "sysv2";
    case S_MAGIC_SYSV4:  
      return "sysv4";
    case S_MAGIC_TMPFS:  
      return "tmpfs";
    case S_MAGIC_TRACEFS:  
      return "tracefs";
    case S_MAGIC_UBIFS:  
      return "ubifs";
    case S_MAGIC_UDF:  
      return "udf";
    case S_MAGIC_UFS:  
      return "ufs";
    case S_MAGIC_UFS_BYTESWAPPED:  
      return "ufs";
    case S_MAGIC_USBDEVFS:  
      return "usbdevfs";
    case S_MAGIC_V9FS:  
      return "v9fs";
    case S_MAGIC_VBOXSF:  
      return "vboxsf";
    case S_MAGIC_VMHGFS:  
      return "vmhgfs";
    case S_MAGIC_VXFS:  
       
      return "vxfs";
    case S_MAGIC_VZFS:  
      return "vzfs";
    case S_MAGIC_WSLFS:  
      return "wslfs";
    case S_MAGIC_XENFS:  
      return "xenfs";
    case S_MAGIC_XENIX:  
      return "xenix";
    case S_MAGIC_XFS:  
      return "xfs";
    case S_MAGIC_XIAFS:  
      return "xia";
    case S_MAGIC_Z3FOLD:  
      return "z3fold";
    case S_MAGIC_ZFS:  
      return "zfs";
    case S_MAGIC_ZONEFS:  
      return "zonefs";
    case S_MAGIC_ZSMALLOC:  
      return "zsmallocfs";


# elif __GNU__
    case FSTYPE_UFS:
      return "ufs";
    case FSTYPE_NFS:
      return "nfs";
    case FSTYPE_GFS:
      return "gfs";
    case FSTYPE_LFS:
      return "lfs";
    case FSTYPE_SYSV:
      return "sysv";
    case FSTYPE_FTP:
      return "ftp";
    case FSTYPE_TAR:
      return "tar";
    case FSTYPE_AR:
      return "ar";
    case FSTYPE_CPIO:
      return "cpio";
    case FSTYPE_MSLOSS:
      return "msloss";
    case FSTYPE_CPM:
      return "cpm";
    case FSTYPE_HFS:
      return "hfs";
    case FSTYPE_DTFS:
      return "dtfs";
    case FSTYPE_GRFS:
      return "grfs";
    case FSTYPE_TERM:
      return "term";
    case FSTYPE_DEV:
      return "dev";
    case FSTYPE_PROC:
      return "proc";
    case FSTYPE_IFSOCK:
      return "ifsock";
    case FSTYPE_AFS:
      return "afs";
    case FSTYPE_DFS:
      return "dfs";
    case FSTYPE_PROC9:
      return "proc9";
    case FSTYPE_SOCKET:
      return "socket";
    case FSTYPE_MISC:
      return "misc";
    case FSTYPE_EXT2FS:
      return "ext2/ext3";
    case FSTYPE_HTTP:
      return "http";
    case FSTYPE_MEMFS:
      return "memfs";
    case FSTYPE_ISO9660:
      return "iso9660";
# endif
    default:
      {
        unsigned long int type = statfsbuf->f_type;
        static char buf[sizeof "UNKNOWN (0x%lx)" - 3
                        + (sizeof type * CHAR_BIT + 3) / 4];
        sprintf (buf, "UNKNOWN (0x%lx)", type);
        return buf;
      }
    }
#endif
}

NODISCARD
static char *
human_access (struct stat const *statbuf)
{
  static char modebuf[12];
  filemodestring (statbuf, modebuf);
  modebuf[10] = 0;
  return modebuf;
}

NODISCARD
static char *
human_time (struct timespec t)
{
   
  static char str[INT_BUFSIZE_BOUND (intmax_t)
                  + INT_STRLEN_BOUND (int)  
                  + 1  
                  + sizeof "-MM-DD HH:MM:SS.NNNNNNNNN +"];
  static timezone_t tz;
  if (!tz)
    tz = tzalloc (getenv ("TZ"));
  struct tm tm;
  int ns = t.tv_nsec;
  if (localtime_rz (tz, &t.tv_sec, &tm))
    nstrftime (str, sizeof str, "%Y-%m-%d %H:%M:%S.%N %z", &tm, tz, ns);
  else
    {
      char secbuf[INT_BUFSIZE_BOUND (intmax_t)];
      sprintf (str, "%s.%09d", timetostr (t.tv_sec, secbuf), ns);
    }
  return str;
}

 
static void
make_format (char *pformat, size_t prefix_len, char const *allowed_flags,
             char const *suffix)
{
  char *dst = pformat + 1;
  char const *src;
  char const *srclim = pformat + prefix_len;
  for (src = dst; src < srclim && strchr (printf_flags, *src); src++)
    if (strchr (allowed_flags, *src))
      *dst++ = *src;
  while (src < srclim)
    *dst++ = *src++;
  strcpy (dst, suffix);
}

static void
out_string (char *pformat, size_t prefix_len, char const *arg)
{
  make_format (pformat, prefix_len, "-", "s");
  printf (pformat, arg);
}
static int
out_int (char *pformat, size_t prefix_len, intmax_t arg)
{
  make_format (pformat, prefix_len, "'-+ 0", PRIdMAX);
  return printf (pformat, arg);
}
static int
out_uint (char *pformat, size_t prefix_len, uintmax_t arg)
{
  make_format (pformat, prefix_len, "'-0", PRIuMAX);
  return printf (pformat, arg);
}
static void
out_uint_o (char *pformat, size_t prefix_len, uintmax_t arg)
{
  make_format (pformat, prefix_len, "-#0", PRIoMAX);
  printf (pformat, arg);
}
static void
out_uint_x (char *pformat, size_t prefix_len, uintmax_t arg)
{
  make_format (pformat, prefix_len, "-#0", PRIxMAX);
  printf (pformat, arg);
}
static int
out_minus_zero (char *pformat, size_t prefix_len)
{
  make_format (pformat, prefix_len, "'-+ 0", ".0f");
  return printf (pformat, -0.25);
}

 
static void
out_epoch_sec (char *pformat, size_t prefix_len,
               struct timespec arg)
{
  char *dot = memchr (pformat, '.', prefix_len);
  size_t sec_prefix_len = prefix_len;
  int width = 0;
  int precision = 0;
  bool frac_left_adjust = false;

  if (dot)
    {
      sec_prefix_len = dot - pformat;
      pformat[prefix_len] = '\0';

      if (ISDIGIT (dot[1]))
        {
          long int lprec = strtol (dot + 1, nullptr, 10);
          precision = (lprec <= INT_MAX ? lprec : INT_MAX);
        }
      else
        {
          precision = 9;
        }

      if (precision && ISDIGIT (dot[-1]))
        {
           
          char *p = dot;
          *dot = '\0';

          do
            --p;
          while (ISDIGIT (p[-1]));

          long int lwidth = strtol (p, nullptr, 10);
          width = (lwidth <= INT_MAX ? lwidth : INT_MAX);
          if (1 < width)
            {
              p += (*p == '0');
              sec_prefix_len = p - pformat;
              int w_d = (decimal_point_len < width
                         ? width - decimal_point_len
                         : 0);
              if (1 < w_d)
                {
                  int w = w_d - precision;
                  if (1 < w)
                    {
                      char *dst = pformat;
                      for (char const *src = dst; src < p; src++)
                        {
                          if (*src == '-')
                            frac_left_adjust = true;
                          else
                            *dst++ = *src;
                        }
                      sec_prefix_len =
                        (dst - pformat
                         + (frac_left_adjust ? 0 : sprintf (dst, "%d", w)));
                    }
                }
            }
        }
    }

  int divisor = 1;
  for (int i = precision; i < 9; i++)
    divisor *= 10;
  int frac_sec = arg.tv_nsec / divisor;
  int int_len;

  if (TYPE_SIGNED (time_t))
    {
      bool minus_zero = false;
      if (arg.tv_sec < 0 && arg.tv_nsec != 0)
        {
          int frac_sec_modulus = 1000000000 / divisor;
          frac_sec = (frac_sec_modulus - frac_sec
                      - (arg.tv_nsec % divisor != 0));
          arg.tv_sec += (frac_sec != 0);
          minus_zero = (arg.tv_sec == 0);
        }
      int_len = (minus_zero
                 ? out_minus_zero (pformat, sec_prefix_len)
                 : out_int (pformat, sec_prefix_len, arg.tv_sec));
    }
  else
    int_len = out_uint (pformat, sec_prefix_len, arg.tv_sec);

  if (precision)
    {
      int prec = (precision < 9 ? precision : 9);
      int trailing_prec = precision - prec;
      int ilen = (int_len < 0 ? 0 : int_len);
      int trailing_width = (ilen < width && decimal_point_len < width - ilen
                            ? width - ilen - decimal_point_len - prec
                            : 0);
      printf ("%s%.*d%-*.*d", decimal_point, prec, frac_sec,
              trailing_width, trailing_prec, 0);
    }
}

 
NODISCARD
static bool
out_file_context (char *pformat, size_t prefix_len, char const *filename)
{
  char *scontext;
  bool fail = false;

  if ((follow_links
       ? getfilecon (filename, &scontext)
       : lgetfilecon (filename, &scontext)) < 0)
    {
      error (0, errno, _("failed to get security context of %s"),
             quoteaf (filename));
      scontext = nullptr;
      fail = true;
    }
  strcpy (pformat + prefix_len, "s");
  printf (pformat, (scontext ? scontext : "?"));
  if (scontext)
    freecon (scontext);
  return fail;
}

 
NODISCARD
static bool
print_statfs (char *pformat, size_t prefix_len, MAYBE_UNUSED char mod, char m,
              int fd, char const *filename,
              void const *data)
{
  STRUCT_STATVFS const *statfsbuf = data;
  bool fail = false;

  switch (m)
    {
    case 'n':
      out_string (pformat, prefix_len, filename);
      break;

    case 'i':
      {
#if STRUCT_STATXFS_F_FSID_IS_INTEGER
        uintmax_t fsid = statfsbuf->f_fsid;
#else
        typedef unsigned int fsid_word;
        static_assert (alignof (STRUCT_STATVFS) % alignof (fsid_word) == 0);
        static_assert (offsetof (STRUCT_STATVFS, f_fsid) % alignof (fsid_word)
                       == 0);
        static_assert (sizeof statfsbuf->f_fsid % alignof (fsid_word) == 0);
        fsid_word const *p = (fsid_word *) &statfsbuf->f_fsid;

         
        uintmax_t fsid = 0;
        int words = sizeof statfsbuf->f_fsid / sizeof *p;
        for (int i = 0; i < words && i * sizeof *p < sizeof fsid; i++)
          {
            uintmax_t u = p[words - 1 - i];
            fsid |= u << (i * CHAR_BIT * sizeof *p);
          }
#endif
        out_uint_x (pformat, prefix_len, fsid);
      }
      break;

    case 'l':
      OUT_NAMEMAX (pformat, prefix_len, SB_F_NAMEMAX (statfsbuf));
      break;
    case 't':
#if HAVE_STRUCT_STATXFS_F_TYPE
      out_uint_x (pformat, prefix_len, statfsbuf->f_type);
#else
      fputc ('?', stdout);
#endif
      break;
    case 'T':
      out_string (pformat, prefix_len, human_fstype (statfsbuf));
      break;
    case 'b':
      out_int (pformat, prefix_len, statfsbuf->f_blocks);
      break;
    case 'f':
      out_int (pformat, prefix_len, statfsbuf->f_bfree);
      break;
    case 'a':
      out_int (pformat, prefix_len, statfsbuf->f_bavail);
      break;
    case 's':
      out_uint (pformat, prefix_len, statfsbuf->f_bsize);
      break;
    case 'S':
      {
        uintmax_t frsize = STATFS_FRSIZE (statfsbuf);
        if (! frsize)
          frsize = statfsbuf->f_bsize;
        out_uint (pformat, prefix_len, frsize);
      }
      break;
    case 'c':
      out_uint (pformat, prefix_len, statfsbuf->f_files);
      break;
    case 'd':
      out_int (pformat, prefix_len, statfsbuf->f_ffree);
      break;
    default:
      fputc ('?', stdout);
      break;
    }
  return fail;
}

 
NODISCARD
static char const *
find_bind_mount (char const * name)
{
  char const * bind_mount = nullptr;

  static struct mount_entry *mount_list;
  static bool tried_mount_list = false;
  if (!tried_mount_list)  
    {
      if (!(mount_list = read_file_system_list (false)))
        error (0, errno, "%s", _("cannot read table of mounted file systems"));
      tried_mount_list = true;
    }

  struct stat name_stats;
  if (stat (name, &name_stats) != 0)
    return nullptr;

  struct mount_entry *me;
  for (me = mount_list; me; me = me->me_next)
    {
      if (me->me_dummy && me->me_devname[0] == '/'
          && STREQ (me->me_mountdir, name))
        {
          struct stat dev_stats;

          if (stat (me->me_devname, &dev_stats) == 0
              && SAME_INODE (name_stats, dev_stats))
            {
              bind_mount = me->me_devname;
              break;
            }
        }
    }

  return bind_mount;
}

 
NODISCARD
static bool
out_mount_point (char const *filename, char *pformat, size_t prefix_len,
                 const struct stat *statp)
{

  char const *np = "?", *bp = nullptr;
  char *mp = nullptr;
  bool fail = true;

   
  if (follow_links || !S_ISLNK (statp->st_mode))
    {
      char *resolved = canonicalize_file_name (filename);
      if (!resolved)
        {
          error (0, errno, _("failed to canonicalize %s"), quoteaf (filename));
          goto print_mount_point;
        }
      bp = find_bind_mount (resolved);
      free (resolved);
      if (bp)
        {
          fail = false;
          goto print_mount_point;
        }
    }

   
  if ((mp = find_mount_point (filename, statp)))
    {
       
      bp = find_bind_mount (mp);
      fail = false;
    }

print_mount_point:

  out_string (pformat, prefix_len, bp ? bp : mp ? mp : np);
  free (mp);
  return fail;
}

 
static inline struct timespec
neg_to_zero (struct timespec ts)
{
  if (0 <= ts.tv_nsec)
    return ts;
  struct timespec z = {0, 0};
  return z;
}

 

static void
getenv_quoting_style (void)
{
  char const *q_style = getenv ("QUOTING_STYLE");
  if (q_style)
    {
      int i = ARGMATCH (q_style, quoting_style_args, quoting_style_vals);
      if (0 <= i)
        set_quoting_style (nullptr, quoting_style_vals[i]);
      else
        {
          set_quoting_style (nullptr, shell_escape_always_quoting_style);
          error (0, 0, _("ignoring invalid value of environment "
                         "variable QUOTING_STYLE: %s"), quote (q_style));
        }
    }
  else
    set_quoting_style (nullptr, shell_escape_always_quoting_style);
}

 
#define quoteN(x) quotearg_style (get_quoting_style (nullptr), x)

 

static void
print_esc_char (char c)
{
  switch (c)
    {
    case 'a':			 
      c ='\a';
      break;
    case 'b':			 
      c ='\b';
      break;
    case 'e':			 
      c ='\x1B';
      break;
    case 'f':			 
      c ='\f';
      break;
    case 'n':			 
      c ='\n';
      break;
    case 'r':			 
      c ='\r';
      break;
    case 't':			 
      c ='\t';
      break;
    case 'v':			 
      c ='\v';
      break;
    case '"':
    case '\\':
      break;
    default:
      error (0, 0, _("warning: unrecognized escape '\\%c'"), c);
      break;
    }
  putchar (c);
}

ATTRIBUTE_PURE
static size_t
format_code_offset (char const *directive)
{
  size_t len = strspn (directive + 1, printf_flags);
  char const *fmt_char = directive + len + 1;
  fmt_char += strspn (fmt_char, digits);
  if (*fmt_char == '.')
    fmt_char += 1 + strspn (fmt_char + 1, digits);
  return fmt_char - directive;
}

 
NODISCARD
static bool
print_it (char const *format, int fd, char const *filename,
          bool (*print_func) (char *, size_t, char, char,
                              int, char const *, void const *),
          void const *data)
{
  bool fail = false;

   
  enum
    {
      MAX_ADDITIONAL_BYTES =
        (MAX (sizeof PRIdMAX,
              MAX (sizeof PRIoMAX, MAX (sizeof PRIuMAX, sizeof PRIxMAX)))
         - 1)
    };
  size_t n_alloc = strlen (format) + MAX_ADDITIONAL_BYTES + 1;
  char *dest = xmalloc (n_alloc);
  char const *b;
  for (b = format; *b; b++)
    {
      switch (*b)
        {
        case '%':
          {
            size_t len = format_code_offset (b);
            char fmt_char = *(b + len);
            char mod_char = 0;
            memcpy (dest, b, len);
            b += len;

            switch (fmt_char)
              {
              case '\0':
                --b;
                FALLTHROUGH;
              case '%':
                if (1 < len)
                  {
                    dest[len] = fmt_char;
                    dest[len + 1] = '\0';
                    error (EXIT_FAILURE, 0, _("%s: invalid directive"),
                           quote (dest));
                  }
                putchar ('%');
                break;
              case 'H':
              case 'L':
                mod_char = fmt_char;
                fmt_char = *(b + 1);
                if (print_func == print_stat
                    && (fmt_char == 'd' || fmt_char == 'r'))
                  {
                    b++;
                  }
                else
                  {
                    fmt_char = mod_char;
                    mod_char = 0;
                  }
                FALLTHROUGH;
              default:
                fail |= print_func (dest, len, mod_char, fmt_char,
                                    fd, filename, data);
                break;
              }
            break;
          }

        case '\\':
          if ( ! interpret_backslash_escapes)
            {
              putchar ('\\');
              break;
            }
          ++b;
          if (isodigit (*b))
            {
              int esc_value = octtobin (*b);
              int esc_length = 1;	 
              for (++b; esc_length < 3 && isodigit (*b);
                   ++esc_length, ++b)
                {
                  esc_value = esc_value * 8 + octtobin (*b);
                }
              putchar (esc_value);
              --b;
            }
          else if (*b == 'x' && isxdigit (to_uchar (b[1])))
            {
              int esc_value = hextobin (b[1]);	 
               
              ++b;
              if (isxdigit (to_uchar (b[1])))
                {
                  ++b;
                  esc_value = esc_value * 16 + hextobin (*b);
                }
              putchar (esc_value);
            }
          else if (*b == '\0')
            {
              error (0, 0, _("warning: backslash at end of format"));
              putchar ('\\');
               
              --b;
            }
          else
            {
              print_esc_char (*b);
            }
          break;

        default:
          putchar (*b);
          break;
        }
    }
  free (dest);

  fputs (trailing_delim, stdout);

  return fail;
}

 
NODISCARD
static bool
do_statfs (char const *filename, char const *format)
{
  STRUCT_STATVFS statfsbuf;

  if (STREQ (filename, "-"))
    {
      error (0, 0, _("using %s to denote standard input does not work"
                     " in file system mode"), quoteaf (filename));
      return false;
    }

  if (STATFS (filename, &statfsbuf) != 0)
    {
      error (0, errno, _("cannot read file system information for %s"),
             quoteaf (filename));
      return false;
    }

  bool fail = print_it (format, -1, filename, print_statfs, &statfsbuf);
  return ! fail;
}

struct print_args {
  struct stat *st;
  struct timespec btime;
};

 
static bool dont_sync;

 
static bool force_sync;

#if USE_STATX
static unsigned int
fmt_to_mask (char fmt)
{
  switch (fmt)
    {
    case 'N':
      return STATX_MODE;
    case 'd':
    case 'D':
      return STATX_MODE;
    case 'i':
      return STATX_INO;
    case 'a':
    case 'A':
      return STATX_MODE;
    case 'f':
      return STATX_MODE|STATX_TYPE;
    case 'F':
      return STATX_TYPE;
    case 'h':
      return STATX_NLINK;
    case 'u':
    case 'U':
      return STATX_UID;
    case 'g':
    case 'G':
      return STATX_GID;
    case 'm':
      return STATX_MODE|STATX_INO;
    case 's':
      return STATX_SIZE;
    case 't':
    case 'T':
      return STATX_MODE;
    case 'b':
      return STATX_BLOCKS;
    case 'w':
    case 'W':
      return STATX_BTIME;
    case 'x':
    case 'X':
      return STATX_ATIME;
    case 'y':
    case 'Y':
      return STATX_MTIME;
    case 'z':
    case 'Z':
      return STATX_CTIME;
    }
  return 0;
}

ATTRIBUTE_PURE
static unsigned int
format_to_mask (char const *format)
{
  unsigned int mask = 0;
  char const *b;

  for (b = format; *b; b++)
    {
      if (*b != '%')
        continue;

      b += format_code_offset (b);
      if (*b == '\0')
        break;
      mask |= fmt_to_mask (*b);
    }
  return mask;
}

 
NODISCARD
static bool
do_stat (char const *filename, char const *format, char const *format2)
{
  int fd = STREQ (filename, "-") ? 0 : AT_FDCWD;
  int flags = 0;
  struct stat st;
  struct statx stx = { 0, };
  char const *pathname = filename;
  struct print_args pa;
  pa.st = &st;
  pa.btime = (struct timespec) {-1, -1};

  if (AT_FDCWD != fd)
    {
      pathname = "";
      flags = AT_EMPTY_PATH;
    }
  else if (!follow_links)
    {
      flags = AT_SYMLINK_NOFOLLOW;
    }

  if (dont_sync)
    flags |= AT_STATX_DONT_SYNC;
  else if (force_sync)
    flags |= AT_STATX_FORCE_SYNC;

  if (! force_sync)
    flags |= AT_NO_AUTOMOUNT;

  fd = statx (fd, pathname, flags, format_to_mask (format), &stx);
  if (fd < 0)
    {
      if (flags & AT_EMPTY_PATH)
        error (0, errno, _("cannot stat standard input"));
      else
        error (0, errno, _("cannot statx %s"), quoteaf (filename));
      return false;
    }

  if (S_ISBLK (stx.stx_mode) || S_ISCHR (stx.stx_mode))
    format = format2;

  statx_to_stat (&stx, &st);
  if (stx.stx_mask & STATX_BTIME)
    pa.btime = statx_timestamp_to_timespec (stx.stx_btime);

  bool fail = print_it (format, fd, filename, print_stat, &pa);
  return ! fail;
}

#else  

static struct timespec
get_birthtime (int fd, char const *filename, struct stat const *st)
{
  struct timespec ts = get_stat_birthtime (st);

# if HAVE_GETATTRAT
  if (ts.tv_nsec < 0)
    {
      nvlist_t *response;
      if ((fd < 0
           ? getattrat (AT_FDCWD, XATTR_VIEW_READWRITE, filename, &response)
           : fgetattr (fd, XATTR_VIEW_READWRITE, &response))
          == 0)
        {
          uint64_t *val;
          uint_t n;
          if (nvlist_lookup_uint64_array (response, A_CRTIME, &val, &n) == 0
              && 2 <= n
              && val[0] <= TYPE_MAXIMUM (time_t)
              && val[1] < 1000000000 * 2  )
            {
              ts.tv_sec = val[0];
              ts.tv_nsec = val[1];
            }
          nvlist_free (response);
        }
    }
# endif

  return ts;
}


 
NODISCARD
static bool
do_stat (char const *filename, char const *format,
         char const *format2)
{
  int fd = STREQ (filename, "-") ? 0 : -1;
  struct stat statbuf;
  struct print_args pa;
  pa.st = &statbuf;
  pa.btime = (struct timespec) {-1, -1};

  if (0 <= fd)
    {
      if (fstat (fd, &statbuf) != 0)
        {
          error (0, errno, _("cannot stat standard input"));
          return false;
        }
    }
   
  else if ((follow_links
            ? stat (filename, &statbuf)
            : lstat (filename, &statbuf)) != 0)
    {
      error (0, errno, _("cannot stat %s"), quoteaf (filename));
      return false;
    }

  if (S_ISBLK (statbuf.st_mode) || S_ISCHR (statbuf.st_mode))
    format = format2;

  bool fail = print_it (format, fd, filename, print_stat, &pa);
  return ! fail;
}
#endif  

 

static uintmax_t
unsigned_file_size (off_t size)
{
  return size + (size < 0) * ((uintmax_t) OFF_T_MAX - OFF_T_MIN + 1);
}

 
static bool
print_stat (char *pformat, size_t prefix_len, char mod, char m,
            int fd, char const *filename, void const *data)
{
  struct print_args *parg = (struct print_args *) data;
  struct stat *statbuf = parg->st;
  struct timespec btime = parg->btime;
  struct passwd *pw_ent;
  struct group *gw_ent;
  bool fail = false;

  switch (m)
    {
    case 'n':
      out_string (pformat, prefix_len, filename);
      break;
    case 'N':
      out_string (pformat, prefix_len, quoteN (filename));
      if (S_ISLNK (statbuf->st_mode))
        {
          char *linkname = areadlink_with_size (filename, statbuf->st_size);
          if (linkname == nullptr)
            {
              error (0, errno, _("cannot read symbolic link %s"),
                     quoteaf (filename));
              return true;
            }
          printf (" -> ");
          out_string (pformat, prefix_len, quoteN (linkname));
          free (linkname);
        }
      break;
    case 'd':
      if (mod == 'H')
        out_uint (pformat, prefix_len, major (statbuf->st_dev));
      else if (mod == 'L')
        out_uint (pformat, prefix_len, minor (statbuf->st_dev));
      else
        out_uint (pformat, prefix_len, statbuf->st_dev);
      break;
    case 'D':
      out_uint_x (pformat, prefix_len, statbuf->st_dev);
      break;
    case 'i':
      out_uint (pformat, prefix_len, statbuf->st_ino);
      break;
    case 'a':
      out_uint_o (pformat, prefix_len, statbuf->st_mode & CHMOD_MODE_BITS);
      break;
    case 'A':
      out_string (pformat, prefix_len, human_access (statbuf));
      break;
    case 'f':
      out_uint_x (pformat, prefix_len, statbuf->st_mode);
      break;
    case 'F':
      out_string (pformat, prefix_len, file_type (statbuf));
      break;
    case 'h':
      out_uint (pformat, prefix_len, statbuf->st_nlink);
      break;
    case 'u':
      out_uint (pformat, prefix_len, statbuf->st_uid);
      break;
    case 'U':
      pw_ent = getpwuid (statbuf->st_uid);
      out_string (pformat, prefix_len,
                  pw_ent ? pw_ent->pw_name : "UNKNOWN");
      break;
    case 'g':
      out_uint (pformat, prefix_len, statbuf->st_gid);
      break;
    case 'G':
      gw_ent = getgrgid (statbuf->st_gid);
      out_string (pformat, prefix_len,
                  gw_ent ? gw_ent->gr_name : "UNKNOWN");
      break;
    case 'm':
      fail |= out_mount_point (filename, pformat, prefix_len, statbuf);
      break;
    case 's':
      out_uint (pformat, prefix_len, unsigned_file_size (statbuf->st_size));
      break;
    case 'r':
      if (mod == 'H')
        out_uint (pformat, prefix_len, major (statbuf->st_rdev));
      else if (mod == 'L')
        out_uint (pformat, prefix_len, minor (statbuf->st_rdev));
      else
        out_uint (pformat, prefix_len, statbuf->st_rdev);
      break;
    case 'R':
      out_uint_x (pformat, prefix_len, statbuf->st_rdev);
      break;
    case 't':
      out_uint_x (pformat, prefix_len, major (statbuf->st_rdev));
      break;
    case 'T':
      out_uint_x (pformat, prefix_len, minor (statbuf->st_rdev));
      break;
    case 'B':
      out_uint (pformat, prefix_len, ST_NBLOCKSIZE);
      break;
    case 'b':
      out_uint (pformat, prefix_len, ST_NBLOCKS (*statbuf));
      break;
    case 'o':
      out_uint (pformat, prefix_len, ST_BLKSIZE (*statbuf));
      break;
    case 'w':
      {
#if ! USE_STATX
        btime = get_birthtime (fd, filename, statbuf);
#endif
        if (btime.tv_nsec < 0)
          out_string (pformat, prefix_len, "-");
        else
          out_string (pformat, prefix_len, human_time (btime));
      }
      break;
    case 'W':
      {
#if ! USE_STATX
        btime = get_birthtime (fd, filename, statbuf);
#endif
        out_epoch_sec (pformat, prefix_len, neg_to_zero (btime));
      }
      break;
    case 'x':
      out_string (pformat, prefix_len, human_time (get_stat_atime (statbuf)));
      break;
    case 'X':
      out_epoch_sec (pformat, prefix_len, get_stat_atime (statbuf));
      break;
    case 'y':
      out_string (pformat, prefix_len, human_time (get_stat_mtime (statbuf)));
      break;
    case 'Y':
      out_epoch_sec (pformat, prefix_len, get_stat_mtime (statbuf));
      break;
    case 'z':
      out_string (pformat, prefix_len, human_time (get_stat_ctime (statbuf)));
      break;
    case 'Z':
      out_epoch_sec (pformat, prefix_len, get_stat_ctime (statbuf));
      break;
    case 'C':
      fail |= out_file_context (pformat, prefix_len, filename);
      break;
    default:
      fputc ('?', stdout);
      break;
    }
  return fail;
}

 
static char *
default_format (bool fs, bool terse, bool device)
{
  char *format;
  if (fs)
    {
      if (terse)
        format = xstrdup (fmt_terse_fs);
      else
        {
           
          format = xstrdup (_("  File: \"%n\"\n"
                              "    ID: %-8i Namelen: %-7l Type: %T\n"
                              "Block size: %-10s Fundamental block size: %S\n"
                              "Blocks: Total: %-10b Free: %-10f Available: %a\n"
                              "Inodes: Total: %-10c Free: %d\n"));
        }
    }
  else  
    {
      if (terse)
        {
          if (0 < is_selinux_enabled ())
            format = xstrdup (fmt_terse_selinux);
          else
            format = xstrdup (fmt_terse_regular);
        }
      else
        {
          char *temp;
           
          format = xstrdup (_("\
  File: %N\n\
  Size: %-10s\tBlocks: %-10b IO Block: %-6o %F\n\
"));

          temp = format;
          if (device)
            {
              /* TRANSLATORS: This string uses format specifiers from
                 'stat --help' without --file-system, and NOT from printf.  */
              format = xasprintf ("%s%s", format, _("\
" "Device: %Hd,%Ld\tInode: %-10i  Links: %-5h Device type: %Hr,%Lr\n\
"));
            }
          else
            {
              /* TRANSLATORS: This string uses format specifiers from
                 'stat --help' without --file-system, and NOT from printf.  */
              format = xasprintf ("%s%s", format, _("\
" "Device: %Hd,%Ld\tInode: %-10i  Links: %h\n\
"));
            }
          free (temp);

          temp = format;
          /* TRANSLATORS: This string uses format specifiers from
             'stat --help' without --file-system, and NOT from printf.  */
          format = xasprintf ("%s%s", format, _("\
" "Access: (%04a/%10.10A)  Uid: (%5u/%8U)   Gid: (%5g/%8G)\n\
"));
          free (temp);

          if (0 < is_selinux_enabled ())
            {
              temp = format;
              /* TRANSLATORS: This string uses format specifiers from
                 'stat --help' without --file-system, and NOT from printf.  */
              format = xasprintf ("%s%s", format, _("Context: %C\n"));
              free (temp);
            }

          temp = format;
          /* TRANSLATORS: This string uses format specifiers from
             'stat --help' without --file-system, and NOT from printf.  */
          format = xasprintf ("%s%s", format,
                              _("Access: %x\n"
                                "Modify: %y\n"
                                "Change: %z\n"
                                " Birth: %w\n"));
          free (temp);
        }
    }
  return format;
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("Usage: %s [OPTION]... FILE...\n"), program_name);
      fputs (_("\
Display file or file system status.\n\
"), stdout);

      emit_mandatory_arg_note ();

      fputs (_("\
  -L, --dereference     follow links\n\
  -f, --file-system     display file system status instead of file status\n\
"), stdout);
      fputs (_("\
      --cached=MODE     specify how to use cached attributes;\n\
                          useful on remote file systems. See MODE below\n\
"), stdout);
      fputs (_("\
  -c  --format=FORMAT   use the specified FORMAT instead of the default;\n\
                          output a newline after each use of FORMAT\n\
      --printf=FORMAT   like --format, but interpret backslash escapes,\n\
                          and do not output a mandatory trailing newline;\n\
                          if you want a newline, include \\n in FORMAT\n\
  -t, --terse           print the information in terse form\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);

      fputs (_("\n\
The MODE argument of --cached can be: always, never, or default.\n\
'always' will use cached attributes if available, while\n\
'never' will try to synchronize with the latest attributes, and\n\
'default' will leave it up to the underlying file system.\n\
"), stdout);

      fputs (_("\n\
The valid format sequences for files (without --file-system):\n\
\n\
  %a   permission bits in octal (note '#' and '0' printf flags)\n\
  %A   permission bits and file type in human readable form\n\
  %b   number of blocks allocated (see %B)\n\
  %B   the size in bytes of each block reported by %b\n\
  %C   SELinux security context string\n\
"), stdout);
      fputs (_("\
  %d   device number in decimal (st_dev)\n\
  %D   device number in hex (st_dev)\n\
  %Hd  major device number in decimal\n\
  %Ld  minor device number in decimal\n\
  %f   raw mode in hex\n\
  %F   file type\n\
  %g   group ID of owner\n\
  %G   group name of owner\n\
"), stdout);
      fputs (_("\
  %h   number of hard links\n\
  %i   inode number\n\
  %m   mount point\n\
  %n   file name\n\
  %N   quoted file name with dereference if symbolic link\n\
  %o   optimal I/O transfer size hint\n\
  %s   total size, in bytes\n\
  %r   device type in decimal (st_rdev)\n\
  %R   device type in hex (st_rdev)\n\
  %Hr  major device type in decimal, for character/block device special files\n\
  %Lr  minor device type in decimal, for character/block device special files\n\
  %t   major device type in hex, for character/block device special files\n\
  %T   minor device type in hex, for character/block device special files\n\
"), stdout);
      fputs (_("\
  %u   user ID of owner\n\
  %U   user name of owner\n\
  %w   time of file birth, human-readable; - if unknown\n\
  %W   time of file birth, seconds since Epoch; 0 if unknown\n\
  %x   time of last access, human-readable\n\
  %X   time of last access, seconds since Epoch\n\
  %y   time of last data modification, human-readable\n\
  %Y   time of last data modification, seconds since Epoch\n\
  %z   time of last status change, human-readable\n\
  %Z   time of last status change, seconds since Epoch\n\
\n\
"), stdout);

      fputs (_("\
Valid format sequences for file systems:\n\
\n\
  %a   free blocks available to non-superuser\n\
  %b   total data blocks in file system\n\
  %c   total file nodes in file system\n\
  %d   free file nodes in file system\n\
  %f   free blocks in file system\n\
"), stdout);
      fputs (_("\
  %i   file system ID in hex\n\
  %l   maximum length of filenames\n\
  %n   file name\n\
  %s   block size (for faster transfers)\n\
  %S   fundamental block size (for block counts)\n\
  %t   file system type in hex\n\
  %T   file system type in human readable form\n\
"), stdout);

      printf (_("\n\
--terse is equivalent to the following FORMAT:\n\
    %s\
"),
#if HAVE_SELINUX_SELINUX_H
              fmt_terse_selinux
#else
              fmt_terse_regular
#endif
              );

        printf (_("\
--terse --file-system is equivalent to the following FORMAT:\n\
    %s\
"), fmt_terse_fs);

      printf (USAGE_BUILTIN_WARNING, PROGRAM_NAME);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

int
main (int argc, char *argv[])
{
  int c;
  bool fs = false;
  bool terse = false;
  char *format = nullptr;
  char *format2;
  bool ok = true;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  struct lconv const *locale = localeconv ();
  decimal_point = (locale->decimal_point[0] ? locale->decimal_point : ".");
  decimal_point_len = strlen (decimal_point);

  atexit (close_stdout);

  while ((c = getopt_long (argc, argv, "c:fLt", long_options, nullptr)) != -1)
    {
      switch (c)
        {
        case PRINTF_OPTION:
          format = optarg;
          interpret_backslash_escapes = true;
          trailing_delim = "";
          break;

        case 'c':
          format = optarg;
          interpret_backslash_escapes = false;
          trailing_delim = "\n";
          break;

        case 'L':
          follow_links = true;
          break;

        case 'f':
          fs = true;
          break;

        case 't':
          terse = true;
          break;

        case 0:
          switch (XARGMATCH ("--cached", optarg, cached_args, cached_modes))
            {
              case cached_never:
                force_sync = true;
                dont_sync = false;
                break;
              case cached_always:
                force_sync = false;
                dont_sync = true;
                break;
              case cached_default:
                force_sync = false;
                dont_sync = false;
            }
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  if (argc == optind)
    {
      error (0, 0, _("missing operand"));
      usage (EXIT_FAILURE);
    }

  if (format)
    {
      if (strstr (format, "%N"))
        getenv_quoting_style ();
      format2 = format;
    }
  else
    {
      format = default_format (fs, terse,   false);
      format2 = default_format (fs, terse,   true);
    }

  for (int i = optind; i < argc; i++)
    ok &= (fs
           ? do_statfs (argv[i], format)
           : do_stat (argv[i], format, format2));

  main_exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
