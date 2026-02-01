 

 

#if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
#endif
@PRAGMA_COLUMNS@

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#if defined __need_system_sys_stat_h
 

#@INCLUDE_NEXT@ @NEXT_SYS_STAT_H@

#else
 

#ifndef _@GUARD_PREFIX@_SYS_STAT_H

 
#include <sys/types.h>

 
#include <time.h>

 
#@INCLUDE_NEXT@ @NEXT_SYS_STAT_H@

#ifndef _@GUARD_PREFIX@_SYS_STAT_H
#define _@GUARD_PREFIX@_SYS_STAT_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 

 

 

 
#ifdef __KLIBC__
# include <unistd.h>
#endif

 
#if defined _WIN32 && ! defined __CYGWIN__
# include <io.h>      
# include <direct.h>  
#endif

 
#if 0 && (defined _WIN32 && ! defined __CYGWIN__)
# include <io.h>
#endif

 
#if @WINDOWS_64_BIT_ST_SIZE@
# define stat _stati64
#endif

 
#if @GNULIB_OVERRIDES_STRUCT_STAT@

# undef stat
# if @GNULIB_STAT@
#  define stat rpl_stat
# else
    
#  define stat stat_used_without_requesting_gnulib_module_stat
# endif

# if !GNULIB_defined_struct_stat
struct stat
{
  dev_t st_dev;
  ino_t st_ino;
  mode_t st_mode;
  nlink_t st_nlink;
#  if 0
  uid_t st_uid;
#  else  
  short st_uid;
#  endif
#  if 0
  gid_t st_gid;
#  else  
  short st_gid;
#  endif
  dev_t st_rdev;
  off_t st_size;
#  if 0
  blksize_t st_blksize;
  blkcnt_t st_blocks;
#  endif

#  if @WINDOWS_STAT_TIMESPEC@
  struct timespec st_atim;
  struct timespec st_mtim;
  struct timespec st_ctim;
#  else
  time_t st_atime;
  time_t st_mtime;
  time_t st_ctime;
#  endif
};
#  if @WINDOWS_STAT_TIMESPEC@
#   define st_atime st_atim.tv_sec
#   define st_mtime st_mtim.tv_sec
#   define st_ctime st_ctim.tv_sec
     
#   define _GL_WINDOWS_STAT_TIMESPEC 1
#  endif
#  define GNULIB_defined_struct_stat 1
# endif

 
# if 0
#  define _S_IFBLK  0x6000
# endif
# if 0
#  define _S_IFLNK  0xA000
# endif
# if 0
#  define _S_IFSOCK 0xC000
# endif

#endif

#ifndef S_IFIFO
# ifdef _S_IFIFO
#  define S_IFIFO _S_IFIFO
# endif
#endif

#ifndef S_IFMT
# define S_IFMT 0170000
#endif

#if STAT_MACROS_BROKEN
# undef S_ISBLK
# undef S_ISCHR
# undef S_ISDIR
# undef S_ISFIFO
# undef S_ISLNK
# undef S_ISNAM
# undef S_ISMPB
# undef S_ISMPC
# undef S_ISNWK
# undef S_ISREG
# undef S_ISSOCK
#endif

#ifndef S_ISBLK
# ifdef S_IFBLK
#  define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
# else
#  define S_ISBLK(m) 0
# endif
#endif

#ifndef S_ISCHR
# ifdef S_IFCHR
#  define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
# else
#  define S_ISCHR(m) 0
# endif
#endif

#ifndef S_ISDIR
# ifdef S_IFDIR
#  define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
# else
#  define S_ISDIR(m) 0
# endif
#endif

#ifndef S_ISDOOR  
# define S_ISDOOR(m) 0
#endif

#ifndef S_ISFIFO
# ifdef S_IFIFO
#  define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
# else
#  define S_ISFIFO(m) 0
# endif
#endif

#ifndef S_ISLNK
# ifdef S_IFLNK
#  define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
# else
#  define S_ISLNK(m) 0
# endif
#endif

#ifndef S_ISMPB  
# ifdef S_IFMPB
#  define S_ISMPB(m) (((m) & S_IFMT) == S_IFMPB)
#  define S_ISMPC(m) (((m) & S_IFMT) == S_IFMPC)
# else
#  define S_ISMPB(m) 0
#  define S_ISMPC(m) 0
# endif
#endif

#ifndef S_ISMPX  
# define S_ISMPX(m) 0
#endif

#ifndef S_ISNAM  
# ifdef S_IFNAM
#  define S_ISNAM(m) (((m) & S_IFMT) == S_IFNAM)
# else
#  define S_ISNAM(m) 0
# endif
#endif

#ifndef S_ISNWK  
# ifdef S_IFNWK
#  define S_ISNWK(m) (((m) & S_IFMT) == S_IFNWK)
# else
#  define S_ISNWK(m) 0
# endif
#endif

#ifndef S_ISPORT  
# define S_ISPORT(m) 0
#endif

#ifndef S_ISREG
# ifdef S_IFREG
#  define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
# else
#  define S_ISREG(m) 0
# endif
#endif

#ifndef S_ISSOCK
# ifdef S_IFSOCK
#  define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
# else
#  define S_ISSOCK(m) 0
# endif
#endif


#ifndef S_TYPEISMQ
# define S_TYPEISMQ(p) 0
#endif

#ifndef S_TYPEISTMO
# define S_TYPEISTMO(p) 0
#endif


#ifndef S_TYPEISSEM
# ifdef S_INSEM
#  define S_TYPEISSEM(p) (S_ISNAM ((p)->st_mode) && (p)->st_rdev == S_INSEM)
# else
#  define S_TYPEISSEM(p) 0
# endif
#endif

#ifndef S_TYPEISSHM
# ifdef S_INSHD
#  define S_TYPEISSHM(p) (S_ISNAM ((p)->st_mode) && (p)->st_rdev == S_INSHD)
# else
#  define S_TYPEISSHM(p) 0
# endif
#endif

 
#ifndef S_ISCTG
# define S_ISCTG(p) 0
#endif

 
#ifndef S_ISOFD
# define S_ISOFD(p) 0
#endif

 
#ifndef S_ISOFL
# define S_ISOFL(p) 0
#endif

 
#ifndef S_ISWHT
# define S_ISWHT(m) 0
#endif

 
#if !S_ISUID
# define S_ISUID 04000
#endif
#if !S_ISGID
# define S_ISGID 02000
#endif

 
#ifndef S_ISVTX
# define S_ISVTX 01000
#endif

#if !S_IRUSR && S_IREAD
# define S_IRUSR S_IREAD
#endif
#if !S_IRUSR
# define S_IRUSR 00400
#endif
#if !S_IRGRP
# define S_IRGRP (S_IRUSR >> 3)
#endif
#if !S_IROTH
# define S_IROTH (S_IRUSR >> 6)
#endif

#if !S_IWUSR && S_IWRITE
# define S_IWUSR S_IWRITE
#endif
#if !S_IWUSR
# define S_IWUSR 00200
#endif
#if !S_IWGRP
# define S_IWGRP (S_IWUSR >> 3)
#endif
#if !S_IWOTH
# define S_IWOTH (S_IWUSR >> 6)
#endif

#if !S_IXUSR && S_IEXEC
# define S_IXUSR S_IEXEC
#endif
#if !S_IXUSR
# define S_IXUSR 00100
#endif
#if !S_IXGRP
# define S_IXGRP (S_IXUSR >> 3)
#endif
#if !S_IXOTH
# define S_IXOTH (S_IXUSR >> 6)
#endif

#if !S_IRWXU
# define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#endif
#if !S_IRWXG
# define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)
#endif
#if !S_IRWXO
# define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)
#endif

 
#if !S_IXUGO
# define S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)
#endif
#ifndef S_IRWXUGO
# define S_IRWXUGO (S_IRWXU | S_IRWXG | S_IRWXO)
#endif

 
#ifndef UTIME_NOW
# define UTIME_NOW (-1)
# define UTIME_OMIT (-2)
#endif


#if @GNULIB_CHMOD@
# if @REPLACE_CHMOD@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef chmod
#   define chmod rpl_chmod
#  endif
_GL_FUNCDECL_RPL (chmod, int, (const char *filename, mode_t mode)
                               _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (chmod, int, (const char *filename, mode_t mode));
# elif defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef chmod
#   define chmod _chmod
#  endif
 
_GL_CXXALIAS_MDA_CAST (chmod, int, (const char *filename, mode_t mode));
# else
_GL_CXXALIAS_SYS (chmod, int, (const char *filename, mode_t mode));
# endif
_GL_CXXALIASWARN (chmod);
#elif defined GNULIB_POSIXCHECK
# undef chmod
# if HAVE_RAW_DECL_CHMOD
_GL_WARN_ON_USE (chmod, "chmod has portability problems - "
                 "use gnulib module chmod for portability");
# endif
#elif @GNULIB_MDA_CHMOD@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef chmod
#   define chmod _chmod
#  endif
 
_GL_CXXALIAS_MDA_CAST (chmod, int, (const char *filename, mode_t mode));
# else
_GL_CXXALIAS_SYS (chmod, int, (const char *filename, mode_t mode));
# endif
_GL_CXXALIASWARN (chmod);
#endif


#if @GNULIB_FCHMODAT@
# if @REPLACE_FCHMODAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fchmodat
#   define fchmodat rpl_fchmodat
#  endif
_GL_FUNCDECL_RPL (fchmodat, int,
                  (int fd, char const *file, mode_t mode, int flag)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (fchmodat, int,
                  (int fd, char const *file, mode_t mode, int flag));
# else
#  if !@HAVE_FCHMODAT@
_GL_FUNCDECL_SYS (fchmodat, int,
                  (int fd, char const *file, mode_t mode, int flag)
                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (fchmodat, int,
                  (int fd, char const *file, mode_t mode, int flag));
# endif
_GL_CXXALIASWARN (fchmodat);
#elif defined GNULIB_POSIXCHECK
# undef fchmodat
# if HAVE_RAW_DECL_FCHMODAT
_GL_WARN_ON_USE (fchmodat, "fchmodat is not portable - "
                 "use gnulib module openat for portability");
# endif
#endif


#if @GNULIB_FSTAT@
# if @REPLACE_FSTAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fstat
#   define fstat rpl_fstat
#  endif
_GL_FUNCDECL_RPL (fstat, int, (int fd, struct stat *buf) _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (fstat, int, (int fd, struct stat *buf));
# else
_GL_CXXALIAS_SYS (fstat, int, (int fd, struct stat *buf));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (fstat);
# endif
#elif @GNULIB_OVERRIDES_STRUCT_STAT@
# undef fstat
# define fstat fstat_used_without_requesting_gnulib_module_fstat
#elif @WINDOWS_64_BIT_ST_SIZE@
 
# define fstat _fstati64
#elif defined GNULIB_POSIXCHECK
# undef fstat
# if HAVE_RAW_DECL_FSTAT
_GL_WARN_ON_USE (fstat, "fstat has portability problems - "
                 "use gnulib module fstat for portability");
# endif
#endif


#if @GNULIB_FSTATAT@
# if @REPLACE_FSTATAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fstatat
#   define fstatat rpl_fstatat
#  endif
_GL_FUNCDECL_RPL (fstatat, int,
                  (int fd, char const *restrict name, struct stat *restrict st,
                   int flags)
                  _GL_ARG_NONNULL ((2, 3)));
_GL_CXXALIAS_RPL (fstatat, int,
                  (int fd, char const *restrict name, struct stat *restrict st,
                   int flags));
# else
#  if !@HAVE_FSTATAT@
_GL_FUNCDECL_SYS (fstatat, int,
                  (int fd, char const *restrict name, struct stat *restrict st,
                   int flags)
                  _GL_ARG_NONNULL ((2, 3)));
#  endif
_GL_CXXALIAS_SYS (fstatat, int,
                  (int fd, char const *restrict name, struct stat *restrict st,
                   int flags));
# endif
_GL_CXXALIASWARN (fstatat);
#elif @GNULIB_OVERRIDES_STRUCT_STAT@
# undef fstatat
# define fstatat fstatat_used_without_requesting_gnulib_module_fstatat
#elif defined GNULIB_POSIXCHECK
# undef fstatat
# if HAVE_RAW_DECL_FSTATAT
_GL_WARN_ON_USE (fstatat, "fstatat is not portable - "
                 "use gnulib module openat for portability");
# endif
#endif


#if @GNULIB_FUTIMENS@
 
# if @REPLACE_FUTIMENS@ || (!@HAVE_FUTIMENS@ && defined __sun)
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef futimens
#   define futimens rpl_futimens
#  endif
_GL_FUNCDECL_RPL (futimens, int, (int fd, struct timespec const times[2]));
_GL_CXXALIAS_RPL (futimens, int, (int fd, struct timespec const times[2]));
# else
#  if !@HAVE_FUTIMENS@
_GL_FUNCDECL_SYS (futimens, int, (int fd, struct timespec const times[2]));
#  endif
_GL_CXXALIAS_SYS (futimens, int, (int fd, struct timespec const times[2]));
# endif
# if __GLIBC__ >= 2 && @HAVE_FUTIMENS@
_GL_CXXALIASWARN (futimens);
# endif
#elif defined GNULIB_POSIXCHECK
# undef futimens
# if HAVE_RAW_DECL_FUTIMENS
_GL_WARN_ON_USE (futimens, "futimens is not portable - "
                 "use gnulib module futimens for portability");
# endif
#endif


#if @GNULIB_GETUMASK@
# if !@HAVE_GETUMASK@
_GL_FUNCDECL_SYS (getumask, mode_t, (void));
# endif
_GL_CXXALIAS_SYS (getumask, mode_t, (void));
# if @HAVE_GETUMASK@
_GL_CXXALIASWARN (getumask);
# endif
#elif defined GNULIB_POSIXCHECK
# undef getumask
# if HAVE_RAW_DECL_GETUMASK
_GL_WARN_ON_USE (getumask, "getumask is not portable - "
                 "use gnulib module getumask for portability");
# endif
#endif


#if @GNULIB_LCHMOD@
 
# if !@HAVE_LCHMOD@ || defined __hpux
_GL_FUNCDECL_SYS (lchmod, int, (const char *filename, mode_t mode)
                               _GL_ARG_NONNULL ((1)));
# endif
_GL_CXXALIAS_SYS (lchmod, int, (const char *filename, mode_t mode));
_GL_CXXALIASWARN (lchmod);
#elif defined GNULIB_POSIXCHECK
# undef lchmod
# if HAVE_RAW_DECL_LCHMOD
_GL_WARN_ON_USE (lchmod, "lchmod is unportable - "
                 "use gnulib module lchmod for portability");
# endif
#endif


#if @GNULIB_MKDIR@
# if @REPLACE_MKDIR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mkdir
#   define mkdir rpl_mkdir
#  endif
_GL_FUNCDECL_RPL (mkdir, int, (char const *name, mode_t mode)
                               _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (mkdir, int, (char const *name, mode_t mode));
# elif defined _WIN32 && !defined __CYGWIN__
 
#  if !GNULIB_defined_rpl_mkdir
static int
rpl_mkdir (char const *name, mode_t mode)
{
  return _mkdir (name);
}
#   define GNULIB_defined_rpl_mkdir 1
#  endif
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mkdir
#   define mkdir rpl_mkdir
#  endif
_GL_CXXALIAS_RPL (mkdir, int, (char const *name, mode_t mode));
# else
_GL_CXXALIAS_SYS (mkdir, int, (char const *name, mode_t mode));
# endif
_GL_CXXALIASWARN (mkdir);
#elif defined GNULIB_POSIXCHECK
# undef mkdir
# if HAVE_RAW_DECL_MKDIR
_GL_WARN_ON_USE (mkdir, "mkdir does not always support two parameters - "
                 "use gnulib module mkdir for portability");
# endif
#elif @GNULIB_MDA_MKDIR@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !GNULIB_defined_rpl_mkdir
static int
rpl_mkdir (char const *name, mode_t mode)
{
  return _mkdir (name);
}
#   define GNULIB_defined_rpl_mkdir 1
#  endif
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mkdir
#   define mkdir rpl_mkdir
#  endif
_GL_CXXALIAS_RPL (mkdir, int, (char const *name, mode_t mode));
# else
_GL_CXXALIAS_SYS (mkdir, int, (char const *name, mode_t mode));
# endif
_GL_CXXALIASWARN (mkdir);
#endif


#if @GNULIB_MKDIRAT@
# if !@HAVE_MKDIRAT@
_GL_FUNCDECL_SYS (mkdirat, int, (int fd, char const *file, mode_t mode)
                                _GL_ARG_NONNULL ((2)));
# endif
_GL_CXXALIAS_SYS (mkdirat, int, (int fd, char const *file, mode_t mode));
_GL_CXXALIASWARN (mkdirat);
#elif defined GNULIB_POSIXCHECK
# undef mkdirat
# if HAVE_RAW_DECL_MKDIRAT
_GL_WARN_ON_USE (mkdirat, "mkdirat is not portable - "
                 "use gnulib module openat for portability");
# endif
#endif


#if @GNULIB_MKFIFO@
# if @REPLACE_MKFIFO@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mkfifo
#   define mkfifo rpl_mkfifo
#  endif
_GL_FUNCDECL_RPL (mkfifo, int, (char const *file, mode_t mode)
                               _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (mkfifo, int, (char const *file, mode_t mode));
# else
#  if !@HAVE_MKFIFO@
_GL_FUNCDECL_SYS (mkfifo, int, (char const *file, mode_t mode)
                               _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (mkfifo, int, (char const *file, mode_t mode));
# endif
_GL_CXXALIASWARN (mkfifo);
#elif defined GNULIB_POSIXCHECK
# undef mkfifo
# if HAVE_RAW_DECL_MKFIFO
_GL_WARN_ON_USE (mkfifo, "mkfifo is not portable - "
                 "use gnulib module mkfifo for portability");
# endif
#endif


#if @GNULIB_MKFIFOAT@
# if @REPLACE_MKFIFOAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mkfifoat
#   define mkfifoat rpl_mkfifoat
#  endif
_GL_FUNCDECL_RPL (mkfifoat, int, (int fd, char const *file, mode_t mode)
                                 _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (mkfifoat, int, (int fd, char const *file, mode_t mode));
# else
#  if !@HAVE_MKFIFOAT@
_GL_FUNCDECL_SYS (mkfifoat, int, (int fd, char const *file, mode_t mode)
                                 _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (mkfifoat, int, (int fd, char const *file, mode_t mode));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mkfifoat);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mkfifoat
# if HAVE_RAW_DECL_MKFIFOAT
_GL_WARN_ON_USE (mkfifoat, "mkfifoat is not portable - "
                 "use gnulib module mkfifoat for portability");
# endif
#endif


#if @GNULIB_MKNOD@
# if @REPLACE_MKNOD@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mknod
#   define mknod rpl_mknod
#  endif
_GL_FUNCDECL_RPL (mknod, int, (char const *file, mode_t mode, dev_t dev)
                              _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (mknod, int, (char const *file, mode_t mode, dev_t dev));
# else
#  if !@HAVE_MKNOD@
_GL_FUNCDECL_SYS (mknod, int, (char const *file, mode_t mode, dev_t dev)
                              _GL_ARG_NONNULL ((1)));
#  endif
 
_GL_CXXALIAS_SYS_CAST (mknod, int, (char const *file, mode_t mode, dev_t dev));
# endif
_GL_CXXALIASWARN (mknod);
#elif defined GNULIB_POSIXCHECK
# undef mknod
# if HAVE_RAW_DECL_MKNOD
_GL_WARN_ON_USE (mknod, "mknod is not portable - "
                 "use gnulib module mknod for portability");
# endif
#endif


#if @GNULIB_MKNODAT@
# if @REPLACE_MKNODAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef mknodat
#   define mknodat rpl_mknodat
#  endif
_GL_FUNCDECL_RPL (mknodat, int,
                  (int fd, char const *file, mode_t mode, dev_t dev)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (mknodat, int,
                  (int fd, char const *file, mode_t mode, dev_t dev));
# else
#  if !@HAVE_MKNODAT@
_GL_FUNCDECL_SYS (mknodat, int,
                  (int fd, char const *file, mode_t mode, dev_t dev)
                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (mknodat, int,
                  (int fd, char const *file, mode_t mode, dev_t dev));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (mknodat);
# endif
#elif defined GNULIB_POSIXCHECK
# undef mknodat
# if HAVE_RAW_DECL_MKNODAT
_GL_WARN_ON_USE (mknodat, "mknodat is not portable - "
                 "use gnulib module mkfifoat for portability");
# endif
#endif


#if @GNULIB_STAT@
# if @REPLACE_STAT@
#  if !@GNULIB_OVERRIDES_STRUCT_STAT@
     
#   if defined _AIX && defined stat && defined _LARGE_FILES
      
#    undef stat64
#    define stat64(name, st) rpl_stat (name, st)
#   elif @WINDOWS_64_BIT_ST_SIZE@
      
#    if defined __MINGW32__ && defined _stati64
#     ifndef _USE_32BIT_TIME_T
        
#      undef _stat64
#      define _stat64(name, st) rpl_stat (name, st)
#     endif
#    elif defined _MSC_VER && defined _stati64
#     ifdef _USE_32BIT_TIME_T
        
#      undef _stat32i64
#      define _stat32i64(name, st) rpl_stat (name, st)
#     else
        
#      undef _stat64
#      define _stat64(name, st) rpl_stat (name, st)
#     endif
#    else
#     undef _stati64
#     define _stati64(name, st) rpl_stat (name, st)
#    endif
#   elif defined __MINGW32__ && defined stat
#    ifdef _USE_32BIT_TIME_T
       
#     undef _stat32i64
#     define _stat32i64(name, st) rpl_stat (name, st)
#    else
       
#     undef _stat64
#     define _stat64(name, st) rpl_stat (name, st)
#    endif
#   elif defined _MSC_VER && defined stat
#    ifdef _USE_32BIT_TIME_T
       
#     undef _stat32
#     define _stat32(name, st) rpl_stat (name, st)
#    else
       
#     undef _stat64i32
#     define _stat64i32(name, st) rpl_stat (name, st)
#    endif
#   else  
#    undef stat
#    define stat(name, st) rpl_stat (name, st)
#   endif  
#  endif  
_GL_EXTERN_C int stat (const char *restrict name, struct stat *restrict buf)
                      _GL_ARG_NONNULL ((1, 2));
# endif
#elif @GNULIB_OVERRIDES_STRUCT_STAT@
 
#elif defined GNULIB_POSIXCHECK
# undef stat
# if HAVE_RAW_DECL_STAT
_GL_WARN_ON_USE (stat, "stat is unportable - "
                 "use gnulib module stat for portability");
# endif
#endif


#if @GNULIB_LSTAT@
# if ! @HAVE_LSTAT@
 
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define lstat stat
#  endif
_GL_CXXALIAS_RPL_1 (lstat, stat, int,
                    (const char *restrict name, struct stat *restrict buf));
# elif @REPLACE_LSTAT@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef lstat
#   define lstat rpl_lstat
#  endif
_GL_FUNCDECL_RPL (lstat, int,
                  (const char *restrict name, struct stat *restrict buf)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (lstat, int,
                  (const char *restrict name, struct stat *restrict buf));
# else
_GL_CXXALIAS_SYS (lstat, int,
                  (const char *restrict name, struct stat *restrict buf));
# endif
# if @HAVE_LSTAT@
_GL_CXXALIASWARN (lstat);
# endif
#elif @GNULIB_OVERRIDES_STRUCT_STAT@
# undef lstat
# define lstat lstat_used_without_requesting_gnulib_module_lstat
#elif defined GNULIB_POSIXCHECK
# undef lstat
# if HAVE_RAW_DECL_LSTAT
_GL_WARN_ON_USE (lstat, "lstat is unportable - "
                 "use gnulib module lstat for portability");
# endif
#endif


#if @GNULIB_MDA_UMASK@
 
# if defined _WIN32 && !defined __CYGWIN__
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef umask
#   define umask _umask
#  endif
 
_GL_CXXALIAS_MDA_CAST (umask, mode_t, (mode_t mask));
# else
_GL_CXXALIAS_SYS (umask, mode_t, (mode_t mask));
# endif
_GL_CXXALIASWARN (umask);
#endif


#if @GNULIB_UTIMENSAT@
 
# if @REPLACE_UTIMENSAT@ || (!@HAVE_UTIMENSAT@ && defined __sun)
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef utimensat
#   define utimensat rpl_utimensat
#  endif
_GL_FUNCDECL_RPL (utimensat, int, (int fd, char const *name,
                                   struct timespec const times[2], int flag)
                                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (utimensat, int, (int fd, char const *name,
                                   struct timespec const times[2], int flag));
# else
#  if !@HAVE_UTIMENSAT@
_GL_FUNCDECL_SYS (utimensat, int, (int fd, char const *name,
                                   struct timespec const times[2], int flag)
                                  _GL_ARG_NONNULL ((2)));
#  endif
_GL_CXXALIAS_SYS (utimensat, int, (int fd, char const *name,
                                   struct timespec const times[2], int flag));
# endif
# if __GLIBC__ >= 2 && @HAVE_UTIMENSAT@
_GL_CXXALIASWARN (utimensat);
# endif
#elif defined GNULIB_POSIXCHECK
# undef utimensat
# if HAVE_RAW_DECL_UTIMENSAT
_GL_WARN_ON_USE (utimensat, "utimensat is not portable - "
                 "use gnulib module utimensat for portability");
# endif
#endif


#endif  
#endif  
#endif
