 

#include <config.h>

#if defined LIBC_SCCS && !defined GCC_LINT && !defined lint
static char sccsid[] = "@(#)fts.c       8.6 (Berkeley) 8/14/94";
#endif

#include "fts_.h"

#if HAVE_SYS_PARAM_H || defined _LIBC
# include <sys/param.h>
#endif
#ifdef _LIBC
# include <include/sys/stat.h>
#else
# include <sys/stat.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if ! _LIBC
# include "attribute.h"
# include "fcntl--.h"
# include "flexmember.h"
# include "openat.h"
# include "opendirat.h"
# include "same-inode.h"
#endif

#include <dirent.h>
#ifndef _D_EXACT_NAMLEN
# define _D_EXACT_NAMLEN(dirent) strlen ((dirent)->d_name)
#endif

#if HAVE_STRUCT_DIRENT_D_TYPE
 
# define DT_IS_KNOWN(d) ((d)->d_type != DT_UNKNOWN)
 
# define DT_MUST_BE(d, t) ((d)->d_type == (t))
# define D_TYPE(d) ((d)->d_type)
#else
# define DT_IS_KNOWN(d) false
# define DT_MUST_BE(d, t) false
# define D_TYPE(d) DT_UNKNOWN

# undef DT_UNKNOWN
# define DT_UNKNOWN 0

 
# undef DT_BLK
# undef DT_CHR
# undef DT_DIR
# undef DT_FIFO
# undef DT_LNK
# undef DT_REG
# undef DT_SOCK
# define DT_BLK 1
# define DT_CHR 2
# define DT_DIR 3
# define DT_FIFO 4
# define DT_LNK 5
# define DT_REG 6
# define DT_SOCK 7
#endif

#ifndef S_IFBLK
# define S_IFBLK 0
#endif
#ifndef S_IFLNK
# define S_IFLNK 0
#endif
#ifndef S_IFSOCK
# define S_IFSOCK 0
#endif

enum
{
  NOT_AN_INODE_NUMBER = 0
};

#ifdef D_INO_IN_DIRENT
# define D_INO(dp) (dp)->d_ino
#else
 
# define D_INO(dp) NOT_AN_INODE_NUMBER
#endif

 
#ifndef FTS_MAX_READDIR_ENTRIES
# define FTS_MAX_READDIR_ENTRIES 100000
#endif

 
#ifndef FTS_INODE_SORT_DIR_ENTRIES_THRESHOLD
# define FTS_INODE_SORT_DIR_ENTRIES_THRESHOLD 10000
#endif

enum
{
  _FTS_INODE_SORT_DIR_ENTRIES_THRESHOLD = FTS_INODE_SORT_DIR_ENTRIES_THRESHOLD
};

enum Fts_stat
{
  FTS_NO_STAT_REQUIRED = 1,
  FTS_STAT_REQUIRED = 2
};

#ifdef _LIBC
# undef close
# define close __close
# undef closedir
# define closedir __closedir
# undef fchdir
# define fchdir __fchdir
# undef open
# define open __open
# undef readdir
# define readdir __readdir
#else
# undef internal_function
# define internal_function  
#endif

#ifndef __set_errno
# define __set_errno(Val) errno = (Val)
#endif

 
#ifdef HAVE_OPENAT
# define HAVE_OPENAT_SUPPORT 1
#else
# define HAVE_OPENAT_SUPPORT 0
#endif

#ifdef NDEBUG
# define fts_assert(expr) ((void) (0 && (expr)))
#else
# define fts_assert(expr)       \
    do                          \
      {                         \
        if (!(expr))            \
          abort ();             \
      }                         \
    while (false)
#endif

#ifdef _LIBC
# if __glibc_has_attribute (__fallthrough__)
#  define FALLTHROUGH __attribute__ ((__fallthrough__))
# else
#  define FALLTHROUGH ((void) 0)
# endif
#endif

static FTSENT   *fts_alloc (FTS *, const char *, size_t) internal_function;
static FTSENT   *fts_build (FTS *, int) internal_function;
static void      fts_lfree (FTSENT *) internal_function;
static void      fts_load (FTS *, FTSENT *) internal_function;
static size_t    fts_maxarglen (char * const *) internal_function;
static void      fts_padjust (FTS *, FTSENT *) internal_function;
static bool      fts_palloc (FTS *, size_t) internal_function;
static FTSENT   *fts_sort (FTS *, FTSENT *, size_t) internal_function;
static unsigned short int fts_stat (FTS *, FTSENT *, bool) internal_function;
static int      fts_safe_changedir (FTS *, FTSENT *, int, const char *)
     internal_function;

#include "fts-cycle.c"

#ifndef MAX
# define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

#define ISDOT(a)        (a[0] == '.' && (!a[1] || (a[1] == '.' && !a[2])))
#define STREQ(a, b)     (strcmp (a, b) == 0)

#define CLR(opt)        (sp->fts_options &= ~(opt))
#define ISSET(opt)      ((sp->fts_options & (opt)) != 0)
#define SET(opt)        (sp->fts_options |= (opt))

 
#define FCHDIR(sp, fd)                                  \
  (!ISSET(FTS_NOCHDIR) && (ISSET(FTS_CWDFD)             \
                           ? (cwd_advance_fd ((sp), (fd), true), 0) \
                           : fchdir (fd)))


 
 
#define BCHILD          1                
#define BNAMES          2                
#define BREAD           3                

#if GNULIB_FTS_DEBUG
# include <inttypes.h>
# include <stdio.h>
bool fts_debug = false;
# define Dprintf(x) do { if (fts_debug) printf x; } while (false)
static void fd_ring_check (FTS const *);
static void fd_ring_print (FTS const *, FILE *, char const *);
#else
# define Dprintf(x)
# define fd_ring_check(x)
# define fd_ring_print(a, b, c)
#endif

#define LEAVE_DIR(Fts, Ent, Tag)                                \
  do                                                            \
    {                                                           \
      Dprintf (("  %s-leaving: %s\n", Tag, (Ent)->fts_path));   \
      leave_dir (Fts, Ent);                                     \
      fd_ring_check (Fts);                                      \
    }                                                           \
  while (false)

static void
fd_ring_clear (I_ring *fd_ring)
{
  while ( ! i_ring_empty (fd_ring))
    {
      int fd = i_ring_pop (fd_ring);
      if (0 <= fd)
        close (fd);
    }
}

 
static void
fts_set_stat_required (FTSENT *p, bool required)
{
  fts_assert (p->fts_info == FTS_NSOK);
  p->fts_statp->st_size = (required
                           ? FTS_STAT_REQUIRED
                           : FTS_NO_STAT_REQUIRED);
}

 
static void
internal_function
cwd_advance_fd (FTS *sp, int fd, bool chdir_down_one)
{
  int old = sp->fts_cwd_fd;
  fts_assert (old != fd || old == AT_FDCWD);

  if (chdir_down_one)
    {
       
      int prev_fd_in_slot = i_ring_push (&sp->fts_fd_ring, old);
      fd_ring_print (sp, stderr, "post-push");
      if (0 <= prev_fd_in_slot)
        close (prev_fd_in_slot);  
    }
  else if ( ! ISSET (FTS_NOCHDIR))
    {
      if (0 <= old)
        close (old);  
    }

  sp->fts_cwd_fd = fd;
}

 
static int
restore_initial_cwd (FTS *sp)
{
  int fail = FCHDIR (sp, ISSET (FTS_CWDFD) ? AT_FDCWD : sp->fts_rfd);
  fd_ring_clear (&(sp->fts_fd_ring));
  return fail;
}

 

static int
internal_function
diropen (FTS const *sp, char const *dir)
{
  int open_flags = (O_SEARCH | O_CLOEXEC | O_DIRECTORY | O_NOCTTY | O_NONBLOCK
                    | (ISSET (FTS_PHYSICAL) ? O_NOFOLLOW : 0));

  int fd = (ISSET (FTS_CWDFD)
            ? openat (sp->fts_cwd_fd, dir, open_flags)
            : open (dir, open_flags));
  return fd;
}

FTS *
fts_open (char * const *argv,
          register int options,
          int (*compar) (FTSENT const **, FTSENT const **))
{
        register FTS *sp;
        register FTSENT *p, *root;
        register size_t nitems;
        FTSENT *parent = NULL;
        FTSENT *tmp = NULL;      
        bool defer_stat;

         
        if (options & ~FTS_OPTIONMASK) {
                __set_errno (EINVAL);
                return (NULL);
        }
        if ((options & FTS_NOCHDIR) && (options & FTS_CWDFD)) {
                __set_errno (EINVAL);
                return (NULL);
        }
        if ( ! (options & (FTS_LOGICAL | FTS_PHYSICAL))) {
                __set_errno (EINVAL);
                return (NULL);
        }

         
        sp = calloc (1, sizeof *sp);
        if (sp == NULL)
                return (NULL);
        sp->fts_compar = compar;
        sp->fts_options = options;

         
        if (ISSET(FTS_LOGICAL)) {
                SET(FTS_NOCHDIR);
                CLR(FTS_CWDFD);
        }

         
        sp->fts_cwd_fd = AT_FDCWD;
        if ( ISSET(FTS_CWDFD) && ! HAVE_OPENAT_SUPPORT)
          {
             
            int fd = open (".", O_SEARCH | O_CLOEXEC);
            if (fd < 0)
              {
                 
                if ( openat_needs_fchdir ())
                  {
                    SET(FTS_NOCHDIR);
                    CLR(FTS_CWDFD);
                  }
              }
            else
              {
                close (fd);
              }
          }

         
#ifndef MAXPATHLEN
# define MAXPATHLEN 1024
#endif
        {
          size_t maxarglen = fts_maxarglen(argv);
          if (! fts_palloc(sp, MAX(maxarglen, MAXPATHLEN)))
                  goto mem1;
        }

         
        if (*argv != NULL) {
                if ((parent = fts_alloc(sp, "", 0)) == NULL)
                        goto mem2;
                parent->fts_level = FTS_ROOTPARENTLEVEL;
          }

         
        defer_stat = (compar == NULL || ISSET(FTS_DEFER_STAT));

         
        for (root = NULL, nitems = 0; *argv != NULL; ++argv, ++nitems) {
                 
                size_t len = strlen(*argv);

                if ( ! (options & FTS_VERBATIM))
                  {
                     
                    char const *v = *argv;
                    if (2 < len && v[len - 1] == '/')
                      while (1 < len && v[len - 2] == '/')
                        --len;
                  }

                if ((p = fts_alloc(sp, *argv, len)) == NULL)
                        goto mem3;
                p->fts_level = FTS_ROOTLEVEL;
                p->fts_parent = parent;
                p->fts_accpath = p->fts_name;
                 
                if (defer_stat && root != NULL) {
                        p->fts_info = FTS_NSOK;
                        fts_set_stat_required(p, true);
                } else {
                        p->fts_info = fts_stat(sp, p, false);
                }

                 
                if (compar) {
                        p->fts_link = root;
                        root = p;
                } else {
                        p->fts_link = NULL;
                        if (root == NULL)
                                tmp = root = p;
                        else {
                                tmp->fts_link = p;
                                tmp = p;
                        }
                }
        }
        if (compar && nitems > 1)
                root = fts_sort(sp, root, nitems);

         
        if ((sp->fts_cur = fts_alloc(sp, "", 0)) == NULL)
                goto mem3;
        sp->fts_cur->fts_link = root;
        sp->fts_cur->fts_info = FTS_INIT;
        sp->fts_cur->fts_level = 1;
        if (! setup_dir (sp))
                goto mem3;

         
        if (!ISSET(FTS_NOCHDIR) && !ISSET(FTS_CWDFD)
            && (sp->fts_rfd = diropen (sp, ".")) < 0)
                SET(FTS_NOCHDIR);

        i_ring_init (&sp->fts_fd_ring, -1);
        return (sp);

mem3:   fts_lfree(root);
        free(parent);
mem2:   free(sp->fts_path);
mem1:   free(sp);
        return (NULL);
}

static void
internal_function
fts_load (FTS *sp, register FTSENT *p)
{
        register size_t len;
        register char *cp;

         
        len = p->fts_pathlen = p->fts_namelen;
        memmove(sp->fts_path, p->fts_name, len + 1);
        if ((cp = strrchr(p->fts_name, '/')) && (cp != p->fts_name || cp[1])) {
                len = strlen(++cp);
                memmove(p->fts_name, cp, len + 1);
                p->fts_namelen = len;
        }
        p->fts_accpath = p->fts_path = sp->fts_path;
}

int
fts_close (FTS *sp)
{
        register FTSENT *freep, *p;
        int saved_errno = 0;

         
        if (sp->fts_cur) {
                for (p = sp->fts_cur; p->fts_level >= FTS_ROOTLEVEL;) {
                        freep = p;
                        p = p->fts_link != NULL ? p->fts_link : p->fts_parent;
                        free(freep);
                }
                free(p);
        }

         
        if (sp->fts_child)
                fts_lfree(sp->fts_child);
        free(sp->fts_array);
        free(sp->fts_path);

        if (ISSET(FTS_CWDFD))
          {
            if (0 <= sp->fts_cwd_fd)
              if (close (sp->fts_cwd_fd))
                saved_errno = errno;
          }
        else if (!ISSET(FTS_NOCHDIR))
          {
             
            if (fchdir(sp->fts_rfd))
              saved_errno = errno;

             
            if (close (sp->fts_rfd))
              if (saved_errno == 0)
                saved_errno = errno;
          }

        fd_ring_clear (&sp->fts_fd_ring);

        if (sp->fts_leaf_optimization_works_ht)
          hash_free (sp->fts_leaf_optimization_works_ht);

        free_dir (sp);

         
        free(sp);

         
        if (saved_errno) {
                __set_errno (saved_errno);
                return (-1);
        }

        return (0);
}

 
enum { MIN_DIR_NLINK = 2 };

 
enum leaf_optimization
  {
     
    NO_LEAF_OPTIMIZATION,

     
    OK_LEAF_OPTIMIZATION
  };

#if (defined __linux__ || defined __ANDROID__) \
  && HAVE_SYS_VFS_H && HAVE_FSTATFS && HAVE_STRUCT_STATFS_F_TYPE

# include <sys/vfs.h>

 
# define S_MAGIC_AFS 0x5346414F
# define S_MAGIC_CIFS 0xFF534D42
# define S_MAGIC_NFS 0x6969
# define S_MAGIC_PROC 0x9FA0
# define S_MAGIC_TMPFS 0x1021994

# ifdef HAVE___FSWORD_T
typedef __fsword_t fsword;
# else
typedef long int fsword;
# endif

 
struct dev_type
{
  dev_t st_dev;
  fsword f_type;
};

 
enum { DEV_TYPE_HT_INITIAL_SIZE = 13 };

static size_t
dev_type_hash (void const *x, size_t table_size)
{
  struct dev_type const *ax = x;
  uintmax_t dev = ax->st_dev;
  return dev % table_size;
}

static bool
dev_type_compare (void const *x, void const *y)
{
  struct dev_type const *ax = x;
  struct dev_type const *ay = y;
  return ax->st_dev == ay->st_dev;
}

 

static fsword
filesystem_type (FTSENT const *p, int fd)
{
  FTS *sp = p->fts_fts;
  Hash_table *h = sp->fts_leaf_optimization_works_ht;
  struct dev_type *ent;
  struct statfs fs_buf;

   
  if (!ISSET (FTS_CWDFD))
    return 0;

  if (! h)
    h = sp->fts_leaf_optimization_works_ht
      = hash_initialize (DEV_TYPE_HT_INITIAL_SIZE, NULL, dev_type_hash,
                         dev_type_compare, free);
  if (h)
    {
      struct dev_type tmp;
      tmp.st_dev = p->fts_statp->st_dev;
      ent = hash_lookup (h, &tmp);
      if (ent)
        return ent->f_type;
    }

   
  if (fd < 0 || fstatfs (fd, &fs_buf) != 0)
    return 0;

  if (h)
    {
      struct dev_type *t2 = malloc (sizeof *t2);
      if (t2)
        {
          t2->st_dev = p->fts_statp->st_dev;
          t2->f_type = fs_buf.f_type;

          ent = hash_insert (h, t2);
          if (ent)
            fts_assert (ent == t2);
          else
            free (t2);
        }
    }

  return fs_buf.f_type;
}

 
static bool
dirent_inode_sort_may_be_useful (FTSENT const *p, int dir_fd)
{
   

  switch (filesystem_type (p, dir_fd))
    {
    case S_MAGIC_CIFS:
    case S_MAGIC_NFS:
    case S_MAGIC_TMPFS:
       
      return false;

    default:
      return true;
    }
}

 
static enum leaf_optimization
leaf_optimization (FTSENT const *p, int dir_fd)
{
  switch (filesystem_type (p, dir_fd))
    {
    case 0:
       
      FALLTHROUGH;
    case S_MAGIC_AFS:
       
      FALLTHROUGH;
    case S_MAGIC_CIFS:
       
      FALLTHROUGH;
    case S_MAGIC_NFS:
       
      FALLTHROUGH;
    case S_MAGIC_PROC:
       
      return NO_LEAF_OPTIMIZATION;

    default:
      return OK_LEAF_OPTIMIZATION;
    }
}

#else
static bool
dirent_inode_sort_may_be_useful (_GL_UNUSED FTSENT const *p,
                                 _GL_UNUSED int dir_fd)
{
  return true;
}
static enum leaf_optimization
leaf_optimization (_GL_UNUSED FTSENT const *p, _GL_UNUSED int dir_fd)
{
  return NO_LEAF_OPTIMIZATION;
}
#endif

 
#define NAPPEND(p)                                                      \
        (p->fts_path[p->fts_pathlen - 1] == '/'                         \
            ? p->fts_pathlen - 1 : p->fts_pathlen)

FTSENT *
fts_read (register FTS *sp)
{
        register FTSENT *p, *tmp;
        register unsigned short int instr;
        register char *t;

         
        if (sp->fts_cur == NULL || ISSET(FTS_STOP))
                return (NULL);

         
        p = sp->fts_cur;

         
        instr = p->fts_instr;
        p->fts_instr = FTS_NOINSTR;

         
        if (instr == FTS_AGAIN) {
                p->fts_info = fts_stat(sp, p, false);
                return (p);
        }
        Dprintf (("fts_read: p=%s\n",
                  p->fts_info == FTS_INIT ? "" : p->fts_path));

         
        if (instr == FTS_FOLLOW &&
            (p->fts_info == FTS_SL || p->fts_info == FTS_SLNONE)) {
                p->fts_info = fts_stat(sp, p, true);
                if (p->fts_info == FTS_D && !ISSET(FTS_NOCHDIR)) {
                        if ((p->fts_symfd = diropen (sp, ".")) < 0) {
                                p->fts_errno = errno;
                                p->fts_info = FTS_ERR;
                        } else
                                p->fts_flags |= FTS_SYMFOLLOW;
                }
                goto check_for_dir;
        }

         
        if (p->fts_info == FTS_D) {
                 
                if (instr == FTS_SKIP ||
                    (ISSET(FTS_XDEV) && p->fts_statp->st_dev != sp->fts_dev)) {
                        if (p->fts_flags & FTS_SYMFOLLOW)
                                (void)close(p->fts_symfd);
                        if (sp->fts_child) {
                                fts_lfree(sp->fts_child);
                                sp->fts_child = NULL;
                        }
                        p->fts_info = FTS_DP;
                        LEAVE_DIR (sp, p, "1");
                        return (p);
                }

                 
                if (sp->fts_child != NULL && ISSET(FTS_NAMEONLY)) {
                        CLR(FTS_NAMEONLY);
                        fts_lfree(sp->fts_child);
                        sp->fts_child = NULL;
                }

                 
                if (sp->fts_child != NULL) {
                        if (fts_safe_changedir(sp, p, -1, p->fts_accpath)) {
                                p->fts_errno = errno;
                                p->fts_flags |= FTS_DONTCHDIR;
                                for (p = sp->fts_child; p != NULL;
                                     p = p->fts_link)
                                        p->fts_accpath =
                                            p->fts_parent->fts_accpath;
                        }
                } else if ((sp->fts_child = fts_build(sp, BREAD)) == NULL) {
                        if (ISSET(FTS_STOP))
                                return (NULL);
                         
                        if (p->fts_errno && p->fts_info != FTS_DNR)
                                p->fts_info = FTS_ERR;
                        LEAVE_DIR (sp, p, "2");
                        return (p);
                }
                p = sp->fts_child;
                sp->fts_child = NULL;
                goto name;
        }

         
next:   tmp = p;

         
        if (p->fts_link == NULL && p->fts_parent->fts_dirp)
          {
            p = tmp->fts_parent;
            sp->fts_cur = p;
            sp->fts_path[p->fts_pathlen] = '\0';

            if ((p = fts_build (sp, BREAD)) == NULL)
              {
                if (ISSET(FTS_STOP))
                  return NULL;
                goto cd_dot_dot;
              }

            free(tmp);
            goto name;
          }

        if ((p = p->fts_link) != NULL) {
                sp->fts_cur = p;
                free(tmp);

                 
                if (p->fts_level == FTS_ROOTLEVEL) {
                        if (restore_initial_cwd(sp)) {
                                SET(FTS_STOP);
                                return (NULL);
                        }
                        free_dir(sp);
                        fts_load(sp, p);
                        if (! setup_dir(sp)) {
                                free_dir(sp);
                                return (NULL);
                        }
                        goto check_for_dir;
                }

                 
                if (p->fts_instr == FTS_SKIP)
                        goto next;
                if (p->fts_instr == FTS_FOLLOW) {
                        p->fts_info = fts_stat(sp, p, true);
                        if (p->fts_info == FTS_D && !ISSET(FTS_NOCHDIR)) {
                                if ((p->fts_symfd = diropen (sp, ".")) < 0) {
                                        p->fts_errno = errno;
                                        p->fts_info = FTS_ERR;
                                } else
                                        p->fts_flags |= FTS_SYMFOLLOW;
                        }
                        p->fts_instr = FTS_NOINSTR;
                }

name:           t = sp->fts_path + NAPPEND(p->fts_parent);
                *t++ = '/';
                memmove(t, p->fts_name, p->fts_namelen + 1);
check_for_dir:
                sp->fts_cur = p;
                if (p->fts_info == FTS_NSOK)
                  {
                    if (p->fts_statp->st_size == FTS_STAT_REQUIRED)
                      p->fts_info = fts_stat(sp, p, false);
                    else
                      fts_assert (p->fts_statp->st_size == FTS_NO_STAT_REQUIRED);
                  }

                if (p->fts_info == FTS_D)
                  {
                     
                    if (p->fts_level == FTS_ROOTLEVEL)
                      sp->fts_dev = p->fts_statp->st_dev;
                    Dprintf (("  entering: %s\n", p->fts_path));
                    if (! enter_dir (sp, p))
                      return NULL;
                  }
                return p;
        }
cd_dot_dot:

         
        p = tmp->fts_parent;
        sp->fts_cur = p;
        free(tmp);

        if (p->fts_level == FTS_ROOTPARENTLEVEL) {
                 
                free(p);
                __set_errno (0);
                return (sp->fts_cur = NULL);
        }

        fts_assert (p->fts_info != FTS_NSOK);

         
        sp->fts_path[p->fts_pathlen] = '\0';

         
        if (p->fts_level == FTS_ROOTLEVEL) {
                if (restore_initial_cwd(sp)) {
                        p->fts_errno = errno;
                        SET(FTS_STOP);
                }
        } else if (p->fts_flags & FTS_SYMFOLLOW) {
                if (FCHDIR(sp, p->fts_symfd)) {
                        p->fts_errno = errno;
                        SET(FTS_STOP);
                }
                (void)close(p->fts_symfd);
        } else if (!(p->fts_flags & FTS_DONTCHDIR) &&
                   fts_safe_changedir(sp, p->fts_parent, -1, "..")) {
                p->fts_errno = errno;
                SET(FTS_STOP);
        }

         
        if (p->fts_info != FTS_DC) {
                p->fts_info = p->fts_errno ? FTS_ERR : FTS_DP;
                if (p->fts_errno == 0)
                        LEAVE_DIR (sp, p, "3");
        }
        return ISSET(FTS_STOP) ? NULL : p;
}

 
 
int
fts_set(_GL_UNUSED FTS *sp, FTSENT *p, int instr)
{
        if (instr != 0 && instr != FTS_AGAIN && instr != FTS_FOLLOW &&
            instr != FTS_NOINSTR && instr != FTS_SKIP) {
                __set_errno (EINVAL);
                return (1);
        }
        p->fts_instr = instr;
        return (0);
}

FTSENT *
fts_children (register FTS *sp, int instr)
{
        register FTSENT *p;
        int fd;

        if (instr != 0 && instr != FTS_NAMEONLY) {
                __set_errno (EINVAL);
                return (NULL);
        }

         
        p = sp->fts_cur;

         
        __set_errno (0);

         
        if (ISSET(FTS_STOP))
                return (NULL);

         
        if (p->fts_info == FTS_INIT)
                return (p->fts_link);

         
        if (p->fts_info != FTS_D  )
                return (NULL);

         
        if (sp->fts_child != NULL)
                fts_lfree(sp->fts_child);

        if (instr == FTS_NAMEONLY) {
                SET(FTS_NAMEONLY);
                instr = BNAMES;
        } else
                instr = BCHILD;

         
        if (p->fts_level != FTS_ROOTLEVEL || p->fts_accpath[0] == '/' ||
            ISSET(FTS_NOCHDIR))
                return (sp->fts_child = fts_build(sp, instr));

        if ((fd = diropen (sp, ".")) < 0)
                return (sp->fts_child = NULL);
        sp->fts_child = fts_build(sp, instr);
        if (ISSET(FTS_CWDFD))
          {
            cwd_advance_fd (sp, fd, true);
          }
        else
          {
            if (fchdir(fd))
              {
                int saved_errno = errno;
                close (fd);
                __set_errno (saved_errno);
                return NULL;
              }
            close (fd);
          }
        return (sp->fts_child);
}

 
static int
fts_compare_ino (struct _ftsent const **a, struct _ftsent const **b)
{
  return _GL_CMP (a[0]->fts_statp->st_ino, b[0]->fts_statp->st_ino);
}

 
static void
set_stat_type (struct stat *st, unsigned int dtype)
{
  mode_t type;
  switch (dtype)
    {
    case DT_BLK:
      type = S_IFBLK;
      break;
    case DT_CHR:
      type = S_IFCHR;
      break;
    case DT_DIR:
      type = S_IFDIR;
      break;
    case DT_FIFO:
      type = S_IFIFO;
      break;
    case DT_LNK:
      type = S_IFLNK;
      break;
    case DT_REG:
      type = S_IFREG;
      break;
    case DT_SOCK:
      type = S_IFSOCK;
      break;
    default:
      type = 0;
    }
  st->st_mode = type;
}

#define closedir_and_clear(dirp)                \
  do                                            \
    {                                           \
      closedir (dirp);                          \
      dirp = NULL;                              \
    }                                           \
  while (0)

#define fts_opendir(file, Pdir_fd)                              \
        opendirat((! ISSET(FTS_NOCHDIR) && ISSET(FTS_CWDFD)     \
                   ? sp->fts_cwd_fd : AT_FDCWD),                \
                  file,                                         \
                  (((ISSET(FTS_PHYSICAL)                        \
                     && ! (ISSET(FTS_COMFOLLOW)                 \
                           && cur->fts_level == FTS_ROOTLEVEL)) \
                    ? O_NOFOLLOW : 0)),                         \
                  Pdir_fd)

 
static FTSENT *
internal_function
fts_build (register FTS *sp, int type)
{
        register FTSENT *p, *head;
        register size_t nitems;
        FTSENT *tail;
        int saved_errno;
        bool descend;
        bool doadjust;
        ptrdiff_t level;
        size_t len, maxlen, new_len;
        char *cp;
        int dir_fd;
        FTSENT *cur = sp->fts_cur;
        bool continue_readdir = !!cur->fts_dirp;
        bool sort_by_inode = false;
        size_t max_entries;

         
        if (continue_readdir)
          {
            DIR *dp = cur->fts_dirp;
            dir_fd = dirfd (dp);
            if (dir_fd < 0)
              {
                int dirfd_errno = errno;
                closedir_and_clear (cur->fts_dirp);
                if (type == BREAD)
                  {
                    cur->fts_info = FTS_DNR;
                    cur->fts_errno = dirfd_errno;
                  }
                return NULL;
              }
          }
        else
          {
             
            if ((cur->fts_dirp = fts_opendir(cur->fts_accpath, &dir_fd)) == NULL)
              {
                if (type == BREAD)
                  {
                    cur->fts_info = FTS_DNR;
                    cur->fts_errno = errno;
                  }
                return NULL;
              }
             
            bool stat_optimization = cur->fts_info == FTS_NSOK;

            if (stat_optimization
                 
                || ISSET (FTS_TIGHT_CYCLE_CHECK))
              {
                if (!stat_optimization)
                  LEAVE_DIR (sp, cur, "4");
                if (fstat (dir_fd, cur->fts_statp) != 0)
                  {
                    int fstat_errno = errno;
                    closedir_and_clear (cur->fts_dirp);
                    if (type == BREAD)
                      {
                        cur->fts_errno = fstat_errno;
                        cur->fts_info = FTS_NS;
                      }
                    __set_errno (fstat_errno);
                    return NULL;
                  }
                if (stat_optimization)
                  cur->fts_info = FTS_D;
                else if (! enter_dir (sp, cur))
                  {
                    int err = errno;
                    closedir_and_clear (cur->fts_dirp);
                    __set_errno (err);
                    return NULL;
                  }
              }
          }

         
        max_entries = sp->fts_compar ? SIZE_MAX : FTS_MAX_READDIR_ENTRIES;

         
        if (continue_readdir)
          {
             
            descend = true;
          }
        else
          {
             
            descend = (type != BNAMES
                       && ! (ISSET (FTS_NOSTAT) && ISSET (FTS_PHYSICAL)
                             && ! ISSET (FTS_SEEDOT)
                             && cur->fts_statp->st_nlink == MIN_DIR_NLINK
                             && (leaf_optimization (cur, dir_fd)
                                 != NO_LEAF_OPTIMIZATION)));
            if (descend || type == BREAD)
              {
                if (ISSET(FTS_CWDFD))
                  dir_fd = fcntl (dir_fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
                if (dir_fd < 0 || fts_safe_changedir(sp, cur, dir_fd, NULL)) {
                        if (descend && type == BREAD)
                                cur->fts_errno = errno;
                        cur->fts_flags |= FTS_DONTCHDIR;
                        descend = false;
                        closedir_and_clear(cur->fts_dirp);
                        if (ISSET(FTS_CWDFD) && 0 <= dir_fd)
                                close (dir_fd);
                        cur->fts_dirp = NULL;
                } else
                        descend = true;
              }
          }

         
        len = NAPPEND(cur);
        if (ISSET(FTS_NOCHDIR)) {
                cp = sp->fts_path + len;
                *cp++ = '/';
        } else {
                 
                cp = NULL;
        }
        len++;
        maxlen = sp->fts_pathlen - len;

        level = cur->fts_level + 1;

         
        doadjust = false;
        head = NULL;
        tail = NULL;
        nitems = 0;
        while (cur->fts_dirp) {
                size_t d_namelen;
                __set_errno (0);
                struct dirent *dp = readdir(cur->fts_dirp);
                if (dp == NULL) {
                        if (errno) {
                                cur->fts_errno = errno;
                                 
                                cur->fts_info = (continue_readdir || nitems)
                                                ? FTS_ERR : FTS_DNR;
                        }
                        closedir_and_clear(cur->fts_dirp);
                        break;
                }
                if (!ISSET(FTS_SEEDOT) && ISDOT(dp->d_name))
                        continue;

                d_namelen = _D_EXACT_NAMLEN (dp);
                p = fts_alloc (sp, dp->d_name, d_namelen);
                if (!p)
                        goto mem1;
                if (d_namelen >= maxlen) {
                         
                        uintptr_t oldaddr = (uintptr_t) sp->fts_path;
                        if (! fts_palloc(sp, d_namelen + len + 1)) {
                                 
mem1:                           saved_errno = errno;
                                free(p);
                                fts_lfree(head);
                                closedir_and_clear(cur->fts_dirp);
                                cur->fts_info = FTS_ERR;
                                SET(FTS_STOP);
                                __set_errno (saved_errno);
                                return (NULL);
                        }
                         
                        if (oldaddr != (uintptr_t) sp->fts_path) {
                                doadjust = true;
                                if (ISSET(FTS_NOCHDIR))
                                        cp = sp->fts_path + len;
                        }
                        maxlen = sp->fts_pathlen - len;
                }

                new_len = len + d_namelen;
                if (new_len < len) {
                         
                        free(p);
                        fts_lfree(head);
                        closedir_and_clear(cur->fts_dirp);
                        cur->fts_info = FTS_ERR;
                        SET(FTS_STOP);
                        __set_errno (ENAMETOOLONG);
                        return (NULL);
                }
                p->fts_level = level;
                p->fts_parent = sp->fts_cur;
                p->fts_pathlen = new_len;

                 
                p->fts_statp->st_ino = D_INO (dp);

                 
                if (ISSET(FTS_NOCHDIR)) {
                        p->fts_accpath = p->fts_path;
                        memmove(cp, p->fts_name, p->fts_namelen + 1);
                } else
                        p->fts_accpath = p->fts_name;

                if (sp->fts_compar == NULL || ISSET(FTS_DEFER_STAT)) {
                         
                        bool skip_stat = (ISSET(FTS_NOSTAT)
                                          && DT_IS_KNOWN(dp)
                                          && ! DT_MUST_BE(dp, DT_DIR)
                                          && (ISSET(FTS_PHYSICAL)
                                              || ! DT_MUST_BE(dp, DT_LNK)));
                        p->fts_info = FTS_NSOK;
                         
                        set_stat_type (p->fts_statp, D_TYPE (dp));
                        fts_set_stat_required(p, !skip_stat);
                } else {
                        p->fts_info = fts_stat(sp, p, false);
                }

                 
                p->fts_link = NULL;
                if (head == NULL)
                        head = tail = p;
                else {
                        tail->fts_link = p;
                        tail = p;
                }

                 
                if (nitems == _FTS_INODE_SORT_DIR_ENTRIES_THRESHOLD
                    && !sp->fts_compar)
                  sort_by_inode = dirent_inode_sort_may_be_useful (cur, dir_fd);

                ++nitems;
                if (max_entries <= nitems) {
                         
                        break;
                }
        }

         
        if (doadjust)
                fts_padjust(sp, head);

         
        if (ISSET(FTS_NOCHDIR)) {
                if (len == sp->fts_pathlen || nitems == 0)
                        --cp;
                *cp = '\0';
        }

         
        if (!continue_readdir && descend && (type == BCHILD || !nitems) &&
            (cur->fts_level == FTS_ROOTLEVEL
             ? restore_initial_cwd(sp)
             : fts_safe_changedir(sp, cur->fts_parent, -1, ".."))) {
                cur->fts_info = FTS_ERR;
                SET(FTS_STOP);
                fts_lfree(head);
                return (NULL);
        }

         
        if (!nitems) {
                if (type == BREAD
                    && cur->fts_info != FTS_DNR && cur->fts_info != FTS_ERR)
                        cur->fts_info = FTS_DP;
                fts_lfree(head);
                return (NULL);
        }

        if (sort_by_inode) {
                sp->fts_compar = fts_compare_ino;
                head = fts_sort (sp, head, nitems);
                sp->fts_compar = NULL;
        }

         
        if (sp->fts_compar && nitems > 1)
                head = fts_sort(sp, head, nitems);
        return (head);
}

#if GNULIB_FTS_DEBUG

struct devino {
  intmax_t dev, ino;
};
#define PRINT_DEVINO "(%jd,%jd)"

static struct devino
getdevino (int fd)
{
  struct stat st;
  return (fd == AT_FDCWD
          ? (struct devino) { -1, 0 }
          : fstat (fd, &st) == 0
          ? (struct devino) { st.st_dev, st.st_ino }
          : (struct devino) { -1, errno });
}

 
static void
find_matching_ancestor (FTSENT const *e_curr, struct Active_dir const *ad)
{
  FTSENT const *ent;
  for (ent = e_curr; ent->fts_level >= FTS_ROOTLEVEL; ent = ent->fts_parent)
    {
      if (ad->ino == ent->fts_statp->st_ino
          && ad->dev == ent->fts_statp->st_dev)
        return;
    }
  printf ("ERROR: tree dir, %s, not active\n", ad->fts_ent->fts_accpath);
  printf ("active dirs:\n");
  for (ent = e_curr;
       ent->fts_level >= FTS_ROOTLEVEL; ent = ent->fts_parent)
    printf ("  %s(%"PRIuMAX"/%"PRIuMAX") to %s(%"PRIuMAX"/%"PRIuMAX")...\n",
            ad->fts_ent->fts_accpath,
            (uintmax_t) ad->dev,
            (uintmax_t) ad->ino,
            ent->fts_accpath,
            (uintmax_t) ent->fts_statp->st_dev,
            (uintmax_t) ent->fts_statp->st_ino);
}

void
fts_cross_check (FTS const *sp)
{
  FTSENT const *ent = sp->fts_cur;
  FTSENT const *t;
  if ( ! ISSET (FTS_TIGHT_CYCLE_CHECK))
    return;

  Dprintf (("fts-cross-check cur=%s\n", ent->fts_path));
   
  for (t = ent->fts_parent; t->fts_level >= FTS_ROOTLEVEL; t = t->fts_parent)
    {
      struct Active_dir ad;
      ad.ino = t->fts_statp->st_ino;
      ad.dev = t->fts_statp->st_dev;
      if ( ! hash_lookup (sp->fts_cycle.ht, &ad))
        printf ("ERROR: active dir, %s, not in tree\n", t->fts_path);
    }

   
  if (ent->fts_parent->fts_level >= FTS_ROOTLEVEL
      && (ent->fts_info == FTS_DP
          || ent->fts_info == FTS_D))
    {
      struct Active_dir *ad;
      for (ad = hash_get_first (sp->fts_cycle.ht); ad != NULL;
           ad = hash_get_next (sp->fts_cycle.ht, ad))
        {
          find_matching_ancestor (ent, ad);
        }
    }
}

static bool
same_fd (int fd1, int fd2)
{
  struct stat sb1, sb2;
  return (fstat (fd1, &sb1) == 0
          && fstat (fd2, &sb2) == 0
          && SAME_INODE (sb1, sb2));
}

static void
fd_ring_print (FTS const *sp, FILE *stream, char const *msg)
{
  if (!fts_debug)
    return;
  I_ring const *fd_ring = &sp->fts_fd_ring;
  unsigned int i = fd_ring->ir_front;
  struct devino cwd = getdevino (sp->fts_cwd_fd);
  fprintf (stream, "=== %s ========== "PRINT_DEVINO"\n", msg, cwd.dev, cwd.ino);
  if (i_ring_empty (fd_ring))
    return;

  while (true)
    {
      int fd = fd_ring->ir_data[i];
      if (fd < 0)
        fprintf (stream, "%u: %d:\n", i, fd);
      else
        {
          struct devino wd = getdevino (fd);
          fprintf (stream, "%u: %d: "PRINT_DEVINO"\n", i, fd, wd.dev, wd.ino);
        }
      if (i == fd_ring->ir_back)
        break;
      i = (i + I_RING_SIZE - 1) % I_RING_SIZE;
    }
}

 
static void
fd_ring_check (FTS const *sp)
{
  if (!fts_debug)
    return;

   
  I_ring fd_w = sp->fts_fd_ring;

  int cwd_fd = sp->fts_cwd_fd;
  cwd_fd = fcntl (cwd_fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
  struct devino dot = getdevino (cwd_fd);
  fprintf (stderr, "===== check ===== cwd: "PRINT_DEVINO"\n",
           dot.dev, dot.ino);
  while ( ! i_ring_empty (&fd_w))
    {
      int fd = i_ring_pop (&fd_w);
      if (0 <= fd)
        {
          int open_flags = O_SEARCH | O_CLOEXEC;
          int parent_fd = openat (cwd_fd, "..", open_flags);
          if (parent_fd < 0)
            {
               
              break;
            }
          if (!same_fd (fd, parent_fd))
            {
              struct devino cwd = getdevino (fd);
              fprintf (stderr, "ring  : "PRINT_DEVINO"\n", cwd.dev, cwd.ino);
              struct devino c2 = getdevino (parent_fd);
              fprintf (stderr, "parent: "PRINT_DEVINO"\n", c2.dev, c2.ino);
              fts_assert (0);
            }
          close (cwd_fd);
          cwd_fd = parent_fd;
        }
    }
  close (cwd_fd);
}
#endif

static unsigned short int
internal_function
fts_stat(FTS *sp, register FTSENT *p, bool follow)
{
        struct stat *sbp = p->fts_statp;

        if (ISSET (FTS_LOGICAL)
            || (ISSET (FTS_COMFOLLOW) && p->fts_level == FTS_ROOTLEVEL))
                follow = true;

         
        int flags = follow ? 0 : AT_SYMLINK_NOFOLLOW;
        if (fstatat (sp->fts_cwd_fd, p->fts_accpath, sbp, flags) < 0)
          {
            if (follow && errno == ENOENT
                && 0 <= fstatat (sp->fts_cwd_fd, p->fts_accpath, sbp,
                                 AT_SYMLINK_NOFOLLOW))
              {
                __set_errno (0);
                return FTS_SLNONE;
              }

            p->fts_errno = errno;
            memset (sbp, 0, sizeof *sbp);
            return FTS_NS;
          }

        if (S_ISDIR(sbp->st_mode)) {
                if (ISDOT(p->fts_name)) {
                         
                        return (p->fts_level == FTS_ROOTLEVEL ? FTS_D : FTS_DOT);
                }

                return (FTS_D);
        }
        if (S_ISLNK(sbp->st_mode))
                return (FTS_SL);
        if (S_ISREG(sbp->st_mode))
                return (FTS_F);
        return (FTS_DEFAULT);
}

static int
fts_compar (void const *a, void const *b)
{
   
  FTSENT const **pa = (FTSENT const **) a;
  FTSENT const **pb = (FTSENT const **) b;
  return pa[0]->fts_fts->fts_compar (pa, pb);
}

static FTSENT *
internal_function
fts_sort (FTS *sp, FTSENT *head, register size_t nitems)
{
        register FTSENT **ap, *p;

         
        FTSENT *dummy;
        int (*compare) (void const *, void const *) =
          ((sizeof &dummy == sizeof (void *)
            && (long int) &dummy == (long int) (void *) &dummy)
           ? (int (*) (void const *, void const *)) sp->fts_compar
           : fts_compar);

         
        if (nitems > sp->fts_nitems) {
                FTSENT **a;

                sp->fts_nitems = nitems + 40;
                if (SIZE_MAX / sizeof *a < sp->fts_nitems
                    || ! (a = realloc (sp->fts_array,
                                       sp->fts_nitems * sizeof *a))) {
                        free(sp->fts_array);
                        sp->fts_array = NULL;
                        sp->fts_nitems = 0;
                        return (head);
                }
                sp->fts_array = a;
        }
        for (ap = sp->fts_array, p = head; p; p = p->fts_link)
                *ap++ = p;
        qsort((void *)sp->fts_array, nitems, sizeof(FTSENT *), compare);
        for (head = *(ap = sp->fts_array); --nitems; ++ap)
                ap[0]->fts_link = ap[1];
        ap[0]->fts_link = NULL;
        return (head);
}

static FTSENT *
internal_function
fts_alloc (FTS *sp, const char *name, register size_t namelen)
{
        register FTSENT *p;
        size_t len;

         
        len = FLEXSIZEOF(FTSENT, fts_name, namelen + 1);
        if ((p = malloc(len)) == NULL)
                return (NULL);

         
        memcpy(p->fts_name, name, namelen);
        p->fts_name[namelen] = '\0';

        p->fts_namelen = namelen;
        p->fts_fts = sp;
        p->fts_path = sp->fts_path;
        p->fts_errno = 0;
        p->fts_dirp = NULL;
        p->fts_flags = 0;
        p->fts_instr = FTS_NOINSTR;
        p->fts_number = 0;
        p->fts_pointer = NULL;
        return (p);
}

static void
internal_function
fts_lfree (register FTSENT *head)
{
        register FTSENT *p;
        int err = errno;

         
        while ((p = head)) {
                head = head->fts_link;
                if (p->fts_dirp)
                        closedir (p->fts_dirp);
                free(p);
        }

        __set_errno (err);
}

 
static bool
internal_function
fts_palloc (FTS *sp, size_t more)
{
        char *p;
        size_t new_len = sp->fts_pathlen + more + 256;

         
        if (new_len < sp->fts_pathlen) {
                free(sp->fts_path);
                sp->fts_path = NULL;
                __set_errno (ENAMETOOLONG);
                return false;
        }
        sp->fts_pathlen = new_len;
        p = realloc(sp->fts_path, sp->fts_pathlen);
        if (p == NULL) {
                free(sp->fts_path);
                sp->fts_path = NULL;
                return false;
        }
        sp->fts_path = p;
        return true;
}

 
static void
internal_function
fts_padjust (FTS *sp, FTSENT *head)
{
        FTSENT *p;
        char *addr = sp->fts_path;

         

#define ADJUST(p) do {                                                  \
        uintptr_t old_accpath = (uintptr_t) (p)->fts_accpath;           \
        if (old_accpath != (uintptr_t) (p)->fts_name) {                 \
                (p)->fts_accpath =                                      \
                  addr + (old_accpath - (uintptr_t) (p)->fts_path);     \
        }                                                               \
        (p)->fts_path = addr;                                           \
} while (0)
         
        for (p = sp->fts_child; p; p = p->fts_link)
                ADJUST(p);

         
        for (p = head; p->fts_level >= FTS_ROOTLEVEL;) {
                ADJUST(p);
                p = p->fts_link ? p->fts_link : p->fts_parent;
        }
}

static size_t
internal_function _GL_ATTRIBUTE_PURE
fts_maxarglen (char * const *argv)
{
        size_t len, max;

        for (max = 0; *argv; ++argv)
                if ((len = strlen(*argv)) > max)
                        max = len;
        return (max + 1);
}

 
static int
internal_function
fts_safe_changedir (FTS *sp, FTSENT *p, int fd, char const *dir)
{
        int ret;
        bool is_dotdot = dir && STREQ (dir, "..");
        int newfd;

         
        if (ISSET (FTS_NOCHDIR))
          {
            if (ISSET (FTS_CWDFD) && 0 <= fd)
              close (fd);
            return 0;
          }

        if (fd < 0 && is_dotdot && ISSET (FTS_CWDFD))
          {
             
            if ( ! i_ring_empty (&sp->fts_fd_ring))
              {
                int parent_fd;
                fd_ring_print (sp, stderr, "pre-pop");
                parent_fd = i_ring_pop (&sp->fts_fd_ring);
                if (0 <= parent_fd)
                  {
                    fd = parent_fd;
                    dir = NULL;
                  }
              }
          }

        newfd = fd;
        if (fd < 0 && (newfd = diropen (sp, dir)) < 0)
          return -1;

         
        if (ISSET(FTS_LOGICAL) || ! HAVE_WORKING_O_NOFOLLOW
            || (dir && STREQ (dir, "..")))
          {
            struct stat sb;
            if (fstat(newfd, &sb))
              {
                ret = -1;
                goto bail;
              }
            if (p->fts_statp->st_dev != sb.st_dev
                || p->fts_statp->st_ino != sb.st_ino)
              {
                __set_errno (ENOENT);            
                ret = -1;
                goto bail;
              }
          }

        if (ISSET(FTS_CWDFD))
          {
            cwd_advance_fd (sp, newfd, ! is_dotdot);
            return 0;
          }

        ret = fchdir(newfd);
bail:
        if (fd < 0)
          {
            int oerrno = errno;
            (void)close(newfd);
            __set_errno (oerrno);
          }
        return ret;
}
