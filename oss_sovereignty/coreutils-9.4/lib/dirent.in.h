 
#if @HAVE_DIRENT_H@
# @INCLUDE_NEXT@ @NEXT_DIRENT_H@
#endif

#ifndef _@GUARD_PREFIX@_DIRENT_H
#define _@GUARD_PREFIX@_DIRENT_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <sys/types.h>

#if !@HAVE_DIRENT_H@
 
# if !GNULIB_defined_struct_dirent
struct dirent
{
  char d_type;
  char d_name[1];
};
 
#  define DT_UNKNOWN 0
#  define DT_FIFO    1           
#  define DT_CHR     2           
#  define DT_DIR     4           
#  define DT_BLK     6           
#  define DT_REG     8           
#  define DT_LNK    10           
#  define DT_SOCK   12           
#  define DT_WHT    14           
#  define GNULIB_defined_struct_dirent 1
# endif
#endif

#if !@DIR_HAS_FD_MEMBER@
# if !GNULIB_defined_DIR
 
struct gl_directory;
#  if @HAVE_DIRENT_H@
#   define DIR struct gl_directory
#  else
typedef struct gl_directory DIR;
#  endif
#  define GNULIB_defined_DIR 1
# endif
#endif

 
#ifndef _GL_ATTRIBUTE_DEALLOC
# if __GNUC__ >= 11
#  define _GL_ATTRIBUTE_DEALLOC(f, i) __attribute__ ((__malloc__ (f, i)))
# else
#  define _GL_ATTRIBUTE_DEALLOC(f, i)
# endif
#endif

 
 
#ifndef _GL_ATTRIBUTE_MALLOC
# if __GNUC__ >= 3 || defined __clang__
#  define _GL_ATTRIBUTE_MALLOC __attribute__ ((__malloc__))
# else
#  define _GL_ATTRIBUTE_MALLOC
# endif
#endif

 
#ifndef _GL_ATTRIBUTE_PURE
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96) || defined __clang__
#  define _GL_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define _GL_ATTRIBUTE_PURE  
# endif
#endif

 

 

 


 

#if @GNULIB_CLOSEDIR@
# if @REPLACE_CLOSEDIR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef closedir
#   define closedir rpl_closedir
#   define GNULIB_defined_closedir 1
#  endif
_GL_FUNCDECL_RPL (closedir, int, (DIR *dirp) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (closedir, int, (DIR *dirp));
# else
#  if !@HAVE_CLOSEDIR@
_GL_FUNCDECL_SYS (closedir, int, (DIR *dirp) _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (closedir, int, (DIR *dirp));
# endif
_GL_CXXALIASWARN (closedir);
#elif defined GNULIB_POSIXCHECK
# undef closedir
# if HAVE_RAW_DECL_CLOSEDIR
_GL_WARN_ON_USE (closedir, "closedir is not portable - "
                 "use gnulib module closedir for portability");
# endif
#endif

#if @GNULIB_OPENDIR@
# if @REPLACE_OPENDIR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef opendir
#   define opendir rpl_opendir
#   define GNULIB_defined_opendir 1
#  endif
_GL_FUNCDECL_RPL (opendir, DIR *,
                  (const char *dir_name)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (closedir, 1));
_GL_CXXALIAS_RPL (opendir, DIR *, (const char *dir_name));
# else
#  if !@HAVE_OPENDIR@ || __GNUC__ >= 11
_GL_FUNCDECL_SYS (opendir, DIR *,
                  (const char *dir_name)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (closedir, 1));
#  endif
_GL_CXXALIAS_SYS (opendir, DIR *, (const char *dir_name));
# endif
_GL_CXXALIASWARN (opendir);
#else
# if @GNULIB_CLOSEDIR@ && !GNULIB_defined_DIR && __GNUC__ >= 11 && !defined opendir
 
_GL_FUNCDECL_SYS (opendir, DIR *,
                  (const char *dir_name)
                  _GL_ARG_NONNULL ((1))
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (closedir, 1));
# endif
# if defined GNULIB_POSIXCHECK
#  undef opendir
#  if HAVE_RAW_DECL_OPENDIR
_GL_WARN_ON_USE (opendir, "opendir is not portable - "
                 "use gnulib module opendir for portability");
#  endif
# endif
#endif

#if @GNULIB_READDIR@
# if @REPLACE_READDIR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef readdir
#   define readdir rpl_readdir
#  endif
_GL_FUNCDECL_RPL (readdir, struct dirent *, (DIR *dirp) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (readdir, struct dirent *, (DIR *dirp));
# else
#  if !@HAVE_READDIR@
_GL_FUNCDECL_SYS (readdir, struct dirent *, (DIR *dirp) _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (readdir, struct dirent *, (DIR *dirp));
# endif
_GL_CXXALIASWARN (readdir);
#elif defined GNULIB_POSIXCHECK
# undef readdir
# if HAVE_RAW_DECL_READDIR
_GL_WARN_ON_USE (readdir, "readdir is not portable - "
                 "use gnulib module readdir for portability");
# endif
#endif

#if @GNULIB_REWINDDIR@
# if @REPLACE_REWINDDIR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef rewinddir
#   define rewinddir rpl_rewinddir
#  endif
_GL_FUNCDECL_RPL (rewinddir, void, (DIR *dirp) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (rewinddir, void, (DIR *dirp));
# else
#  if !@HAVE_REWINDDIR@
_GL_FUNCDECL_SYS (rewinddir, void, (DIR *dirp) _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (rewinddir, void, (DIR *dirp));
# endif
_GL_CXXALIASWARN (rewinddir);
#elif defined GNULIB_POSIXCHECK
# undef rewinddir
# if HAVE_RAW_DECL_REWINDDIR
_GL_WARN_ON_USE (rewinddir, "rewinddir is not portable - "
                 "use gnulib module rewinddir for portability");
# endif
#endif

#if @GNULIB_DIRFD@
 
# if @REPLACE_DIRFD@
 
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE) || defined dirfd
#   undef dirfd
#   define dirfd rpl_dirfd
#  endif
_GL_FUNCDECL_RPL (dirfd, int, (DIR *) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (dirfd, int, (DIR *));

#  ifdef __KLIBC__
 
_GL_EXTERN_C int _gl_register_dirp_fd (int fd, DIR *dirp)
     _GL_ARG_NONNULL ((2));
_GL_EXTERN_C void _gl_unregister_dirp_fd (int fd);
#  endif
# else
#  if defined __cplusplus && defined GNULIB_NAMESPACE && defined dirfd
     
static inline int (dirfd) (DIR *dp) { return dirfd (dp); }
#   undef dirfd
#  endif
#  if !(@HAVE_DECL_DIRFD@ || defined dirfd)
_GL_FUNCDECL_SYS (dirfd, int, (DIR *) _GL_ARG_NONNULL ((1)));
#  endif
_GL_CXXALIAS_SYS (dirfd, int, (DIR *));
# endif
_GL_CXXALIASWARN (dirfd);
#elif defined GNULIB_POSIXCHECK
# undef dirfd
# if HAVE_RAW_DECL_DIRFD
_GL_WARN_ON_USE (dirfd, "dirfd is unportable - "
                 "use gnulib module dirfd for portability");
# endif
#endif

#if @GNULIB_FDOPENDIR@
 
# if @REPLACE_FDOPENDIR@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef fdopendir
#   define fdopendir rpl_fdopendir
#  endif
_GL_FUNCDECL_RPL (fdopendir, DIR *,
                  (int fd)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (closedir, 1));
_GL_CXXALIAS_RPL (fdopendir, DIR *, (int fd));
# else
#  if !@HAVE_FDOPENDIR@ || !@HAVE_DECL_FDOPENDIR@ || __GNUC__ >= 11
_GL_FUNCDECL_SYS (fdopendir, DIR *,
                  (int fd)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (closedir, 1));
#  endif
_GL_CXXALIAS_SYS (fdopendir, DIR *, (int fd));
# endif
_GL_CXXALIASWARN (fdopendir);
#else
# if @GNULIB_CLOSEDIR@ && __GNUC__ >= 11 && !defined fdopendir
 
_GL_FUNCDECL_SYS (fdopendir, DIR *,
                  (int fd)
                  _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC (closedir, 1));
# endif
# if defined GNULIB_POSIXCHECK
#  undef fdopendir
#  if HAVE_RAW_DECL_FDOPENDIR
_GL_WARN_ON_USE (fdopendir, "fdopendir is unportable - "
                 "use gnulib module fdopendir for portability");
#  endif
# endif
#endif

#if @GNULIB_SCANDIR@
 
# if !@HAVE_SCANDIR@
_GL_FUNCDECL_SYS (scandir, int,
                  (const char *dir, struct dirent ***namelist,
                   int (*filter) (const struct dirent *),
                   int (*cmp) (const struct dirent **, const struct dirent **))
                  _GL_ARG_NONNULL ((1, 2, 4)));
# endif
 
_GL_CXXALIAS_SYS_CAST (scandir, int,
                       (const char *dir, struct dirent ***namelist,
                        int (*filter) (const struct dirent *),
                        int (*cmp) (const struct dirent **, const struct dirent **)));
_GL_CXXALIASWARN (scandir);
#elif defined GNULIB_POSIXCHECK
# undef scandir
# if HAVE_RAW_DECL_SCANDIR
_GL_WARN_ON_USE (scandir, "scandir is unportable - "
                 "use gnulib module scandir for portability");
# endif
#endif

#if @GNULIB_ALPHASORT@
 
# if !@HAVE_ALPHASORT@
_GL_FUNCDECL_SYS (alphasort, int,
                  (const struct dirent **, const struct dirent **)
                  _GL_ATTRIBUTE_PURE
                  _GL_ARG_NONNULL ((1, 2)));
# endif
 
_GL_CXXALIAS_SYS_CAST (alphasort, int,
                       (const struct dirent **, const struct dirent **));
_GL_CXXALIASWARN (alphasort);
#elif defined GNULIB_POSIXCHECK
# undef alphasort
# if HAVE_RAW_DECL_ALPHASORT
_GL_WARN_ON_USE (alphasort, "alphasort is unportable - "
                 "use gnulib module alphasort for portability");
# endif
#endif


#endif  
#endif  
